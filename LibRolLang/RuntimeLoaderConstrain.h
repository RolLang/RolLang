#pragma once
#include "RuntimeLoaderRefList.h"

struct RuntimeLoaderConstrain : RuntimeLoaderRefList
{
public:
	bool CheckConstrainsImpl(const std::string& srcAssebly, GenericDeclaration* g,
		const std::vector<RuntimeType*>& args)
	{
		std::vector<ConstrainType> cargs;
		for (auto a : args)
		{
			cargs.push_back(ConstrainType::RT(a));
		}
		for (auto& constrain : g->Constrains)
		{
			ConstrainCalculationCacheRoot root;
			auto c = CreateConstrainCache(constrain, srcAssebly, cargs, ConstrainType::Fail());
			c->Root = &root;
			if (!CheckConstrainCached(c.get()))
			{
				return false;
			}
		}
		return true;
	}

private:
	enum ConstrainTypeType
	{
		CTT_FAIL,
		CTT_ANY,
		CTT_GENERIC,
		CTT_SUBTYPE,
		CTT_RT,
	};

	struct ConstrainUndeterminedTypeSource;
	struct ConstrainUndeterminedTypeRef
	{
		ConstrainUndeterminedTypeSource* Parent;
		std::size_t Index;
	};
	struct ConstrainUndeterminedTypeInfo
	{
		RuntimeType* Determined;
	};
	struct ConstrainUndeterminedTypeSource
	{
		ConstrainUndeterminedTypeSource* SParent = nullptr;
		std::vector<ConstrainUndeterminedTypeInfo> UndeterminedTypes;
		std::size_t TotalSize = 0;
		RuntimeType* GetDetermined(std::size_t i)
		{
			auto ps = SParent ? SParent->TotalSize : 0;
			if (i >= ps)
			{
				return UndeterminedTypes[i - ps].Determined;
			}
			return SParent->GetDetermined(i);
		}
		void Determined(std::size_t i, RuntimeType* t)
		{
			auto ps = SParent ? SParent->TotalSize : 0;
			if (i >= ps)
			{
				UndeterminedTypes[i - ps].Determined = t;
			}
			else
			{
				SParent->Determined(i, t);
			}
		}
	};
	struct ConstrainType
	{
		ConstrainTypeType CType;
		RuntimeType* Determined;
		std::string TypeTemplateAssembly;
		std::size_t TypeTemplateIndex;
		std::string SubtypeName;
		std::vector<ConstrainType> Args;
		ConstrainUndeterminedTypeRef Undetermined;
		bool TryArgumentConstrain;

		bool ContainsUndetermined()
		{
			switch (CType)
			{
			case CTT_RT: return false;
			case CTT_GENERIC:
			case CTT_SUBTYPE:
				for (auto& a : Args)
				{
					if (a.ContainsUndetermined()) return true;
				}
				return false;
			case CTT_ANY:
				return !Undetermined.Parent->GetDetermined(Undetermined.Index);
			default:
				assert(0);
				return false;
			}
		}

		static ConstrainType Fail()
		{
			return { CTT_FAIL };
		}

		static ConstrainType RT(RuntimeType* rt)
		{
			return { CTT_RT, rt };
		}

		static ConstrainType UD(ConstrainUndeterminedTypeSource& src)
		{
			auto id = src.UndeterminedTypes.size() + (src.SParent ? src.SParent->TotalSize : 0);
			src.UndeterminedTypes.push_back({});
			return { CTT_ANY, nullptr, {}, 0, {}, {}, { &src, id } };
		}

		static ConstrainType G(const std::string& a, std::size_t i)
		{
			return { CTT_GENERIC, nullptr, a, i };
		}

		static ConstrainType SUB(const std::string& n)
		{
			return { CTT_SUBTYPE, nullptr, {}, {}, n };
		}

