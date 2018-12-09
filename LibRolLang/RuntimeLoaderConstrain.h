#pragma once
#include "RuntimeLoaderRefList.h"

struct RuntimeLoaderConstrain : RuntimeLoaderRefList
{
public:
	bool CheckConstrainsImpl(const std::string& srcAssebly, GenericDeclaration* g,
		const std::vector<RuntimeType*>& args, ConstrainExportList* exportList)
	{
		std::vector<ConstrainType> cargs;
		ConstrainCalculationCacheRoot root = {};
		for (auto a : args)
		{
			cargs.push_back(ConstrainType::RT(&root, a));
		}
		for (auto& constrain : g->Constrains)
		{
			auto c = CreateConstrainCache(constrain, srcAssebly, cargs, ConstrainType::Fail(&root), &root);
			if (!CheckConstrainCached(c.get()))
			{
				return false;
			}

			auto prefix = constrain.ExportName + "/";

			if (exportList != nullptr)
			{
				//Export types
				for (std::size_t i = 0; i < g->Types.size(); ++i)
				{
					if ((g->Types[i].Type & REF_REFTYPES) != REF_CONSTRAIN) continue;
					auto& name = g->NamesList[g->Types[i].Index];
					if (name.compare(0, prefix.length(), prefix) == 0)
					{
						auto type = FindConstrainExportType(c.get(), name.substr(prefix.length()));
						if (type != nullptr)
						{
							ConstrainExportListEntry entry;
							entry.EntryType = CONSTRAIN_EXPORT_TYPE;
							entry.Index = i;
							entry.Type = type;
							exportList->emplace_back(std::move(entry));
						}
					}
				}

				//Export functions
				for (std::size_t i = 0; i < g->Functions.size(); ++i)
				{
					if ((g->Functions[i].Type & REF_REFTYPES) != REF_CONSTRAIN) continue;
					auto& name = g->NamesList[g->Functions[i].Index];
					if (name.compare(0, prefix.length(), prefix) == 0)
					{
						auto func = FindConstrainExportFunction(c.get(), name.substr(prefix.length()));
						if (func != nullptr)
						{
							ConstrainExportListEntry entry;
							entry.EntryType = CONSTRAIN_EXPORT_FUNCTION;
							entry.Index = i;
							entry.Function = func;
							exportList->emplace_back(std::move(entry));
						}
					}
				}

				//TODO fields
			}

			root.Clear();
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
		CTT_EMPTY,
	};

	struct ConstrainUndeterminedTypeSource;
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
	struct ConstrainCalculationCacheRoot;
	struct ConstrainType
	{
		ConstrainCalculationCacheRoot* Root;
		ConstrainTypeType CType;
		RuntimeType* Determined;
		std::string TypeTemplateAssembly;
		std::size_t TypeTemplateIndex;
		std::string SubtypeName;
		std::vector<ConstrainType> Args;
		std::size_t Undetermined;
		bool TryArgumentConstrain;


		//Following 2 fields are related to backtracking.
		ConstrainTypeType OType;
		std::size_t CLevel;

		static ConstrainType Fail(ConstrainCalculationCacheRoot* root)
		{
			return { root, CTT_FAIL };
		}

		static ConstrainType RT(ConstrainCalculationCacheRoot* root, RuntimeType* rt)
		{
			return { root, CTT_RT, rt };
		}

		static ConstrainType UD(ConstrainCalculationCacheRoot* root)
		{
			auto id = root->UndeterminedTypes.size();
			root->UndeterminedTypes.push_back({});
			return { root, CTT_ANY, nullptr, {}, 0, {}, {}, id };
		}

		static ConstrainType G(ConstrainCalculationCacheRoot* root, const std::string& a, std::size_t i)
		{
			return { root, CTT_GENERIC, nullptr, a, i };
		}

		static ConstrainType SUB(ConstrainCalculationCacheRoot* root, const std::string& n)
		{
			return { root, CTT_SUBTYPE, nullptr, {}, {}, n };
		}

		static ConstrainType Try(ConstrainType&& t)
		{
			auto ret = ConstrainType(std::move(t));
			ret.TryArgumentConstrain = true;
			return ret;
		}

		static ConstrainType Empty(ConstrainCalculationCacheRoot* root)
		{
			return { root, CTT_EMPTY };
		}

		void DeductFail()
		{
			assert(CLevel == 0);
			OType = CType;
			CLevel = Root->GetCurrentLevel();
			CType = CTT_FAIL;
			Root->BacktrackList.push_back(this);
		}

		void DeductRT(RuntimeType* rt)
		{
			assert(CLevel == 0);
			OType = CType;
			CLevel = Root->GetCurrentLevel();
			CType = CTT_RT;
			Determined = rt;
			Root->BacktrackList.push_back(this);
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
		std::size_t CurrentOverload;
		ConstrainType TraitReturnType;
		std::vector<ConstrainType> TraitParameterTypes;
	};
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
		std::vector<TraitCacheFunctionInfo> TraitFunctions;
		std::vector<ConstrainType> TraitFunctionUndetermined;
	};
	struct ConstrainCalculationCacheRoot : ConstrainUndeterminedTypeSource
	{
		std::size_t Size;

		std::vector<ConstrainType*> BacktrackList;
		std::vector<std::size_t> BacktrackListSize;

		void Clear()
		{
			Size = 0;
			BacktrackList.clear();
			BacktrackListSize.clear();
		}

		bool IsUndeterminedType(ConstrainType& ct)
		{
			switch (ct.CType)
			{
			case CTT_RT:
			case CTT_EMPTY:
				return false;
			case CTT_GENERIC:
			case CTT_SUBTYPE:
				for (auto& a : ct.Args)
				{
					if (IsUndeterminedType(a)) return true;
				}
				return false;
			case CTT_ANY:
				return !GetDetermined(ct.Undetermined);
			default:
				assert(0);
				return false;
			}
		}

		std::size_t StartBacktrackPoint()
		{
			auto id = BacktrackListSize.size();
			BacktrackListSize.push_back(BacktrackList.size());
			return id;
		}

		void DoBacktrack(std::size_t level)
		{
			assert(level < BacktrackListSize.size());
			auto size = BacktrackListSize[level];
			assert(size <= BacktrackList.size());
			auto num = BacktrackList.size() - size;
			for (std::size_t i = 0; i < num; ++i)
			{
				auto t = BacktrackList.back();
				if (t->CLevel > level)
				{
					t->CType = t->OType;
					t->CLevel = 0;
					t->Determined = nullptr;
				}
				BacktrackList.pop_back();
			}
		}

		std::size_t GetCurrentLevel()
		{
			return BacktrackListSize.size();
		}
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
		//TODO Probably we don't need to simplify it.

		SimplifyConstrainType(a);
		SimplifyConstrainType(b);

		//Note that different CType may produce same determined type, but
		//in a circular loading stack, there must be 2 to have exactly 
		//the same value (including CType, Args, etc).
		if (a.CType != b.CType) return false;

		switch (a.CType)
		{
		case CTT_EMPTY:
		case CTT_FAIL:
			//Although we don't know whether they come from the same type,
			//since they both fail, they will lead to the same result (and
			//keep failing in children).
			return true;
		case CTT_ANY:
			return a.Root == b.Root && a.Undetermined == b.Undetermined;
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

		for (auto& func : trait->Functions)
		{
			TraitCacheFunctionInfo func_info = {};
			func_info.TraitReturnType = ConstructConstrainTraitType(parent, func.ReturnType);
			for (auto p : func.ParameterTypes)
			{
				func_info.TraitParameterTypes.push_back(ConstructConstrainTraitType(parent, p));
			}
			parent.TraitFunctions.emplace_back(std::move(func_info));
		}

		parent.TraitMemberResolved = false;
		parent.TraitCacheCreated = true;
	}

	//ret 1: all members successfully resolved. 0: cannot resolve (not determined). -1: constrain fails
	int TryCalculateTraitSubMember(ConstrainCalculationCache& parent)
	{
		assert(parent.TraitCacheCreated);

		if (parent.TraitMemberResolved) return 1;
		auto trait = parent.Trait;

		SimplifyConstrainType(parent.Target);
		if (parent.Target.CType != CTT_RT && parent.Target.CType != CTT_EMPTY)
		{
			return 0;
		}

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
				parent.TraitFields[i].TypeInTarget = ConstrainType::RT(parent.Root, field_type);
			}
			else
			{
				parent.TraitFields[i].TypeInTarget = 
					ConstrainType::RT(parent.Root, target->Fields[fid].Type);
			}
		}

		std::vector<ConstrainType> ud;
		for (std::size_t i = 0; i < trait->Functions.size(); ++i)
		{
			auto& f = trait->Functions[i];
			for (std::size_t j = 0; j < tt->PublicFunctions.size(); ++j)
			{
				if (tt->PublicFunctions[j].Name != f.ElementName) continue;
				TraitCacheFunctionOverloadInfo fi = {};
				fi.Index = tt->PublicFunctions[j].Id;
				ud.clear();
				if (!LoadTraitFunctionCacheInfo(parent, tt->Generic, target->Args.Assembly, fi, ud))
				{
					continue;
				}
				//TODO handle parameter pack
				if (fi.ParameterTypes.size() != f.ParameterTypes.size())
				{
					continue;
				}
				if (!CheckTypePossiblyEqual(fi.ReturnType, parent.TraitFunctions[i].TraitReturnType))
				{
					continue;
				}
				for (std::size_t k = 0; k < fi.ParameterTypes.size(); ++k)
				{
					if (!CheckTypePossiblyEqual(fi.ParameterTypes[k],
						parent.TraitFunctions[i].TraitParameterTypes[k]))
					{
						continue;
					}
				}
				parent.TraitFunctions[i].Overloads.emplace_back(std::move(fi));
				parent.TraitFunctionUndetermined.insert(parent.TraitFunctionUndetermined.end(),
					ud.begin(), ud.end());
			}
			if (parent.TraitFunctions[i].Overloads.size() == 0)
			{
				//Fail if any function does not match.
				return -1;
			}
		}

		parent.TraitMemberResolved = true;
		return 1;
	}

