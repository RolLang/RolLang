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
			auto c = CreateConstrainCache(constrain, srcAssebly, cargs, ConstrainType::Fail(), &root);
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
		std::vector<ConstrainUndeterminedTypeInfo> UndeterminedTypes;
		RuntimeType* GetDetermined(std::size_t i)
		{
			return UndeterminedTypes[i].Determined;
		}
		void Determined(std::size_t i, RuntimeType* t)
		{
			UndeterminedTypes[i].Determined = t;
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
			auto id = src.UndeterminedTypes.size();
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
	struct TraitCacheFieldInfo
	{
		ConstrainType Type;
		ConstrainType TypeInTarget;
		std::size_t FieldIndex;
	};
	struct TraitCacheFunctionOverloadInfo
	{
		std::size_t Index;
		ConstrainType ReturnType;
		std::vector<ConstrainType> ParameterTypes;
	};
	struct TraitCacheFunctionInfo
	{
		std::vector<TraitCacheFunctionOverloadInfo> Overloads;
	};
	struct ConstrainCalculationCacheRoot;
	struct ConstrainCalculationCache
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

		//Following fields are only for trait constrains.
		bool TraitCacheCreated;
		bool TraitMemberResolved;
		Trait* Trait;
		std::string TraitAssembly;
		std::vector<TraitCacheFieldInfo> TraitFields;
	};
	struct ConstrainCalculationCacheRoot : ConstrainUndeterminedTypeSource
	{
		std::size_t Size;
	};