		static ConstrainType Try(ConstrainType&& t)
		{
			auto ret = ConstrainType(std::move(t));
			ret.TryArgumentConstrain = true;
			return ret;
		}
	};
	struct ConstrainCalculationCacheRoot;
	struct ConstrainCalculationCache : ConstrainUndeterminedTypeSource
	{
		ConstrainCalculationCacheRoot* Root;
		ConstrainCalculationCache* Parent;

		GenericConstrain* Source;
		std::vector<ConstrainType> CheckArguments;
		ConstrainType CheckTarget;

		std::string SrcAssembly;
		ConstrainType Target;
		std::vector<ConstrainType> Arguments;
		std::vector<std::unique_ptr<ConstrainCalculationCache>> Children;
	};
	struct ConstrainCalculationCacheRoot
	{
		std::size_t Size;
	};

private:
	void CreateSubConstrainCache(const std::string& srcAssebly, GenericDeclaration* g,
		ConstrainCalculationCache& parent)
	{
		for (auto& constrain : g->Constrains)
		{
			parent.Children.emplace_back(CreateConstrainCache(constrain, srcAssebly,
				parent.Arguments, parent.Target));
			auto ptr = parent.Children.back().get();
			ptr->Parent = &parent;
			ptr->Root = parent.Root;
			ptr->SParent = &parent;
		}
	}

	//TODO separate create+basic fields from load argument/target types (reduce # of args)
	std::unique_ptr<ConstrainCalculationCache> CreateConstrainCache(GenericConstrain& constrain,
		const std::string& srcAssebly, const std::vector<ConstrainType>& args, ConstrainType checkTarget)
	{
		auto ret = std::make_unique<ConstrainCalculationCache>();
		ret->Source = &constrain;
		ret->SrcAssembly = srcAssebly;
		ret->CheckArguments = args;
		ret->CheckTarget = checkTarget;
		ret->Target = ConstructConstrainArgumentType(*ret.get(), constrain, constrain.Target);
		for (auto a : constrain.Arguments)
		{
			ret->Arguments.push_back(ConstructConstrainArgumentType(*ret.get(), constrain, a));
		}
		return ret;
	}

	bool CheckConstrainCached(ConstrainCalculationCache* cache)
	{
		while (ListContainUndetermined(cache->Arguments, cache->Target))
		{
			auto check = TryDetermineConstrainArgument(*cache);
			if (check == 1) continue;
			return false;
		}
		//All REF_ANY are resolved.
		if (!CheckConstrainDetermined(*cache))
		{
			return false;
		}
		//TODO calculate exportable references
		return true;
	}
	bool ListContainUndetermined(std::vector<ConstrainType>& l, ConstrainType& t)
	{
		for (auto& a : l)
		{
			if (a.ContainsUndetermined()) return true;
		}
		if (t.ContainsUndetermined()) return true;
		return false;
	}