	bool LoadTraitFunctionCacheInfo(ConstrainCalculationCache& parent, GenericDeclaration& g,
		const std::string& src_assembly, TraitCacheFunctionOverloadInfo& result,
		std::vector<ConstrainType> additionalUd)
	{
		assert(&g == &FindTypeTemplate(parent.Target.Determined->Args)->Generic);

		auto target = parent.Target.Determined;
		assert(target);

		auto id = result.Index;
		if (id >= g.Functions.size())
		{
			throw RuntimeLoaderException("Invalid function reference");
		}

		//First resolve any REF_CLONE so that we don't need to worry it any more.
		//TODO detect circular REF_CLONE
		while (g.Functions[id].Type == REF_CLONE)
		{
			id = g.Functions[id].Index;
			if (id >= g.Functions.size())
			{
				throw RuntimeLoaderException("Invalid function reference");
			}
		}

		//Find the function template.
		LoadingArguments la;
		switch (g.Functions[id].Type & REF_REFTYPES)
		{
		case REF_ASSEMBLY:
			la.Assembly = src_assembly;
			la.Id = g.Functions[id].Index;
			break;
		case REF_IMPORT:
		{
			auto a = FindAssemblyThrow(src_assembly);
			if (g.Functions[id].Index >= a->ImportFunctions.size())
			{
				throw RuntimeLoaderException("Invalid function reference");
			}
			auto i = a->ImportFunctions[g.Functions[id].Index];
			if (!FindExportFunction(i, la))
			{
				throw RuntimeLoaderException("Import function not found");
			}
			break;
		}
		default:
			throw RuntimeLoaderException("Invalid function reference");
		}

		//About the arguments:
		//We are in a type. Its Functions[id] specify a function. The argument to the
		//function is the arg list in the RefList plus some additional REF_ANY.
		//To calculate those in the RefList, we need the args to the type itself.

		auto additional = GetFunctionAdditionalArgumentNumber(g, id);

		std::vector<ConstrainType> typeArgs;
		for (auto ta : target->Args.Arguments)
		{
			typeArgs.emplace_back(ConstrainType::RT(parent.Root, ta));
		}
		//Note that additional arguments are appended to type arguments.
		for (std::size_t i = typeArgs.size(); i < additional; ++i)
		{
			auto t = ConstrainType::UD(parent.Root);
			typeArgs.emplace_back(t);
			additionalUd.emplace_back(t);
		}

		std::vector<ConstrainType> funcArgs;
		auto& type_assembly = parent.Target.Determined->Args.Assembly;
		for (auto i = id + 1; i < g.Functions.size(); ++i)
		{
			if (g.Functions[i].Type == REF_EMPTY) break;
			//Already checked in GetFunctionAdditionalArgumentNumber. Using assert.
			assert(g.Functions[i].Type == REF_CLONETYPE);
			assert(i < g.Functions.size());
			funcArgs.push_back(ConstructConstrainRefListType(parent.Root,
				g, type_assembly, g.Functions[i].Index, typeArgs, target));
		}

		Function* ft = FindFunctionTemplate(la.Assembly, la.Id);

		//Construct ConstrainType for ret and params.
		result.ReturnType = ConstructConstrainRefListType(parent.Root, ft->Generic,
			la.Assembly, ft->ReturnValue.TypeId, funcArgs, nullptr);
		for (auto& parameter : ft->Parameters)
		{
			result.ParameterTypes.emplace_back(ConstructConstrainRefListType(parent.Root, ft->Generic,
				la.Assembly, parameter.TypeId, funcArgs, nullptr));
		}
		return true;
	}