private:
	void InitTraitConstrainCache(ConstrainCalculationCache& cache)
	{
		switch (cache.Source->Type)
		{
		case CONSTRAIN_TRAIT_ASSEMBLY:
		{
			auto assembly = FindAssemblyThrow(cache.SrcAssembly);
			if (cache.Source->Index >= assembly->Traits.size())
			{
				throw RuntimeLoaderException("Invalid trait reference");
			}
			cache.Trait = &assembly->Traits[cache.Source->Index];
			cache.TraitAssembly = cache.SrcAssembly;
			break;
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
			cache.Trait = &FindAssemblyThrow(la.Assembly)->Traits[la.Id];
			cache.TraitAssembly = la.Assembly;
			break;
		}
		default:
			assert(0);
		}

		//We don't create cache here (higher chance to fail elsewhere).
		cache.TraitCacheCreated = false;
		cache.TraitMemberResolved = false;
	}

	bool AreConstrainTypesEqual(ConstrainType& a, ConstrainType& b)
	{
		SimplifyConstrainType(a);
		SimplifyConstrainType(b);

		//Note that different CType may produce same determined type, but
		//in a circular loading stack, there must be 2 to have exactly 
		//the same value (including CType, Args, etc).
		if (a.CType != b.CType) return false;

		switch (a.CType)
		{
		case CTT_FAIL:
			//Although we don't know whether they come from the same type,
			//since they both fail, they will lead to the same result (and
			//keep failing in children).
			return true;
		case CTT_ANY:
			return a.Undetermined.Parent == b.Undetermined.Parent &&
				a.Undetermined.Index == b.Undetermined.Index;
		case CTT_RT:
			return a.Determined == b.Determined;
		case CTT_GENERIC:
			if (a.TypeTemplateAssembly != b.TypeTemplateAssembly ||
				a.TypeTemplateIndex != b.TypeTemplateIndex)
			{
				return false;
			}
			break;
		case CTT_SUBTYPE:
			if (a.SubtypeName != b.SubtypeName)
			{
				return false;
			}
			break;
		default:
			assert(0);
		}

		//Unfortunately we cannot use operator== for std::vector: our comparison
		//requires non-constant reference to simplify.
		if (a.Args.size() != b.Args.size()) return false;
		for (std::size_t i = 0; i < a.Args.size(); ++i)
		{
			if (!AreConstrainTypesEqual(a.Args[i], b.Args[i])) return false;
		}

		return true;
	}

	bool AreConstrainsEqual(ConstrainCalculationCache& a, ConstrainCalculationCache& b)
	{
		if (a.Source != b.Source) return false;
		if (a.CheckArguments.size() != b.CheckArguments.size())
		{
			//This should not happen, but we don't want to limit it here.
			return false;
		}
		for (std::size_t i = 0; i < a.CheckArguments.size(); ++i)
		{
			if (!AreConstrainTypesEqual(a.CheckArguments[i], b.CheckArguments[i]))
			{
				return false;
			}
		}
		return true;
	}

	void EnsureSubConstrainCached(ConstrainCalculationCache& parent)
	{
		auto trait = parent.Trait;
		auto g = &trait->Generic;

		if (parent.TraitCacheCreated)
		{
			assert(parent.Children.size() == g->Constrains.size());
			assert(parent.TraitFields.size() == trait->Fields.size());
			//TODO other checks
			return;
		}

		assert(!parent.TraitMemberResolved);

		//Children (sub-constrains)
		assert(parent.Children.size() == 0);
		if (parent.Arguments.size() != g->ParameterCount)
		{
			throw RuntimeLoaderException("Invalid generic arguments");
		}
		for (auto& constrain : g->Constrains)
		{
			parent.Children.emplace_back(CreateConstrainCache(constrain, parent.TraitAssembly,
				parent.Arguments, parent.Target, parent.Root));
			auto ptr = parent.Children.back().get();
			ptr->Parent = &parent;

			//Check circular constrain.
			//Note that, same as what we do elsewhere in this project, 
			//we only need to check trait-trait constrain loop.
			//Trait-type or trait-function circular loop will trigger another
			//trait-trait, type-type or function-function circular check.

			//I have no better idea but to simplify and check.
			ConstrainCalculationCache* p = &parent;
			while (p != nullptr)
			{
				if (AreConstrainsEqual(*p, *ptr))
				{
					//Circular constrain is always considered as a program error.
					throw RuntimeLoaderException("Circular constrain check");
				}
				p = p->Parent;
			}
		}

		//Fields
		assert(parent.TraitCacheCreated == 0);
		for (auto& field : trait->Fields)
		{
			parent.TraitFields.push_back({ ConstructConstrainTraitType(parent, field.Type), {}, 0 });
		}

		//TODO Functions?

		parent.TraitMemberResolved = false;
		parent.TraitCacheCreated = true;
	}

	//ret 1: all members successfully resolved. 0: cannot resolve (not determined). -1: constrain fails
	int TryCalculateTraitSubMember(ConstrainCalculationCache& parent)
	{
		assert(parent.TraitCacheCreated);

		auto trait = parent.Trait;

		if (parent.TraitMemberResolved) return 1;

		SimplifyConstrainType(parent.Target);
		if (parent.Target.CType != CTT_RT) return 0;

		auto target = parent.Target.Determined;
		assert(target);

		auto tt = FindTypeTemplate(target->Args);

		for (std::size_t i = 0; i < trait->Fields.size(); ++i)
		{
			auto& f = trait->Fields[i];
			std::size_t fid = SIZE_MAX;
			for (auto& ft : tt->PublicFields)
			{
				if (ft.Name == f.ElementName)
				{
					fid = ft.Id;
					break;
				}
			}
			if (fid == SIZE_MAX)
			{
				return -1;
			}
			parent.TraitFields[i].FieldIndex = fid;

			if (target->Fields.size() == 0)
			{
				//We found the filed in type template, but now there is no field
				//loaded (can happen to reference types). We have to use template.
				//Fortunately, the target has determined generic arguments and has
				//passes its constrain check, which means we can simply use LoadRefType.
				//Note that we may still have constrain check failure when loading field
				//types, but that is considered as a program error instead of constrain
				//check failure of this constrain we are testing, and we can simply let 
				//it throws.

				auto type_id = tt->Fields[fid];
				auto field_type = LoadRefType({ target, tt->Generic }, type_id);
				parent.TraitFields[i].TypeInTarget = ConstrainType::RT(field_type);
			}
			else
			{
				parent.TraitFields[i].TypeInTarget = ConstrainType::RT(target->Fields[fid].Type);
			}
		}

		//TODO function

		parent.TraitMemberResolved = true;
		return 1;
	}