	ConstrainType ConstructConstrainArgumentType(ConstrainCalculationCache& cache,
		GenericConstrain& constrain, std::size_t i)
	{
		auto& list = constrain.TypeReferences;
		auto& t = list[i];
		switch (t.Type)
		{
		case REF_ANY:
			return ConstrainType::UD(cache);
		case REF_TRY:
			return ConstrainType::Try(ConstructConstrainArgumentType(cache, constrain, t.Index));
		case REF_CLONE:
			return ConstructConstrainArgumentType(cache, constrain, t.Index);
		case REF_ARGUMENT:
			return cache.CheckArguments[t.Index];
		case REF_SELF:
			if (cache.CheckTarget.CType == CTT_FAIL)
			{
				throw RuntimeLoaderException("Invalid use of REF_SELF");
			}
			return cache.CheckTarget;
		case REF_ASSEMBLY:
		{
			auto ret = ConstrainType::G(cache.SrcAssembly, t.Index);
			for (std::size_t j = 1; list[i + j].Type != REF_EMPTY; ++j)
			{
				if (i + j == list.size())
				{
					throw RuntimeLoaderException("Invalid type reference");
				}
				ret.Args.push_back(ConstructConstrainArgumentType(cache, constrain, i + j));
			}
			return ret;
		}
		case REF_IMPORT:
		{
			auto assembly = FindAssemblyThrow(cache.SrcAssembly);
			if (t.Index > assembly->ImportTypes.size())
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			LoadingArguments la;
			FindExportType(assembly->ImportTypes[t.Index], la);
			auto ret = ConstrainType::G(la.Assembly, la.Id);
			for (std::size_t j = 1; list[i + j].Type != REF_EMPTY; ++j)
			{
				if (i + j == list.size())
				{
					throw RuntimeLoaderException("Invalid type reference");
				}
				ret.Args.push_back(ConstructConstrainArgumentType(cache, constrain, i + j));
			}
			if (assembly->ImportTypes[t.Index].GenericParameters != ret.Args.size())
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			return ret;
		}
		case REF_SUBTYPE:
		{
			if (t.Index > constrain.SubtypeNames.size())
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			auto ret = ConstrainType::SUB(constrain.SubtypeNames[t.Index]);
			for (std::size_t j = 1; list[i + j].Type != REF_EMPTY; ++j)
			{
				if (i + j == list.size())
				{
					throw RuntimeLoaderException("Invalid type reference");
				}
				ret.Args.push_back(ConstructConstrainArgumentType(cache, constrain, i + j));
			}
			if (ret.Args.size() == 0)
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			return ret;
		}
		default:
			throw RuntimeLoaderException("Invalid type reference");
		}
	}

	//ret: 1: determined something. 0: no change. -1: impossible (constrain check fails)
	int TryDetermineEqualTypes(ConstrainType& a, ConstrainType& b)
	{
		SimplifyConstrainType(a);
		SimplifyConstrainType(b);
		if (a.CType == CTT_FAIL || b.CType == CTT_FAIL) return -1;
		if (a.CType == CTT_ANY || b.CType == CTT_ANY)
		{
			if (a.CType == CTT_RT)
			{
				b.Undetermined.Parent->Determined(b.Undetermined.Index, a.Determined);
				return 1;
			}
			else if (b.CType == CTT_RT)
			{
				a.Undetermined.Parent->Determined(a.Undetermined.Index, b.Determined);
				return 1;
			}
			else
			{
				return 0;
			}
		}
		if (a.CType == CTT_SUBTYPE || b.CType == CTT_SUBTYPE) return 0;
		if (a.CType == CTT_RT && b.CType == CTT_RT)
		{
			if (a.Determined != b.Determined) return -1;
			return 0;
		}
		if (a.CType == CTT_GENERIC && b.CType == CTT_GENERIC)
		{
			if (a.TypeTemplateAssembly != b.TypeTemplateAssembly ||
				a.TypeTemplateIndex != b.TypeTemplateIndex ||
				a.Args.size() != b.Args.size())
			{
				return -1;
			}
			for (std::size_t i = 0; i < a.Args.size(); ++i)
			{
				int r = TryDetermineEqualTypes(a.Args[i], b.Args[i]);
				if (r != 0) return r;
			}
			return 0;
		}
		else if (a.CType == CTT_RT)
		{
			if (a.Determined->Args.Assembly != b.TypeTemplateAssembly ||
				a.Determined->Args.Id != b.TypeTemplateIndex ||
				a.Determined->Args.Arguments.size() != b.Args.size())
			{
				return -1;
			}
			for (std::size_t i = 0; i < b.Args.size(); ++i)
			{
				ConstrainType ct = ConstrainType::RT(a.Determined->Args.Arguments[i]);
				int r = TryDetermineEqualTypes(b.Args[i], ct);
				if (r != 0) return r;
			}
			return 0;
		}
		else //b.CType == CTT_RT
		{
			if (b.Determined->Args.Assembly != a.TypeTemplateAssembly ||
				b.Determined->Args.Id != a.TypeTemplateIndex ||
				b.Determined->Args.Arguments.size() != a.Args.size())
			{
				return -1;
			}
			for (std::size_t i = 0; i < a.Args.size(); ++i)
			{
				ConstrainType ct = ConstrainType::RT(b.Determined->Args.Arguments[i]);
				int r = TryDetermineEqualTypes(a.Args[i], ct);
				if (r != 0) return r;
			}
			return 0;
		}
	}