	//Scan the function reference. Make sure it's valid. Return the total number of args needed.
	//TODO consider move to RefList
	std::size_t GetFunctionAdditionalArgumentNumber(GenericDeclaration& g, std::size_t id)
	{
		if (id >= g.Functions.size())
		{
			throw RuntimeLoaderException("Invalid function reference");
		}
		std::size_t ret = 0;
		switch (g.Functions[id].Type & REF_REFTYPES)
		{
		case REF_CLONE:
			//TODO detect circular REF_CLONE
			return GetFunctionAdditionalArgumentNumber(g, g.Functions[id].Index);
		case REF_ASSEMBLY:
		case REF_IMPORT:
			while (++id < g.Functions.size() && g.Functions[id].Type == REF_CLONETYPE)
			{
				auto a = GetTypeAdditionalArgumentNumber(g, g.Functions[id].Index);
				if (a > ret) ret = a;
			}
			if (id == g.Functions.size())
			{
				throw RuntimeLoaderException("Invalid function reference");
			}
			return ret;
		default:
			throw RuntimeLoaderException("Invalid function reference");
		}
	}

	//TODO consider move to RefList
	std::size_t GetTypeAdditionalArgumentNumber(GenericDeclaration& g, std::size_t id)
	{
		if (id >= g.Types.size())
		{
			throw RuntimeLoaderException("Invalid type reference");
		}
		std::size_t ret = 0;
		auto t = g.Types[id];
		switch (t.Type & REF_REFTYPES)
		{
		case REF_CLONE:
			//TODO detect circular REF_CLONE
			return GetTypeAdditionalArgumentNumber(g, t.Index);
		case REF_ASSEMBLY:
		case REF_IMPORT:
		case REF_SUBTYPE:
			while (++id < g.Types.size())
			{
				auto a = GetTypeAdditionalArgumentNumber(g, id);
				if (a > ret) ret = a;
			}
			return ret;
		case REF_ARGUMENT:
			return t.Index + 1;
		case REF_SELF:
			return 0; //In case this function is used with type's GenericDeclaration.
		default:
			throw RuntimeLoaderException("Invalid type reference");
		}
	}