private:
	//TODO separate create+basic fields from load argument/target types (reduce # of args)
	std::unique_ptr<ConstrainCalculationCache> CreateConstrainCache(GenericConstrain& constrain,
		const std::string& srcAssebly, const std::vector<ConstrainType>& args, ConstrainType checkTarget,
		ConstrainCalculationCacheRoot* root)
	{
		auto ret = std::make_unique<ConstrainCalculationCache>();
		ret->Root = root;
		ret->Source = &constrain;
		ret->SrcAssembly = srcAssebly;
		ret->CheckArguments = args;
		ret->CheckTarget = checkTarget;
		ret->Target = ConstructConstrainArgumentType(*ret.get(), constrain, constrain.Target);
		for (auto a : constrain.Arguments)
		{
			ret->Arguments.push_back(ConstructConstrainArgumentType(*ret.get(), constrain, a));
		}

		if (constrain.Type == CONSTRAIN_TRAIT_ASSEMBLY ||
			constrain.Type == CONSTRAIN_TRAIT_IMPORT)
		{
			InitTraitConstrainCache(*ret.get());
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

	ConstrainType ConstructConstrainTraitType(ConstrainCalculationCache& cache, std::size_t i)
	{
		auto trait = cache.Trait;
		assert(trait);
		auto& list = trait->Generic.Types;
		auto& t = list[i];

		switch (t.Type & REF_REFTYPES)
		{
		case REF_ANY:
		case REF_TRY:
			throw RuntimeLoaderException("Invalid type reference");
		case REF_CLONE:
			return ConstructConstrainTraitType(cache, t.Index);
		case REF_ARGUMENT:
			return cache.Arguments[t.Index];
		case REF_SELF:
			return cache.Target;
		case REF_ASSEMBLY:
		{
			auto ret = ConstrainType::G(cache.TraitAssembly, t.Index);
			for (std::size_t j = 1; list[i + j].Type != REF_EMPTY; ++j)
			{
				if (i + j == list.size())
				{
					throw RuntimeLoaderException("Invalid type reference");
				}
				ret.Args.push_back(ConstructConstrainTraitType(cache, i + j));
			}
			return ret;
		}
		case REF_IMPORT:
		{
			auto assembly = FindAssemblyThrow(cache.TraitAssembly);
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
				ret.Args.push_back(ConstructConstrainTraitType(cache, i + j));
			}
			if (assembly->ImportTypes[t.Index].GenericParameters != ret.Args.size())
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			return ret;
		}
		case REF_SUBTYPE:
		{
			if (t.Index > trait->Generic.SubtypeNames.size())
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			auto ret = ConstrainType::SUB(trait->Generic.SubtypeNames[t.Index]);
			for (std::size_t j = 1; list[i + j].Type != REF_EMPTY; ++j)
			{
				if (i + j == list.size())
				{
					throw RuntimeLoaderException("Invalid type reference");
				}
				ret.Args.push_back(ConstructConstrainTraitType(cache, i + j));
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

	ConstrainType ConstructConstrainArgumentType(ConstrainCalculationCache& cache,
		GenericConstrain& constrain, std::size_t i)
	{
		auto& list = constrain.TypeReferences;
		auto& t = list[i];
		switch (t.Type)
		{
		case REF_ANY:
			return ConstrainType::UD(*cache.Root);
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

	//Return 0, 1, or -1 (see TryDetermineEqualTypes)
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
		{
			EnsureSubConstrainCached(cache);
			auto resolveMembers = TryCalculateTraitSubMember(cache);
			if (resolveMembers <= 0) return resolveMembers;

			auto trait = cache.Trait;
			auto target = cache.Target.Determined;
			assert(target);

			//Note that we create cache for sub-constrains but do not use it
			//for determining REF_ANY. This is because linked traits with any 
			//type can easily lead to infinite constrain chain, which is not 
			//circular (because of the new REF_ANY) and difficult to check.
			//We simplify the situation by not checking it. Because of the 
			//undetermined REF_ANY, the constrain will fail at the parent level.
			//Example:
			//  class A requires some_trait<any>(A)
			//  some_trait<T1>(T) requires some_trait<any>(T1) (and ...)

			for (auto& f : cache.TraitFields)
			{
				auto ret = TryDetermineEqualTypes(f.TypeInTarget, f.Type);
				if (ret != 0) return ret;
			}

			//TODO functions
			//Determining REF_ANY with functions is a NP-hard problem. So we
			//can only try with all possible combination at the end.

		}
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
			SubMemberLoadingArguments lg;
			assert(t.Args.size() > 0);
			for (std::size_t i = 0; i < t.Args.size(); ++i)
			{
				if (!TrySimplifyConstrainType(t.Args[i], t))
				{
					return;
				}
				if (i == 0)
				{
					lg = { t.Args[0].Determined, t.SubtypeName };
				}
				else
				{
					lg.Arguments.push_back(t.Args[i].Determined);
				}
			}

			LoadingArguments la;
			if (!FindSubType(lg, la))
			{
				if (t.TryArgumentConstrain)
				{
					t = ConstrainType::Fail();
					return;
				}
				throw RuntimeLoaderException("Invalid subtype constrain");
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

	bool CheckTraitDetermined(ConstrainCalculationCache& cache)
	{
		EnsureSubConstrainCached(cache);
		if (TryCalculateTraitSubMember(cache) != 1)
		{
			//Resolving submember only requires Target to be determined,
			//which should be success if it goes here.
			return false;
		}

		//Sub-constrains in trait
		for (auto& subconstrain : cache.Children)
		{
			//Not guaranteed to be determined, and we also need to calculate exports,
			//so use CheckConstrainCached.
			if (!CheckConstrainCached(subconstrain.get()))
			{
				return false;
			}
		}

		auto target = cache.Target.Determined;
		assert(target);

		//Field
		for (std::size_t i = 0; i < cache.TraitFields.size(); ++i)
		{
			auto& tf = cache.TraitFields[i];
			if (!CheckSimplifiedConstrainType(tf.Type)) return false;
			auto field_type_target = tf.TypeInTarget.Determined;
			auto field_type_trait = tf.Type.Determined;
			assert(field_type_target && field_type_trait);
			if (field_type_target != field_type_trait)
			{
				return false;
			}
		}

		//TODO Function

		return true;
	}

	bool CheckLoadingTypeBase(RuntimeType* typeChecked, RuntimeType* typeBase)
	{
		if (typeChecked == typeBase) return true;

		//Loaded
		if (typeChecked->BaseType.Type != nullptr)
		{
			return CheckLoadingTypeBase(typeChecked->BaseType.Type, typeBase);
		}

		//Not yet, or no base type. Load using LoadRefType.
		auto tt = FindTypeTemplate(typeChecked->Args);
		auto loadedBase = LoadRefType({ typeChecked, tt->Generic }, tt->Base.InheritedType);

		return loadedBase != nullptr && CheckLoadingTypeBase(loadedBase, typeBase);
	}

	bool CheckLoadingTypeInterface(RuntimeType* typeChecked, RuntimeType* typeBase)
	{
		if (typeChecked == typeBase) return true;

		//Loaded
		if (typeChecked->Interfaces.size() > 0)
		{
			for (auto& i : typeChecked->Interfaces)
			{
				if (CheckLoadingTypeInterface(i.Type, typeBase)) return true;
			}
			return false;
		}

		//Not yet, or no interfaces. Load using LoadRefType.
		auto tt = FindTypeTemplate(typeChecked->Args);
		for (auto& i : tt->Interfaces)
		{
			auto loadedInterface = LoadRefType({ typeChecked, tt->Generic }, i.InheritedType);
			if (CheckLoadingTypeInterface(loadedInterface, typeBase)) return true;
		}
		return false;
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
			return CheckLoadingTypeBase(cache.Target.Determined, cache.Arguments[0].Determined);
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
			return CheckLoadingTypeInterface(cache.Target.Determined, cache.Arguments[0].Determined);
		case CONSTRAIN_TRAIT_ASSEMBLY:
		case CONSTRAIN_TRAIT_IMPORT:
			return CheckTraitDetermined(cache);
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