	int TryDetermineConstrainArgument(ConstrainCalculationCache& cache)
	{
		switch (cache.Source->Type)
		{
		case CONSTRAIN_EXIST:
		case CONSTRAIN_BASE:
		case CONSTRAIN_INTERFACE:
			return 0;
		case CONSTRAIN_SAME:
			if (cache.Arguments.size() != 1)
			{
				throw RuntimeLoaderException("Invalid constrain arguments");
			}
			return TryDetermineEqualTypes(cache.Arguments[0], cache.Target);
		case CONSTRAIN_TRAIT_ASSEMBLY:
		case CONSTRAIN_TRAIT_IMPORT:
			if (cache.Target.CType == CTT_RT)
			{
				//Only check member components if the parent (target) has been fully determined.
			}
			//for each clause (subconstrain, field, subtype, function)
			//  for subconstrain, create cache if necessary
			//  try determine something
		default:
			return 0;
		}
	}

	bool TrySimplifyConstrainType(ConstrainType& t, ConstrainType& parent)
	{
		SimplifyConstrainType(t);
		if (t.CType != CTT_RT)
		{
			if (t.CType == CTT_FAIL)
			{
				parent = ConstrainType::Fail();
			}
			return false;
		}
		assert(t.Determined);
		return true;
	}

	void SimplifyConstrainType(ConstrainType& t)
	{
		switch (t.CType)
		{
		case CTT_RT:
		case CTT_FAIL:
			//Elemental type. Can't simplify.
			return;
		case CTT_ANY:
			if (auto rt = t.Undetermined.Parent->GetDetermined(t.Undetermined.Index))
			{
				t = ConstrainType::RT(rt);
			}
			return;
		case CTT_GENERIC:
		{
			LoadingArguments la = { t.TypeTemplateAssembly, t.TypeTemplateIndex };
			for (auto& arg : t.Args)
			{
				if (!TrySimplifyConstrainType(arg, t))
				{
					return;
				}
				la.Arguments.push_back(arg.Determined);
			}
			if (t.TryArgumentConstrain)
			{
				auto tt = FindTypeTemplate(la);
				if (!CheckGenericArguments(tt->Generic, la))
				{
					t = ConstrainType::Fail();
					return;
				}
			}
			t = ConstrainType::RT(LoadTypeInternal(la, t.TryArgumentConstrain));
			return;
		}
		case CTT_SUBTYPE:
		{
			SubMemberLoadingArguments la;
			assert(t.Args.size() > 0);
			for (std::size_t i = 0; i < t.Args.size(); ++i)
			{
				if (!TrySimplifyConstrainType(t.Args[i], t))
				{
					return;
				}
				if (i == 0)
				{
					la = { t.Args[0].Determined, t.SubtypeName };
				}
				else
				{
					la.Arguments.push_back(t.Args[i].Determined);
				}
			}
			//LoadSubtype is too complicated to separate the constrain check.
			//We have to use a try block.
			//TODO maybe it's still better not to throw.
			if (t.TryArgumentConstrain)
			{
				try
				{
					t = ConstrainType::RT(LoadSubType(la));
					return;
				}
				catch (...)
				{
					//TODO or at least make sure it's really caused by constrain checking.
					t = ConstrainType::Fail();
					return;
				}
			}
			else
			{
				t = ConstrainType::RT(LoadSubType(la));
				return;
			}
		}
		default:
			assert(0);
			break;
		}
	}

	bool CheckSimplifiedConstrainType(ConstrainType& t)
	{
		SimplifyConstrainType(t);
		if (t.CType != CTT_RT)
		{
			assert(t.CType == CTT_FAIL);
			return false;
		}
		assert(t.Determined);
		return true;
	}