	//TODO separate create+basic fields from load argument/target types (reduce # of args)
	std::unique_ptr<ConstrainCalculationCache> CreateConstrainCache(GenericConstrain& constrain,
		const std::string& srcAssebly, const std::vector<ConstrainType>& args, ConstrainType checkTarget,
		ConstrainCalculationCacheRoot* root)
	{
		root->Size += 1;
		//TODO check loading limit (low priority)

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

	//Check without changing function overload candidates.
	bool CheckConstrainCachedSinglePass(ConstrainCalculationCache* cache)
	{
		//TODO we only need to  do it for trait
		//One pass to create function list (will produce more REF_ANY).
		if (TryDetermineConstrainArgument(*cache) == -1) return false;
		while (ListContainUndetermined(cache->Root, cache))
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
		return true;
	}

	bool CheckConstrainCached(ConstrainCalculationCache* cache)
	{
		do
		{
			auto id = cache->Root->StartBacktrackPoint();
			if (CheckConstrainCachedSinglePass(cache)) return true;
			cache->Root->DoBacktrack(id);
		} while (MoveToNextCandidates(cache));
		return false;
	}

	bool MoveToNextCandidates(ConstrainCalculationCache* cache)
	{
		//First move children (they may cause parent to fail).
		//But we don't need to create the children if they don't exist,
		//because if so, they can't make parent to fail.
		for (auto& t : cache->Children)
		{
			if (MoveToNextCandidates(t.get()))
			{
				return true;
			}
		}
		for (std::size_t i = 0; i < cache->TraitFunctions.size(); ++i)
		{
			//Reverse iterate
			auto& f = cache->TraitFunctions[cache->TraitFunctions.size() - 1 - i];
			if (++f.CurrentOverload < f.Overloads.size())
			{
				return true;
			}
			f.CurrentOverload = 0;
		}

		//Failed (no more overloads). Note that all have been set back to 0.
		return false;
	}

	static bool ListContainUndetermined(ConstrainCalculationCacheRoot* root,
		ConstrainCalculationCache* cache)
	{
		for (auto& a : cache->Arguments)
		{
			if (root->IsUndeterminedType(a)) return true;
		}
		for (auto& a : cache->TraitFunctionUndetermined)
		{
			if (root->IsUndeterminedType(a)) return true;
		}
		if (root->IsUndeterminedType(cache->Target)) return true;
		return false;
	}

	//TODO simplify parameters using struct

	void LoadConstrainTypeArgList(ConstrainType& type, GenericDeclaration& g, std::size_t index,
		const std::string& src, std::vector<ConstrainType>& arguments, RuntimeType* selfType)
	{
		for (std::size_t i = index + 1; i < g.Types.size(); ++i)
		{
			if (g.Types[i].Type == REF_EMPTY) break;
			if (i == g.Types.size() - 1)
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			type.Args.emplace_back(ConstructConstrainRefListType(type.Root,
				g, src, i, arguments, selfType));
		}
	}

	ConstrainType ConstructConstrainRefListType(ConstrainCalculationCacheRoot* root, GenericDeclaration& g,
		const std::string& src, std::size_t i, std::vector<ConstrainType>& arguments, RuntimeType* selfType)
	{
		if (i >= g.Types.size())
		{
			throw RuntimeLoaderException("Invalid type reference");
		}
		while (g.Types[i].Type == REF_CLONE)
		{
			i = g.Types[i].Index;
			if (i >= g.Types.size())
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
		}
		switch (g.Types[i].Type & REF_REFTYPES)
		{
		case REF_EMPTY:
			return ConstrainType::Empty(root);
		case REF_ARGUMENT:
			if (g.Types[i].Index >= arguments.size())
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			return arguments[g.Types[i].Index];
		case REF_SELF:
			if (selfType != nullptr)
			{
				return ConstrainType::RT(root, selfType);
			}
			return ConstrainType::Fail(root);
		case REF_ASSEMBLY:
		{
			ConstrainType ret = ConstrainType::G(root, src, g.Types[i].Index);
			LoadConstrainTypeArgList(ret, g, i, src, arguments, selfType);
			return ret;
		}
		case REF_IMPORT:
		{
			LoadingArguments la;
			auto a = FindAssemblyThrow(src);
			if (g.Types[i].Index >= a->ImportTypes.size())
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			auto import_info = a->ImportTypes[g.Types[i].Index];
			if (!FindExportType(import_info, la))
			{
				throw RuntimeLoaderException("Import type not found");
			}
			ConstrainType ret = ConstrainType::G(root, la.Assembly, la.Id);
			LoadConstrainTypeArgList(ret, g, i, src, arguments, selfType);
			return ret;
		}
		case REF_SUBTYPE:
		{
			ConstrainType ret = ConstrainType::SUB(root, g.NamesList[g.Types[i].Index]);
			LoadConstrainTypeArgList(ret, g, i, src, arguments, selfType);
			if (ret.Args.size() == 0)
			{
				//Parent type cannot be empty (not checked in LoadConstrainTypeArgList).
				throw RuntimeLoaderException("Invalid type reference");
			}
			return ret;
		}
		default:
			throw RuntimeLoaderException("Invalid type reference");
		}
	}

	ConstrainType ConstructConstrainTraitType(ConstrainCalculationCache& cache, std::size_t i)
	{
		auto trait = cache.Trait;
		assert(trait);
		auto& list = trait->Generic.Types;
		auto& t = list[i];

		switch (t.Type & REF_REFTYPES)
		{
		case REF_CLONE:
			//TODO detect circular REF_CLONE
			return ConstructConstrainTraitType(cache, t.Index);
		case REF_ARGUMENT:
			if (t.Index >= cache.Arguments.size())
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			return cache.Arguments[t.Index];
		case REF_SELF:
			return cache.Target;
		case REF_ASSEMBLY:
		{
			auto ret = ConstrainType::G(cache.Root, cache.TraitAssembly, t.Index);
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
			auto ret = ConstrainType::G(cache.Root, la.Assembly, la.Id);
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
			if (t.Index > trait->Generic.NamesList.size())
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			auto ret = ConstrainType::SUB(cache.Root, trait->Generic.NamesList[t.Index]);
			for (std::size_t j = 1; list[i + j].Type != REF_EMPTY; ++j)
			{
				//TODO check such REF_EMPTY termination check (REF_EMPTY is not optional)
				if (i + j >= list.size() - 1)
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
		case REF_EMPTY:
			return ConstrainType::Empty(cache.Root);
		case REF_ANY:
		case REF_TRY:
		default:
			throw RuntimeLoaderException("Invalid type reference");
		}
	}

	ConstrainType ConstructConstrainArgumentType(ConstrainCalculationCache& cache,
		GenericConstrain& constrain, std::size_t i)
	{
		auto& list = constrain.TypeReferences;
		auto& t = list[i];
		switch (t.Type & REF_REFTYPES)
		{
		case REF_ANY:
			return ConstrainType::UD(cache.Root);
		case REF_TRY:
			return ConstrainType::Try(ConstructConstrainArgumentType(cache, constrain, t.Index));
		case REF_CLONE:
			//TODO detect circular REF_CLONE
			return ConstructConstrainArgumentType(cache, constrain, t.Index);
		case REF_ARGUMENT:
			if (t.Index >= cache.CheckArguments.size())
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			return cache.CheckArguments[t.Index];
		case REF_SELF:
			if (cache.CheckTarget.CType == CTT_FAIL)
			{
				throw RuntimeLoaderException("Invalid use of REF_SELF");
			}
			return cache.CheckTarget;
		case REF_ASSEMBLY:
		{
			auto ret = ConstrainType::G(cache.Root, cache.SrcAssembly, t.Index);
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
			auto ret = ConstrainType::G(cache.Root, la.Assembly, la.Id);
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
			if (t.Index > constrain.NamesList.size())
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			auto ret = ConstrainType::SUB(cache.Root, constrain.NamesList[t.Index]);
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
	
	bool CheckTypePossiblyEqual(ConstrainType& a, ConstrainType& b)
	{
		//We only need a quick check to eliminate most overloads. Don't simplify.
		if (a.CType == CTT_FAIL || a.CType == CTT_FAIL) return false;
		if (a.CType == CTT_EMPTY || b.CType == CTT_EMPTY)
		{
			return a.CType == b.CType;
		}
		if (a.CType == CTT_ANY || b.CType == CTT_ANY) return true;
		if (a.CType == CTT_SUBTYPE || b.CType == CTT_SUBTYPE) return true;
		if (a.CType == CTT_RT && b.CType == CTT_RT)
		{
			return a.Determined == b.Determined;
		}
		if (a.CType == CTT_GENERIC && b.CType == CTT_GENERIC)
		{
			if (a.TypeTemplateAssembly != b.TypeTemplateAssembly ||
				a.TypeTemplateIndex != b.TypeTemplateIndex ||
				a.Args.size() != b.Args.size())
			{
				return false;
			}
			for (std::size_t i = 0; i < a.Args.size(); ++i)
			{
				if (!CheckTypePossiblyEqual(a.Args[i], b.Args[i])) return false;
			}
			return true;
		}
		else if (a.CType == CTT_RT)
		{
			if (a.Determined->Args.Assembly != b.TypeTemplateAssembly ||
				a.Determined->Args.Id != b.TypeTemplateIndex ||
				a.Determined->Args.Arguments.size() != b.Args.size())
			{
				return false;
			}
			for (std::size_t i = 0; i < b.Args.size(); ++i)
			{
				auto ct = ConstrainType::RT(b.Args[i].Root, a.Determined->Args.Arguments[i]);
				if (!CheckTypePossiblyEqual(b.Args[i], ct)) return false;
			}
			return true;
		}
		else //b.CType == CTT_RT
		{
			if (b.Determined->Args.Assembly != a.TypeTemplateAssembly ||
				b.Determined->Args.Id != a.TypeTemplateIndex ||
				b.Determined->Args.Arguments.size() != a.Args.size())
			{
				return false;
			}
			for (std::size_t i = 0; i < a.Args.size(); ++i)
			{
				auto ct = ConstrainType::RT(a.Args[i].Root, b.Determined->Args.Arguments[i]);
				if (!CheckTypePossiblyEqual(a.Args[i], ct)) return false;
			}
			return true;
		}
	}

	//ret: 1: determined something. 0: no change. -1: impossible (constrain check fails).
	int TryDetermineEqualTypes(ConstrainType& a, ConstrainType& b)
	{
		//Should not modify a or b except for calling SimplifyConstrainType at the beginning.

		SimplifyConstrainType(a);
		SimplifyConstrainType(b);
		if (a.CType == CTT_EMPTY || b.CType == CTT_EMPTY)
		{
			//We don't allow CTT_ANY to be empty.
			return 0;
		}
		if (a.CType == CTT_FAIL || b.CType == CTT_FAIL) return -1;
		if (a.CType == CTT_ANY || b.CType == CTT_ANY)
		{
			if (a.CType == CTT_RT)
			{
				b.Root->Determined(b.Undetermined, a.Determined);
				return 1;
			}
			else if (b.CType == CTT_RT)
			{
				a.Root->Determined(a.Undetermined, b.Determined);
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
				auto ct = ConstrainType::RT(b.Args[i].Root, a.Determined->Args.Arguments[i]);
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
				auto ct = ConstrainType::RT(a.Args[i].Root, b.Determined->Args.Arguments[i]);
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

			//Determining REF_ANY with functions is a NP-hard problem. So we
			//can only try with all possible combination at the end. 
			//Basic idea is to apply the CurrentOverload for each function here
			//if it fails, or any other checks fails because of it, the 
			//CurrentOverload will move to the next overload and repeat again.

			//First check functions with only one candidate.
			for (auto& f : cache.TraitFunctions)
			{
				if (f.Overloads.size() == 0)
				{
					return -1;
				}
				if (f.Overloads.size() == 1)
				{
					auto ret = TryDetermineEqualFunctions(f, 0);
					if (ret != 0) return ret;
				}
			}

			//Then with multiple candidates.
			//To simplify, we always apply all functions, although some
			//or most of them actually have been applied already.
			//TODO add a flag to indicate the starting point of applying.
			for (auto& f : cache.TraitFunctions)
			{
				if (f.Overloads.size() <= 1) continue;
				auto ret = TryDetermineEqualFunctions(f, f.CurrentOverload);
				if (ret != 0) return ret;
			}
		}
		default:
			return 0;
		}
	}

	//Returns 0, -1, 1.
	int TryDetermineEqualFunctions(TraitCacheFunctionInfo& f, std::size_t id)
	{
		TraitCacheFunctionOverloadInfo& overload = f.Overloads[id];
		int ret = 0;
		
		ret = TryDetermineEqualTypes(f.TraitReturnType, overload.ReturnType);
		if (ret != 0) return ret;

		assert(f.TraitParameterTypes.size() == overload.ParameterTypes.size());
		for (std::size_t i = 0; i < f.TraitParameterTypes.size(); ++i)
		{
			ret = TryDetermineEqualTypes(f.TraitParameterTypes[i], overload.ParameterTypes[i]);
			if (ret != 0) return ret;
		}

		return 0;
	}

	//Can only be used in SimplifyConstrainType.
	bool TrySimplifyConstrainType(ConstrainType& t, ConstrainType& parent)
	{
		SimplifyConstrainType(t);
		//Note that we only allow RT. EMPTY cannot be a valid argument.
		if (t.CType != CTT_RT)
		{
			if (t.CType == CTT_FAIL)
			{
				parent.DeductFail();
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
		case CTT_EMPTY:
		case CTT_FAIL:
			//Elemental type. Can't simplify.
			return;
		case CTT_ANY:
			if (auto rt = t.Root->GetDetermined(t.Undetermined))
			{
				t.DeductRT(rt);
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
				if (!CheckGenericArguments(tt->Generic, la, nullptr))
				{
					t.DeductFail();
					return;
				}
			}
			t.DeductRT(LoadTypeInternal(la, t.TryArgumentConstrain));
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
					t.DeductFail();
					return;
				}
				throw RuntimeLoaderException("Invalid subtype constrain");
			}
			if (t.TryArgumentConstrain)
			{
				auto tt = FindTypeTemplate(la);
				if (!CheckGenericArguments(tt->Generic, la, nullptr))
				{
					t.DeductFail();
					return;
				}
			}
			t.DeductRT(LoadTypeInternal(la, t.TryArgumentConstrain));
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
		if (t.CType != CTT_RT && t.CType != CTT_EMPTY)
		{
			assert(t.CType == CTT_FAIL);
			return false;
		}
		assert(t.Determined || t.CType == CTT_EMPTY);
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
		for (auto& tf : cache.TraitFields)
		{
			if (!CheckDeterminedTypesEqual(tf.Type, tf.TypeInTarget))
			{
				return false;
			}
		}

		for (auto& tf : cache.TraitFunctions)
		{
			auto& overload = tf.Overloads[tf.CurrentOverload];
			if (!CheckDeterminedTypesEqual(tf.TraitReturnType, overload.ReturnType))
			{
				return false;
			}
			assert(tf.TraitParameterTypes.size() == overload.ParameterTypes.size());
			for (std::size_t i = 0; i < tf.TraitParameterTypes.size(); ++i)
			{
				if (!CheckDeterminedTypesEqual(tf.TraitParameterTypes[i], overload.ParameterTypes[i]))
				{
					return false;
				}
			}
		}

		return true;
	}

	bool CheckDeterminedTypesEqual(ConstrainType& a, ConstrainType& b)
	{
		if (!CheckSimplifiedConstrainType(a)) return false;
		if (!CheckSimplifiedConstrainType(b)) return false;
		assert(a.Determined || a.CType == CTT_EMPTY);
		assert(b.Determined || b.CType == CTT_EMPTY);
		return a.Determined == b.Determined;
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
		//Note that for value type as we are loading interfaces to Box type, we have to
		//check template.
		if (typeChecked->Interfaces.size() > 0 || typeChecked->Storage == TSM_VALUE)
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

	RuntimeType* FindConstrainExportType(ConstrainCalculationCache* cache, const std::string& name)
	{
		if (name.length() == 0) return nullptr;
		auto& constrainName = cache->Source->ExportName;
		auto slash = name.find('/');
		if (slash == 0) return nullptr;
		if (slash == std::string::npos)
		{
			if (name == ".target")
			{
				assert(cache->Target.Determined);
				return cache->Target.Determined;
			}
			switch (cache->Source->Type)
			{
			case CONSTRAIN_TRAIT_ASSEMBLY:
			case CONSTRAIN_TRAIT_IMPORT:
				for (auto& e : cache->Trait->Types)
				{
					if (name == e.ExportName)
					{
						auto ct = ConstructConstrainTraitType(*cache, e.Index);
						SimplifyConstrainType(ct);
						assert(ct.CType == CTT_RT || ct.CType == CTT_EMPTY);
						if (ct.CType == CTT_RT)
						{
							assert(ct.Determined);
							return ct.Determined;
						}
					}
				}
				return nullptr;
			default:
				return nullptr;
			}
		}
		else
		{
			auto childName = name.substr(0, slash);
			switch (cache->Source->Type)
			{
			case CONSTRAIN_TRAIT_ASSEMBLY:
			case CONSTRAIN_TRAIT_IMPORT:
			{
				auto& constrainList = cache->Trait->Generic.Constrains;
				assert(constrainList.size() == cache->Children.size());
				for (std::size_t i = 0; i < cache->Children.size(); ++i)
				{
					if (constrainList[i].ExportName == childName)
					{
						return FindConstrainExportType(cache->Children[i].get(), name.substr(slash + 1));
					}
				}
				return nullptr;
			}
			default:
				return nullptr;
			}
		}
	}

	RuntimeFunction* FindConstrainExportFunction(ConstrainCalculationCache* cache, const std::string& name)
	{
		if (name.length() == 0) return nullptr;
		auto& constrainName = cache->Source->ExportName;
		auto slash = name.find('/');
		if (slash == 0) return nullptr;
		if (slash == std::string::npos)
		{
			switch (cache->Source->Type)
			{
			case CONSTRAIN_TRAIT_ASSEMBLY:
			case CONSTRAIN_TRAIT_IMPORT:
				for (std::size_t i = 0; i < cache->Trait->Functions.size(); ++i)
				{
					auto& e = cache->Trait->Functions[i];
					auto& tf = cache->TraitFunctions[i];
					if (name == e.ExportName)
					{
						auto index = tf.Overloads[tf.CurrentOverload].Index;
						assert(cache->Target.Determined);
						auto tt = FindTypeTemplate(cache->Target.Determined->Args);
						LoadingRefArguments lg = { cache->Target.Determined, tt->Generic };
						return LoadRefFunction(lg, index);
					}
				}
				return nullptr;
			default:
				return nullptr;
			}
		}
		else
		{
			auto childName = name.substr(0, slash);
			switch (cache->Source->Type)
			{
			case CONSTRAIN_TRAIT_ASSEMBLY:
			case CONSTRAIN_TRAIT_IMPORT:
			{
				auto& constrainList = cache->Trait->Generic.Constrains;
				assert(constrainList.size() == cache->Children.size());
				for (std::size_t i = 0; i < cache->Children.size(); ++i)
				{
					if (constrainList[i].ExportName == childName)
					{
						return FindConstrainExportFunction(cache->Children[i].get(), name.substr(slash + 1));
					}
				}
			}
			default:
				return nullptr;
			}
		}
	}

	std::size_t FindConstrainExportField(ConstrainCalculationCache* cache, const std::string& name)
	{
		if (name.length() == 0) return SIZE_MAX;
		auto& constrainName = cache->Source->ExportName;
		auto slash = name.find('/');
		if (slash == 0) return SIZE_MAX;
		if (slash == std::string::npos)
		{
			switch (cache->Source->Type)
			{
			case CONSTRAIN_TRAIT_ASSEMBLY:
			case CONSTRAIN_TRAIT_IMPORT:
				for (std::size_t i = 0; i < cache->Trait->Fields.size(); ++i)
				{
					if (name == cache->Trait->Fields[i].ExportName)
					{
						return cache->TraitFields[i].FieldIndex;
					}
				}
				return SIZE_MAX;
			default:
				return SIZE_MAX;
			}
		}
		else
		{
			auto childName = name.substr(0, slash);
			switch (cache->Source->Type)
			{
			case CONSTRAIN_TRAIT_ASSEMBLY:
			case CONSTRAIN_TRAIT_IMPORT:
			{
				auto& constrainList = cache->Trait->Generic.Constrains;
				assert(constrainList.size() == cache->Children.size());
				for (std::size_t i = 0; i < cache->Children.size(); ++i)
				{
					if (constrainList[i].ExportName == childName)
					{
						return FindConstrainExportField(cache->Children[i].get(), name.substr(slash + 1));
					}
				}
			}
			default:
				return SIZE_MAX;
			}
		}
	}
};

inline bool RuntimeLoaderCore::CheckConstrains(const std::string& srcAssebly, GenericDeclaration* g,
	const std::vector<RuntimeType*>& args, ConstrainExportList* exportList)
{
	auto l = static_cast<RuntimeLoaderConstrain*>(this);
	return l->CheckConstrainsImpl(srcAssebly, g, args, exportList);
}