	bool CheckTraitDetermined(const std::string& a, Trait* t, ConstrainCalculationCache& cache)
	{
		//First sub-constrains
		if (cache.Children.size() < t->Generic.Constrains.size())
		{
			if (cache.Arguments.size() != t->Generic.ParameterCount)
			{
				throw RuntimeLoaderException("Invalid generic arguments");
			}
			CreateSubConstrainCache(a, &t->Generic, cache);
		}
		assert(cache.Children.size() == t->Generic.Constrains.size());
		for (auto& subconstrain : cache.Children)
		{
			//Not guaranteed to be determined, and we also need to calculate exports.
			//Use CheckConstrainCached.
			if (!CheckConstrainCached(subconstrain.get()))
			{
				return false;
			}
		}
		//TODO check fields, functions
		return true;
	}

	bool CheckConstrainDetermined(ConstrainCalculationCache& cache)
	{
		switch (cache.Source->Type)
		{
		case CONSTRAIN_EXIST:
			if (cache.Arguments.size() != 0)
			{
				throw RuntimeLoaderException("Invalid constrain arguments");
			}
			return CheckSimplifiedConstrainType(cache.Target);
		case CONSTRAIN_SAME:
			if (cache.Arguments.size() != 1)
			{
				throw RuntimeLoaderException("Invalid constrain arguments");
			}
			if (!CheckSimplifiedConstrainType(cache.Target) ||
				!CheckSimplifiedConstrainType(cache.Arguments[0]))
			{
				return false;
			}
			return cache.Target.Determined == cache.Arguments[0].Determined;
		case CONSTRAIN_BASE:
			if (cache.Arguments.size() != 1)
			{
				throw RuntimeLoaderException("Invalid constrain arguments");
			}
			if (!CheckSimplifiedConstrainType(cache.Target) ||
				!CheckSimplifiedConstrainType(cache.Arguments[0]))
			{
				return false;
			}
			return cache.Arguments[0].Determined->IsBaseTypeOf(cache.Target.Determined);
		case CONSTRAIN_INTERFACE:
			if (cache.Arguments.size() != 1)
			{
				throw RuntimeLoaderException("Invalid constrain arguments");
			}
			if (!CheckSimplifiedConstrainType(cache.Target) ||
				!CheckSimplifiedConstrainType(cache.Arguments[0]))
			{
				return false;
			}
			return cache.Arguments[0].Determined->IsInterfaceOf(cache.Target.Determined);
		case CONSTRAIN_TRAIT_ASSEMBLY:
		{
			auto assembly = FindAssemblyThrow(cache.SrcAssembly);
			if (cache.Source->Index >= assembly->Traits.size())
			{
				throw RuntimeLoaderException("Invalid trait reference");
			}
			auto trait = &assembly->Traits[cache.Source->Index];
			return CheckTraitDetermined(cache.SrcAssembly, trait, cache);
		}
		case CONSTRAIN_TRAIT_IMPORT:
		{
			auto assembly = FindAssemblyThrow(cache.SrcAssembly);
			LoadingArguments la;
			if (cache.Source->Index >= assembly->ImportTraits.size())
			{
				throw RuntimeLoaderException("Invalid trait reference");
			}
			if (!FindExportTrait(assembly->ImportTraits[cache.Source->Index], la))
			{
				throw RuntimeLoaderException("Invalid trait reference");
			}
			auto trait = &FindAssemblyThrow(la.Assembly)->Traits[la.Id];
			return CheckTraitDetermined(la.Assembly, trait, cache);
		}
		default:
			throw RuntimeLoaderException("Invalid constrain type");
		}
	}
};

inline bool RuntimeLoaderCore::CheckConstrains(const std::string& srcAssebly, GenericDeclaration* g,
	const std::vector<RuntimeType*>& args)
{
	auto l = static_cast<RuntimeLoaderConstrain*>(this);
	return l->CheckConstrainsImpl(srcAssebly, g, args);
}
