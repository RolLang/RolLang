#pragma once
#include "RuntimeLoaderRefList.h"
#include "RuntimeLoaderConstraintObjects.h"

namespace RolLang { namespace ConstraintObjects {

struct RuntimeLoaderConstraint : RuntimeLoaderRefList
{
public:
	bool CheckConstraintsImpl(const std::string& srcAssebly, GenericDeclaration* g,
		const MultiList<RuntimeType*>& args, ConstraintExportList* exportList)
	{
		ConstraintUndeterminedTypeSource ud = {};
		ConstraintCalculationCacheRoot root(ud);

		MultiList<ConstraintCheckType> cargs;
		auto& argsSize = args.GetSizeList();
		for (std::size_t i = 0; i < argsSize.size(); ++i)
		{
			cargs.NewList();
			for (std::size_t j = 0; j < argsSize[i]; ++j)
			{
				cargs.AppendLast(ConstraintCheckType::Determined(&root, args.Get(i, j)));
			}
		}

		//Not using the same root, but they share the same udList.
		return CheckConstraintsInternal(srcAssebly, g, cargs, ud, exportList);
	}

private:
	bool CheckConstraintsInternal(const std::string& srcAssebly, GenericDeclaration* g,
		MultiList<ConstraintCheckType>& cargs, ConstraintUndeterminedTypeSource& udList, ConstraintExportList* exportList)
	{
		ConstraintCalculationCacheRoot root(udList);
		for (auto& constraint : g->Constraints)
		{
			auto c = CreateConstraintCache(constraint, srcAssebly, cargs, ConstraintCheckType::Fail(&root), &root);
			if (!CheckConstraintCached(c.get()))
			{
				return false;
			}

			if (exportList != nullptr)
			{
				CalculateExportList(c.get(), g, exportList);
			}
			root.Clear();
		}
		return true;
	}

private:
	//TODO separate create+basic fields from load argument/target types (reduce # of args)
	std::unique_ptr<ConstraintCalculationCache> CreateConstraintCache(GenericConstraint& constraint,
		const std::string& srcAssebly, const MultiList<ConstraintCheckType>& args, ConstraintCheckType checkTarget,
		ConstraintCalculationCacheRoot* root)
	{
		root->Size += 1;
		//TODO check loading limit (low priority) (should include other roots)

		auto ret = std::make_unique<ConstraintCalculationCache>();
		ret->Root = root;
		ret->Source = &constraint;
		ret->SrcAssembly = srcAssebly;
		ret->CheckArguments = args;
		ret->CheckTarget = checkTarget;
		ret->Target = ConstructConstraintArgumentType(*ret.get(), constraint, constraint.Target);

		//TODO segment support
		ret->Arguments.NewList();
		for (auto a : constraint.Arguments)
		{
			ret->Arguments.AppendLast(ConstructConstraintArgumentType(*ret.get(), constraint, a));
		}

		if (constraint.Type == CONSTRAINT_TRAIT_ASSEMBLY ||
			constraint.Type == CONSTRAINT_TRAIT_IMPORT)
		{
			InitTraitConstraintCache(*ret.get());
		}
		return ret;
	}

	//Check without changing function overload candidates.
	bool CheckConstraintCachedSinglePass(ConstraintCalculationCache* cache)
	{
		while (CheckCacheContainsUndetermined(cache))
		{
			auto check = TryDetermineConstraintArgument(*cache);
			if (check == 1) continue;
			return false;
		}
		//All REF_ANY are resolved.
		if (!CheckConstraintDetermined(*cache))
		{
			return false;
		}
		return true;
	}

	bool CheckConstraintCached(ConstraintCalculationCache* cache)
	{
		do
		{
			auto id = cache->Root->StartBacktrackPoint();
			if (CheckConstraintCachedSinglePass(cache)) return true;
			cache->Root->DoBacktrack(id);
		} while (MoveToNextCandidates(cache));
		return false;
	}

	bool MoveToNextCandidates(ConstraintCalculationCache* cache)
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

	static bool IsTypeUndetermined(ConstraintCalculationCacheRoot* root, ConstraintCheckType& ct)
	{
		switch (ct.CType)
		{
		case CTT_RT:
		case CTT_EMPTY:
			return false;
		case CTT_SUBTYPE:
			if (IsTypeUndetermined(root, ct.ParentType[0])) return true;
			//fall through
		case CTT_GENERIC:
			for (auto& a : ct.Args.GetAll())
			{
				if (IsTypeUndetermined(root, a)) return true;
			}
			return false;
		case CTT_ANY:
			return !root->IsDetermined(ct.UndeterminedId);
		default:
			assert(0);
			return false;
		}
	}

	static bool CheckCacheContainsUndetermined(ConstraintCalculationCache* cache)
	{
		auto ctype = cache->Source->Type;
		if (ctype == CONSTRAINT_TRAIT_ASSEMBLY || ctype == CONSTRAINT_TRAIT_IMPORT)
		{
			//When resolving we will create some REF_ANY. Allow executing until resolved.
			if (!cache->TraitMemberResolved) return true;
		}

		ConstraintCalculationCacheRoot* root = cache->Root;
		for (auto& a : cache->Arguments.GetAll())
		{
			if (IsTypeUndetermined(root, a)) return true;
		}
		for (auto& a : cache->TraitFunctionUndetermined)
		{
			if (IsTypeUndetermined(root, a)) return true;
		}
		if (IsTypeUndetermined(root, cache->Target)) return true;
		return false;
	}

private:
	void InitTraitConstraintCache(ConstraintCalculationCache& cache)
	{
		switch (cache.Source->Type)
		{
		case CONSTRAINT_TRAIT_ASSEMBLY:
		{
			auto assembly = FindAssemblyThrow(cache.SrcAssembly);
			if (cache.Source->Index >= assembly->Traits.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid trait reference");
			}
			cache.Trait = &assembly->Traits[cache.Source->Index];
			cache.TraitAssembly = cache.SrcAssembly;
			break;
		}
		case CONSTRAINT_TRAIT_IMPORT:
		{
			auto assembly = FindAssemblyThrow(cache.SrcAssembly);
			LoadingArguments la;
			if (cache.Source->Index >= assembly->ImportTraits.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid trait reference");
			}
			if (!FindExportTrait(assembly->ImportTraits[cache.Source->Index], la))
			{
				throw RuntimeLoaderException(ERR_L_LINK, "Invalid trait reference");
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

	bool AreConstraintTypesEqual(ConstraintCheckType& a, ConstraintCheckType& b)
	{
		//TODO Probably we don't need to simplify it.

		SimplifyConstraintType(a);
		SimplifyConstraintType(b);

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
			return a.Root == b.Root && a.UndeterminedId == b.UndeterminedId;
		case CTT_RT:
			return a.DeterminedType == b.DeterminedType;
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
			if (!AreConstraintTypesEqual(a.ParentType[0], b.ParentType[0]))
			{
				return false;
			}
			break;
		default:
			assert(0);
		}

		//Unfortunately we cannot use operator== for std::vector: our comparison
		//requires non-constant reference to simplify.
		//TODO consider merge with the loop in AreConstraintsEqual
		auto& sa = a.Args.GetSizeList();
		auto& sb = b.Args.GetSizeList();
		if (sa != sb) return false;
		for (std::size_t i = 0; i < sa.size(); ++i)
		{
			for (std::size_t j = 0; j < sa[i]; ++j)
			{
				if (!AreConstraintTypesEqual(a.Args.GetRef(i, j), b.Args.GetRef(i, j))) return false;
			}
		}

		return true;
	}

	bool AreConstraintsEqual(ConstraintCalculationCache& a, ConstraintCalculationCache& b)
	{
		if (a.Source != b.Source) return false;
		auto&& sa = a.CheckArguments.GetSizeList();
		auto&& sb = b.CheckArguments.GetSizeList();
		if (sa != sb)
		{
			return false;
		}
		for (std::size_t i = 0; i < sa.size(); ++i)
		{
			for (std::size_t j = 0; j < sa[i]; ++j)
			{
				if (!AreConstraintTypesEqual(a.CheckArguments.GetRef(i, j), b.CheckArguments.GetRef(i, j)))
				{
					return false;
				}
			}
		}
		return true;
	}

	void EnsureSubConstraintCached(ConstraintCalculationCache& parent)
	{
		auto trait = parent.Trait;
		auto g = &trait->Generic;

		if (parent.TraitCacheCreated)
		{
			assert(parent.Children.size() == g->Constraints.size());
			assert(parent.TraitFields.size() == trait->Fields.size());
			assert(parent.TraitFunctions.size() == trait->Functions.size());
			return;
		}
		assert(parent.Children.size() == 0);
		assert(parent.TraitFields.size() == 0);
		assert(parent.TraitFunctions.size() == 0);
		assert(!parent.TraitMemberResolved);

		//Children (sub-constraints)
		if (!g->ParameterCount.CanMatch(parent.Arguments.GetSizeList()))
		{
			throw RuntimeLoaderException(ERR_L_GENERIC, "Invalid generic arguments");
		}
		for (auto& constraint : g->Constraints)
		{
			parent.Children.emplace_back(CreateConstraintCache(constraint, parent.TraitAssembly,
				parent.Arguments, parent.Target, parent.Root));
			auto ptr = parent.Children.back().get();
			ptr->Parent = &parent;

			//Check circular constraint.
			//Note that, same as what we do elsewhere in this project, 
			//we only need to check trait-trait constraint loop.
			//Trait-type or trait-function circular loop will trigger another
			//trait-trait, type-type or function-function circular check.

			//I have no better idea but to simplify and check.
			ConstraintCalculationCache* p = &parent;
			while (p != nullptr)
			{
				if (AreConstraintsEqual(*p, *ptr))
				{
					//Circular constraint is always considered as a program error.
					throw RuntimeLoaderException(ERR_L_CIRCULAR, "Circular constraint check");
				}
				p = p->Parent;
			}
		}

		//Fields
		for (auto& field : trait->Fields)
		{
			parent.TraitFields.push_back({ ConstructConstraintTraitType(parent, field.Type), {}, 0 });
		}

		//Functions
		for (auto& func : trait->Functions)
		{
			TraitCacheFunctionInfo func_info = {};
			func_info.TraitReturnType = ConstructConstraintTraitType(parent, func.ReturnType);
			for (auto p : func.ParameterTypes)
			{
				func_info.TraitParameterTypes.push_back(ConstructConstraintTraitType(parent, p));
			}
			parent.TraitFunctions.emplace_back(std::move(func_info));
		}

		parent.TraitMemberResolved = false;
		parent.TraitCacheCreated = true;
	}

	//ret 1: all members successfully resolved. 0: cannot resolve (not determined). -1: constraint fails
	int TryCalculateTraitSubMember(ConstraintCalculationCache& parent)
	{
		assert(parent.TraitCacheCreated);

		if (parent.TraitMemberResolved) return 1;
		auto trait = parent.Trait;

		SimplifyConstraintType(parent.Target);
		if (parent.Target.CType != CTT_RT && parent.Target.CType != CTT_EMPTY)
		{
			return 0;
		}

		auto target = parent.Target.DeterminedType;
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
				//passes its constraint check, which means we can simply use LoadRefType.
				//Note that we may still have constraint check failure when loading field
				//types, but that is considered as a program error instead of constraint
				//check failure of this constraint we are testing, and we can simply let 
				//it throws.

				auto type_id = tt->Fields[fid];
				auto field_type = LoadRefType({ target, tt->Generic }, type_id);
				parent.TraitFields[i].TypeInTarget = ConstraintCheckType::Determined(parent.Root, field_type);
			}
			else
			{
				parent.TraitFields[i].TypeInTarget = 
					ConstraintCheckType::Determined(parent.Root, target->Fields[fid].Type);
			}
		}

		std::vector<ConstraintCheckType> ud;
		for (std::size_t i = 0; i < trait->Functions.size(); ++i)
		{
			//Search in public function list.
			for (auto& func : tt->PublicFunctions)
			{
				if (func.Name != trait->Functions[i].ElementName) continue;
				TraitCacheFunctionOverloadInfo fi = {};
				fi.Index = func.Id;
				ud.clear();
				if (!CheckTraitTargetFunctionOverload(parent, i, tt, ud, fi))
				{
					continue;
				}
				parent.TraitFunctions[i].Overloads.emplace_back(std::move(fi));
				parent.TraitFunctionUndetermined.insert(parent.TraitFunctionUndetermined.end(),
					ud.begin(), ud.end());
			}
			//Search in type virtual function table.
			for (auto& func : tt->Base.VirtualFunctions)
			{
				if (func.Name != trait->Functions[i].ElementName) continue;
				TraitCacheFunctionOverloadInfo fi = {};
				//Bind to the virtual version.
				fi.Index = func.VirtualFunction;
				ud.clear();
				if (!CheckTraitTargetFunctionOverload(parent, i, tt, ud, fi))
				{
					continue;
				}
				parent.TraitFunctions[i].Overloads.emplace_back(std::move(fi));
				parent.TraitFunctionUndetermined.insert(parent.TraitFunctionUndetermined.end(),
					ud.begin(), ud.end());
			}
			//Search in interface virtual function table.
			for (auto& interfaceInfo : tt->Interfaces)
			{
				for (auto& func : interfaceInfo.VirtualFunctions)
				{
					if (func.Name != trait->Functions[i].ElementName) continue;
					TraitCacheFunctionOverloadInfo fi = {};
					//Bind to the virtual version.
					fi.Index = func.VirtualFunction;
					ud.clear();
					if (!CheckTraitTargetFunctionOverload(parent, i, tt, ud, fi))
					{
						continue;
					}
					parent.TraitFunctions[i].Overloads.emplace_back(std::move(fi));
					parent.TraitFunctionUndetermined.insert(parent.TraitFunctionUndetermined.end(),
						ud.begin(), ud.end());
				}
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

	bool CheckTraitTargetFunctionOverload(ConstraintCalculationCache& parent, std::size_t i, Type* tt,
		std::vector<ConstraintCheckType>& ud, TraitCacheFunctionOverloadInfo& fi)
	{
		auto target = parent.Target.DeterminedType;
		auto trait = parent.Trait;
		auto& f = trait->Functions[i];
		if (!LoadTraitFunctionCacheInfo(parent, tt->Generic, target->Args.Assembly, fi, ud))
		{
			return false;
		}
		//TODO handle parameter pack
		if (fi.ParameterTypes.size() != f.ParameterTypes.size())
		{
			return false;
		}
		if (!CheckTypePossiblyEqual(fi.ReturnType, parent.TraitFunctions[i].TraitReturnType))
		{
			return false;
		}
		for (std::size_t k = 0; k < fi.ParameterTypes.size(); ++k)
		{
			if (!CheckTypePossiblyEqual(fi.ParameterTypes[k],
				parent.TraitFunctions[i].TraitParameterTypes[k]))
			{
				return false;
			}
		}
		return true;
	}

	bool LoadTraitFunctionCacheInfo(ConstraintCalculationCache& parent, GenericDeclaration& g,
		const std::string& src_assembly, TraitCacheFunctionOverloadInfo& result,
		std::vector<ConstraintCheckType>& additionalUd)
	{
		assert(&g == &FindTypeTemplate(parent.Target.DeterminedType->Args)->Generic);

		auto target = parent.Target.DeterminedType;
		assert(target);

		auto id = result.Index;
		if (id >= g.Functions.size())
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
		}

		//First resolve any REF_CLONE so that we don't need to worry it any more.
		//TODO detect circular REF_CLONE
		while (g.Functions[id].Type == REF_CLONE)
		{
			id = g.Functions[id].Index;
			if (id >= g.Functions.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
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
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
			}
			auto i = a->ImportFunctions[g.Functions[id].Index];
			if (!FindExportFunction(i, la))
			{
				throw RuntimeLoaderException(ERR_L_LINK, "Import function not found");
			}
			break;
		}
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
		}

		//About the arguments:
		//We are in a type. Its Functions[id] specify a function. The argument to the
		//function is the arg list in the RefList plus some additional REF_ANY.
		//To calculate those in the RefList, we need the args to the type itself.

		auto additional = GetFunctionAdditionalArgumentNumber(g, id);

		MultiList<ConstraintCheckType> typeArgs;
		target->Args.Arguments.CopyList(typeArgs, [&parent](RuntimeType* ta)
		{
			return ConstraintCheckType::Determined(parent.Root, ta);
		});
		//Note that additional arguments are appended to type arguments.
		for (std::size_t i = 0; i < target->Args.Arguments.GetSizeList().size(); ++i)
		{
			if (additional[i] > target->Args.Arguments.GetSizeList()[i])
			{
				throw RuntimeLoaderException(ERR_L_GENERIC, "Invalid function reference");
			}
		}
		for (std::size_t i = target->Args.Arguments.GetSizeList().size(); i < additional.size(); ++i)
		{
			typeArgs.NewList();
			for (std::size_t j = 0; j < additional[i]; ++j)
			{
				auto t = ConstraintCheckType::Undetermined(parent.Root);
				typeArgs.AppendLast(t);
				additionalUd.emplace_back(t);
			}
		}

		MultiList<ConstraintCheckType> funcArgs;
		auto& type_assembly = parent.Target.DeterminedType->Args.Assembly;
		for (auto&& e : GetRefArgList(g.Functions, id, funcArgs))
		{
			assert(e.Entry.Type == REF_CLONETYPE);
			funcArgs.AppendLast(ConstructConstraintRefListType(parent.Root,
				g, type_assembly, e.Entry.Index, typeArgs, target, nullptr));
		}

		Function* ft = FindFunctionTemplate(la.Assembly, la.Id);
		if (!ft->Generic.ParameterCount.CanMatch(funcArgs.GetSizeList()))
		{
			//Argument count mismatch.
			return false;
		}

		//Construct ConstraintCheckType for ret and params.
		result.ReturnType = ConstructConstraintRefListType(parent.Root, ft->Generic,
			la.Assembly, ft->ReturnValue.TypeId, funcArgs, nullptr, &result.ExportTypes);
		for (auto& parameter : ft->Parameters)
		{
			result.ParameterTypes.emplace_back(ConstructConstraintRefListType(parent.Root, ft->Generic,
				la.Assembly, parameter.TypeId, funcArgs, nullptr, &result.ExportTypes));
		}

		result.FunctionTemplate = ft;
		result.GenericArguments = std::move(funcArgs);
		result.FunctionTemplateAssembly = la.Assembly;

		return true;
	}

	//Scan the function reference. Make sure it's valid. Return the total number of args needed.
	std::vector<std::size_t> GetFunctionAdditionalArgumentNumber(GenericDeclaration& g, std::size_t id)
	{
		std::vector<std::size_t> ret;
		GetFunctionAdditionalArgumentNumberInternal(g, id, ret);
		return ret;
	}

	//TODO consider move to RefList
	void GetFunctionAdditionalArgumentNumberInternal(GenericDeclaration& g, std::size_t id,
		std::vector<std::size_t>& result)
	{
		if (id >= g.Functions.size())
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
		}
		std::size_t ret = 0;
		switch (g.Functions[id].Type & REF_REFTYPES)
		{
		case REF_CLONE:
			//TODO detect circular REF_CLONE
			GetFunctionAdditionalArgumentNumberInternal(g, g.Functions[id].Index, result);
			break;
		case REF_ASSEMBLY:
		case REF_IMPORT:
		{
			MultiList<int> notUsed;
			for (auto&& e : GetRefArgList(g.Functions, id, notUsed))
			{
				GetTypeAdditionalArgumentNumberInternal(g, e.Entry.Index, result);
			}
			break;
		}
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
		}
	}

	//TODO consider move to RefList
	void GetTypeAdditionalArgumentNumberInternal(GenericDeclaration& g, std::size_t id,
		std::vector<std::size_t>& result)
	{
		if (id >= g.Types.size())
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
		}
		std::size_t ret = 0;
		auto t = g.Types[id];
		switch (t.Type & REF_REFTYPES)
		{
		case REF_CLONE:
			//TODO detect circular REF_CLONE
			GetTypeAdditionalArgumentNumberInternal(g, t.Index, result);
			break;
		case REF_ASSEMBLY:
		case REF_IMPORT:
		{
			MultiList<int> notUsed;
			for (auto&& e : GetRefArgList(g.Types, id, notUsed))
			{
				GetTypeAdditionalArgumentNumberInternal(g, e.Index, result);
			}
			break;
		}
		case REF_SUBTYPE:
		{
			MultiList<int> notUsed;
			GetTypeAdditionalArgumentNumberInternal(g, id + 1, result);
			for (auto&& e : GetRefArgList(g.Types, id + 1, notUsed))
			{
				GetTypeAdditionalArgumentNumberInternal(g, e.Index, result);
			}
			break;
		}
		case REF_ARGUMENT:
		{
			if (id + 1 >= g.Types.size() || g.Types[id + 1].Type != REF_ARGUMENTSEG)
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid RefList entry");
			}
			auto seg = g.Types[id + 1].Index;
			auto i = g.Types[id].Index;
			while (result.size() <= seg)
			{
				result.push_back(0);
			}
			if (i + 1 > result[seg])
			{
				result[seg] = i + 1;
			}
			break;
		}
		case REF_SELF:
		case REF_CONSTRAINT:
		case REF_EMPTY:
			break;
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
		}
	}

	ConstraintCheckType ConstructConstraintRefListType(ConstraintCalculationCacheRoot* root, GenericDeclaration& g,
		const std::string& src, std::size_t i, MultiList<ConstraintCheckType>& arguments, RuntimeType* selfType,
		std::vector<TraitCacheFunctionConstrainExportInfo>* exportList)
	{
		if (i >= g.Types.size())
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
		}
		while (g.Types[i].Type == REF_CLONE)
		{
			i = g.Types[i].Index;
			if (i >= g.Types.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
			}
		}
		switch (g.Types[i].Type & REF_REFTYPES)
		{
		case REF_EMPTY:
			return ConstraintCheckType::Empty(root);
		case REF_ARGUMENT:
			return GetRefArgument(g.Types, i, arguments);
		case REF_SELF:
			if (selfType != nullptr)
			{
				return ConstraintCheckType::Determined(root, selfType);
			}
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
			return ConstraintCheckType::Fail(root);
		case REF_ASSEMBLY:
		{
			auto ret = ConstraintCheckType::Generic(root, src, g.Types[i].Index);
			for (auto&& e : GetRefArgList(g.Types, i, ret.Args))
			{
				ret.Args.AppendLast(ConstructConstraintRefListType(ret.Root,
					g, src, e.Index, arguments, selfType, exportList));
			}
			return ret;
		}
		case REF_IMPORT:
		{
			LoadingArguments la;
			auto a = FindAssemblyThrow(src);
			if (g.Types[i].Index >= a->ImportTypes.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
			}
			auto import_info = a->ImportTypes[g.Types[i].Index];
			if (!FindExportType(import_info, la))
			{
				throw RuntimeLoaderException(ERR_L_LINK, "Import type not found");
			}
			auto ret = ConstraintCheckType::Generic(root, la.Assembly, la.Id);
			for (auto&& e : GetRefArgList(g.Types, i, ret.Args))
			{
				ret.Args.AppendLast(ConstructConstraintRefListType(ret.Root,
					g, src, e.Index, arguments, selfType, exportList));
			}
			return ret;
		}
		case REF_SUBTYPE:
		{
			auto ret = ConstraintCheckType::Subtype(root, g.NamesList[g.Types[i].Index]);
			ret.ParentType.emplace_back(ConstructConstraintRefListType(ret.Root, g, src, i + 1,
				arguments, selfType, exportList));
			for (auto&& e : GetRefArgList(g.Types, i + 1, ret.Args))
			{
				ret.Args.AppendLast(ConstructConstraintRefListType(ret.Root,
					g, src, e.Index, arguments, selfType, exportList));
			}
			return ret;
		}
		case REF_CONSTRAINT:
		{
			if (selfType != nullptr)
			{
				//Calling for determined type. Calculate REF_CONSTRAINT.
				for (auto& ct : selfType->ConstraintExportList)
				{
					if (ct.EntryType == CONSTRAINT_EXPORT_TYPE && ct.Index == g.Types[i].Index)
					{
						return ConstraintCheckType::Determined(root, ct.Type);
					}
				}
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid REF_CONSTRAINT reference");
			}
			else
			{
				assert(exportList != nullptr);
				for (auto& ct : *exportList)
				{
					if (ct.NameIndex == g.Types[i].Index)
					{
						return ct.UndeterminedType;
					}
				}
				auto newType = ConstraintCheckType::Undetermined(root);
				exportList->push_back({ i, newType });
				return newType;
			}
		}
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
		}
	}

	ConstraintCheckType ConstructConstraintTraitType(ConstraintCalculationCache& cache, std::size_t i)
	{
		auto trait = cache.Trait;
		assert(trait);
		auto& list = trait->Generic.Types;
		auto& t = list[i];

		switch (t.Type & REF_REFTYPES)
		{
		case REF_CLONE:
			//TODO detect circular REF_CLONE
			return ConstructConstraintTraitType(cache, t.Index);
		case REF_ARGUMENT:
			return GetRefArgument(list, i, cache.Arguments);
		case REF_SELF:
			return cache.Target;
		case REF_ASSEMBLY:
		{
			auto ret = ConstraintCheckType::Generic(cache.Root, cache.TraitAssembly, t.Index);
			for (auto&& e : GetRefArgList(list, i, ret.Args))
			{
				ret.Args.AppendLast(ConstructConstraintTraitType(cache, e.Index));
			}
			return ret;
		}
		case REF_IMPORT:
		{
			auto assembly = FindAssemblyThrow(cache.TraitAssembly);
			if (t.Index > assembly->ImportTypes.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
			}
			LoadingArguments la;
			if (!FindExportType(assembly->ImportTypes[t.Index], la))
			{
				throw RuntimeLoaderException(ERR_L_LINK, "Invalid type reference");
			}
			auto ret = ConstraintCheckType::Generic(cache.Root, la.Assembly, la.Id);
			for (auto&& e : GetRefArgList(list, i, ret.Args))
			{
				ret.Args.AppendLast(ConstructConstraintTraitType(cache, e.Index));
			}
			return ret;
		}
		case REF_SUBTYPE:
		{
			if (t.Index > trait->Generic.NamesList.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
			}
			auto ret = ConstraintCheckType::Subtype(cache.Root, trait->Generic.NamesList[t.Index]);
			ret.ParentType.emplace_back(ConstructConstraintTraitType(cache, i + 1));
			for (auto&& e : GetRefArgList(list, i + 1, ret.Args))
			{
				ret.Args.AppendLast(ConstructConstraintTraitType(cache, e.Index));
			}
			return ret;
		}
		case REF_EMPTY:
			return ConstraintCheckType::Empty(cache.Root);
		case REF_LISTEND:
		case REF_ANY:
		case REF_TRY:
		case REF_CONSTRAINT:
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
		}
	}

	ConstraintCheckType ConstructConstraintArgumentType(ConstraintCalculationCache& cache,
		GenericConstraint& constraint, std::size_t i)
	{
		auto& list = constraint.TypeReferences;
		auto& t = list[i];
		switch (t.Type & REF_REFTYPES)
		{
		case REF_ANY:
			return ConstraintCheckType::Undetermined(cache.Root);
		case REF_TRY:
			return ConstraintCheckType::Try(ConstructConstraintArgumentType(cache, constraint, t.Index));
		case REF_CLONE:
			//TODO detect circular REF_CLONE
			return ConstructConstraintArgumentType(cache, constraint, t.Index);
		case REF_ARGUMENT:
			return GetRefArgument(list, i, cache.CheckArguments);
		case REF_SELF:
			if (cache.CheckTarget.CType == CTT_FAIL)
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid use of REF_SELF");
			}
			return cache.CheckTarget;
		case REF_ASSEMBLY:
		{
			auto ret = ConstraintCheckType::Generic(cache.Root, cache.SrcAssembly, t.Index);
			for (auto&& e : GetRefArgList(list, i, ret.Args))
			{
				ret.Args.AppendLast(ConstructConstraintArgumentType(cache, constraint, e.Index));
			}
			return ret;
		}
		case REF_IMPORT:
		{
			auto assembly = FindAssemblyThrow(cache.SrcAssembly);
			if (t.Index > assembly->ImportTypes.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
			}
			LoadingArguments la;
			if (!FindExportType(assembly->ImportTypes[t.Index], la))
			{
				throw RuntimeLoaderException(ERR_L_LINK, "Invalid type reference");
			}
			auto ret = ConstraintCheckType::Generic(cache.Root, la.Assembly, la.Id);
			for (auto&& e : GetRefArgList(list, i, ret.Args))
			{
				ret.Args.AppendLast(ConstructConstraintArgumentType(cache, constraint, e.Index));
			}
			return ret;
		}
		case REF_SUBTYPE:
		{
			if (t.Index > constraint.NamesList.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
			}
			auto ret = ConstraintCheckType::Subtype(cache.Root, constraint.NamesList[t.Index]);
			ret.ParentType.push_back(ConstructConstraintArgumentType(cache, constraint, i + 1));
			for (auto&& e : GetRefArgList(list, i + 1, ret.Args))
			{
				ret.Args.AppendLast(ConstructConstraintArgumentType(cache, constraint, e.Index));
			}
			return ret;
		}
		case REF_CONSTRAINT:
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
		}
	}
	
	bool CheckTypePossiblyEqual(ConstraintCheckType& a, ConstraintCheckType& b)
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
			return a.DeterminedType == b.DeterminedType;
		}
		if (a.CType == CTT_GENERIC && b.CType == CTT_GENERIC)
		{
			auto sa = a.Args.GetSizeList();
			auto sb = b.Args.GetSizeList();
			if (a.TypeTemplateAssembly != b.TypeTemplateAssembly ||
				a.TypeTemplateIndex != b.TypeTemplateIndex ||
				sa != sb) //TODO support for variable-sized
			{
				return false;
			}
			for (std::size_t i = 0; i < sa.size(); ++i)
			{
				for (std::size_t j = 0; j < sa[i]; ++j)
				{
					if (!CheckTypePossiblyEqual(a.Args.GetRef(i, j), b.Args.GetRef(i, j))) return false;
				}
			}
			return true;
		}
		else if (a.CType == CTT_RT)
		{
			auto& sa = a.DeterminedType->Args.Arguments.GetSizeList();
			auto& sb = b.Args.GetSizeList();
			if (a.DeterminedType->Args.Assembly != b.TypeTemplateAssembly ||
				a.DeterminedType->Args.Id != b.TypeTemplateIndex ||
				sa != sb) //TODO support for variable-sized
			{
				return false;
			}
			for (std::size_t i = 0; i < sa.size(); ++i)
			{
				for (std::size_t j = 0; j < sa[i]; ++j)
				{
					auto ct = ConstraintCheckType::Determined(b.Root, a.DeterminedType->Args.Arguments.Get(i, j));
					if (!CheckTypePossiblyEqual(b.Args.GetRef(i, j), ct)) return false;
				}
			}
			return true;
		}
		else //b.CType == CTT_RT
		{
			return CheckTypePossiblyEqual(b, a);
		}
	}

	//ret: 1: determined something. 0: no change. -1: impossible (constraint check fails).
	int TryDetermineEqualTypes(ConstraintCheckType& a, ConstraintCheckType& b)
	{
		//Should not modify a or b except for calling SimplifyConstraintType at the beginning.

		SimplifyConstraintType(a);
		SimplifyConstraintType(b);
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
				b.Root->Determine(b.UndeterminedId, a.DeterminedType);
				return 1;
			}
			else if (b.CType == CTT_RT)
			{
				a.Root->Determine(a.UndeterminedId, b.DeterminedType);
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
			if (a.DeterminedType != b.DeterminedType) return -1;
			return 0;
		}
		if (a.CType == CTT_GENERIC && b.CType == CTT_GENERIC)
		{
			auto& sa = a.Args.GetSizeList();
			auto& sb = b.Args.GetSizeList();
			if (a.TypeTemplateAssembly != b.TypeTemplateAssembly ||
				a.TypeTemplateIndex != b.TypeTemplateIndex ||
				sa != sb) //TODO support for variable-sized
			{
				return -1;
			}
			for (std::size_t i = 0; i < sa.size(); ++i)
			{
				for (std::size_t j = 0; j < sa[i]; ++j)
				{
					int r = TryDetermineEqualTypes(a.Args.GetRef(i, j), b.Args.GetRef(i, j));
					if (r != 0) return r;
				}
			}
			return 0;
		}
		else if (a.CType == CTT_RT)
		{
			auto& sa = a.DeterminedType->Args.Arguments.GetSizeList();
			auto& sb = b.Args.GetSizeList();
			if (a.DeterminedType->Args.Assembly != b.TypeTemplateAssembly ||
				a.DeterminedType->Args.Id != b.TypeTemplateIndex ||
				sa != sb) //TODO support for variable-sized
			{
				return -1;
			}
			for (std::size_t i = 0; i < sa.size(); ++i)
			{
				for (std::size_t j = 0; j < sa[i]; ++j)
				{
					auto ct = ConstraintCheckType::Determined(b.Root, a.DeterminedType->Args.Arguments.Get(i, j));
					int r = TryDetermineEqualTypes(b.Args.GetRef(i, j), ct);
					if (r != 0) return r;
				}
			}
			return 0;
		}
		else //b.CType == CTT_RT
		{
			return TryDetermineEqualTypes(b, a);
		}
	}

	//Return 0, 1, or -1 (see TryDetermineEqualTypes)
	int TryDetermineConstraintArgument(ConstraintCalculationCache& cache)
	{
		switch (cache.Source->Type)
		{
		case CONSTRAINT_EXIST:
		case CONSTRAINT_BASE:
		case CONSTRAINT_INTERFACE:
			return 0;
		case CONSTRAINT_SAME:
			if (!cache.Arguments.IsSingle())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid constraint arguments");
			}
			return TryDetermineEqualTypes(cache.Arguments.GetRef(0, 0), cache.Target);
		case CONSTRAINT_TRAIT_ASSEMBLY:
		case CONSTRAINT_TRAIT_IMPORT:
		{
			EnsureSubConstraintCached(cache);
			if (!cache.TraitMemberResolved)
			{
				return TryCalculateTraitSubMember(cache);
			}

			auto trait = cache.Trait;
			auto target = cache.Target.DeterminedType;
			assert(target);

			//Note that we create cache for sub-constraints but do not use it
			//for determining REF_ANY. This is because linked traits with any 
			//type can easily lead to infinite constraint chain, which is not 
			//circular (because of the new REF_ANY) and difficult to check.
			//We simplify the situation by not checking it. Because of the 
			//undetermined REF_ANY, the constraint will fail at the parent level.
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

			return 0;
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

	//Can only be used in SimplifyConstraintType.
	bool TrySimplifyConstraintType(ConstraintCheckType& t, ConstraintCheckType& parent)
	{
		SimplifyConstraintType(t);
		//Note that we only allow RT. EMPTY cannot be a valid argument.
		if (t.CType != CTT_RT)
		{
			if (t.CType == CTT_FAIL)
			{
				parent.DeductFail();
			}
			return false;
		}
		assert(t.DeterminedType);
		return true;
	}

	void SimplifyConstraintType(ConstraintCheckType& t)
	{
		switch (t.CType)
		{
		case CTT_RT:
		case CTT_EMPTY:
		case CTT_FAIL:
			//Elemental type. Can't simplify.
			return;
		case CTT_ANY:
			if (auto rt = t.Root->GetDetermined(t.UndeterminedId))
			{
				t.DeductRT(rt);
			}
			return;
		case CTT_GENERIC:
		{
			LoadingArguments la = { t.TypeTemplateAssembly, t.TypeTemplateIndex };
			bool breakFlag = false;
			t.Args.CopyList(la.Arguments, [&breakFlag, &t, this](ConstraintCheckType& arg)
			{
				if (breakFlag || !TrySimplifyConstraintType(arg, t))
				{
					breakFlag = true;
					return (RuntimeType*)nullptr;
				}
				return arg.DeterminedType;
			});
			if (breakFlag) return;
			if (t.TryArgumentConstraint)
			{
				auto tt = FindTypeTemplate(la);
				if (!CheckTypeGenericArguments(tt->Generic, la, nullptr))
				{
					//TODO support REF_EMPTY
					t.DeductFail();
					return;
				}
			}
			t.DeductRT(LoadTypeInternal(la, t.TryArgumentConstraint));
			return;
		}
		case CTT_SUBTYPE:
		{
			SubMemberLoadingArguments lg;
			assert(t.ParentType.size() == 1);
			if (!TrySimplifyConstraintType(t.ParentType[0], t))
			{
				return;
			}
			lg = { t.ParentType[0].DeterminedType, t.SubtypeName };
			bool breakFlag = false;
			t.Args.CopyList(lg.Arguments, [&breakFlag, &t, this](ConstraintCheckType& arg)
			{
				if (breakFlag || !TrySimplifyConstraintType(arg, t))
				{
					breakFlag = true;
					return (RuntimeType*)nullptr;
				}
				return arg.DeterminedType;
			});
			if (breakFlag) return;

			LoadingArguments la;
			if (!FindSubType(lg, la))
			{
				//TODO support REF_EMPTY
				if (t.TryArgumentConstraint)
				{
					t.DeductFail();
					return;
				}
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid subtype constraint");
			}
			if (t.TryArgumentConstraint)
			{
				auto tt = FindTypeTemplate(la);
				if (!CheckTypeGenericArguments(tt->Generic, la, nullptr))
				{
					//TODO support REF_EMPTY
					t.DeductFail();
					return;
				}
			}
			t.DeductRT(LoadTypeInternal(la, t.TryArgumentConstraint));
			return;
		}
		default:
			assert(0);
			break;
		}
	}

	bool CheckSimplifiedConstraintType(ConstraintCheckType& t)
	{
		SimplifyConstraintType(t);
		if (t.CType != CTT_RT && t.CType != CTT_EMPTY)
		{
			assert(t.CType == CTT_FAIL);
			return false;
		}
		assert(t.DeterminedType || t.CType == CTT_EMPTY);
		return true;
	}

	bool CheckTraitDetermined(ConstraintCalculationCache& cache)
	{
		EnsureSubConstraintCached(cache);
		if (TryCalculateTraitSubMember(cache) != 1)
		{
			//Resolving submember only requires Target to be determined,
			//which should be success if it goes here.
			return false;
		}

		//Sub-constraints in trait
		for (auto& subconstraint : cache.Children)
		{
			//Not guaranteed to be determined, and we also need to calculate exports,
			//so use CheckConstraintCached.
			if (!CheckConstraintCached(subconstraint.get()))
			{
				return false;
			}
		}

		auto target = cache.Target.DeterminedType;
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

			//Match function types (return & parameters).
			//Note that there might still be undetermined (because of how we resolve REF_CONSTRAINT).
			if (!CheckTypePossiblyEqual(tf.TraitReturnType, overload.ReturnType))
			{
				return false;
			}
			assert(tf.TraitParameterTypes.size() == overload.ParameterTypes.size());
			for (std::size_t i = 0; i < tf.TraitParameterTypes.size(); ++i)
			{
				if (!CheckTypePossiblyEqual(tf.TraitParameterTypes[i], overload.ParameterTypes[i]))
				{
					return false;
				}
			}

			//Simplify generic arguments before checking constraints.
			for (auto& t : overload.GenericArguments.GetAll())
			{
				SimplifyConstraintType(t);
			}

			//Check function template constrains.
			ConstraintExportList exportList;
			if (!CheckConstraintsInternal(overload.FunctionTemplateAssembly, &overload.FunctionTemplate->Generic,
				overload.GenericArguments, *cache.Root->UndeterminedList, &exportList))
			{
				return false;
			}

			//Make REF_CONSTRAINT types equal to the export list we got from the constraint check.
			for (auto& ct : overload.ExportTypes)
			{
				for (auto& et : exportList)
				{
					if (et.EntryType == CONSTRAINT_EXPORT_TYPE && et.Index == ct.NameIndex)
					{
						assert(ct.UndeterminedType.CType == CTT_ANY);
						ct.UndeterminedType.Root->Determine(ct.UndeterminedType.UndeterminedId, et.Type);
						break;
					}
				}
			}

			//Final check (now there should be no REF_ANY).
			if (!CheckDeterminedTypesEqual(tf.TraitReturnType, overload.ReturnType))
			{
				return false;
			}
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

	bool CheckDeterminedTypesEqual(ConstraintCheckType& a, ConstraintCheckType& b)
	{
		if (!CheckSimplifiedConstraintType(a)) return false;
		if (!CheckSimplifiedConstraintType(b)) return false;
		assert(a.DeterminedType || a.CType == CTT_EMPTY);
		assert(b.DeterminedType || b.CType == CTT_EMPTY);
		return a.DeterminedType == b.DeterminedType;
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

	bool CheckConstraintDetermined(ConstraintCalculationCache& cache)
	{
		switch (cache.Source->Type)
		{
		case CONSTRAINT_EXIST:
			//TODO should be IsEmpty (after supporting multilist)
			if (cache.Arguments.GetTotalSize() != 0)
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid constraint arguments");
			}
			return CheckSimplifiedConstraintType(cache.Target);
		case CONSTRAINT_SAME:
			if (!cache.Arguments.IsSingle())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid constraint arguments");
			}
			if (!CheckSimplifiedConstraintType(cache.Target) ||
				!CheckSimplifiedConstraintType(cache.Arguments.GetRef(0, 0)))
			{
				return false;
			}
			return cache.Target.DeterminedType == cache.Arguments.GetRef(0, 0).DeterminedType;
		case CONSTRAINT_BASE:
			if (!cache.Arguments.IsSingle())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid constraint arguments");
			}
			if (!CheckSimplifiedConstraintType(cache.Target) ||
				!CheckSimplifiedConstraintType(cache.Arguments.GetRef(0, 0)))
			{
				return false;
			}
			return CheckLoadingTypeBase(cache.Target.DeterminedType, cache.Arguments.GetRef(0, 0).DeterminedType);
		case CONSTRAINT_INTERFACE:
			if (!cache.Arguments.IsSingle())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid constraint arguments");
			}
			if (!CheckSimplifiedConstraintType(cache.Target) ||
				!CheckSimplifiedConstraintType(cache.Arguments.GetRef(0, 0)))
			{
				return false;
			}
			return CheckLoadingTypeInterface(cache.Target.DeterminedType, cache.Arguments.GetRef(0, 0).DeterminedType);
		case CONSTRAINT_TRAIT_ASSEMBLY:
		case CONSTRAINT_TRAIT_IMPORT:
			return CheckTraitDetermined(cache);
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid constraint type");
		}
	}

	void CalculateExportList(ConstraintCalculationCache* cache, GenericDeclaration* g, ConstraintExportList* exportList)
	{
		auto prefix = cache->Source->ExportName + "/";

		//Export types
		for (std::size_t i = 0; i < g->Types.size(); ++i)
		{
			if ((g->Types[i].Type & REF_REFTYPES) != REF_CONSTRAINT) continue;
			auto& name = g->NamesList[g->Types[i].Index];
			if (name.compare(0, prefix.length(), prefix) == 0)
			{
				auto type = FindConstraintExportType(cache, name.substr(prefix.length()));
				if (type != nullptr)
				{
					ConstraintExportListEntry entry;
					entry.EntryType = CONSTRAINT_EXPORT_TYPE;
					entry.Index = i;
					entry.Type = type;
					exportList->push_back(entry);
				}
			}
		}

		//Export functions
		for (std::size_t i = 0; i < g->Functions.size(); ++i)
		{
			if ((g->Functions[i].Type & REF_REFTYPES) != REF_CONSTRAINT) continue;
			auto& name = g->NamesList[g->Functions[i].Index];
			if (name.compare(0, prefix.length(), prefix) == 0)
			{
				auto func = FindConstraintExportFunction(cache, name.substr(prefix.length()));
				if (func != nullptr)
				{
					ConstraintExportListEntry entry;
					entry.EntryType = CONSTRAINT_EXPORT_FUNCTION;
					entry.Index = i;
					entry.Function = func;
					exportList->push_back(entry);
				}
			}
		}

		//Export field
		for (std::size_t i = 0; i < g->Fields.size(); ++i)
		{
			if ((g->Fields[i].Type & REF_REFTYPES) != REF_CONSTRAINT) continue;
			auto& name = g->NamesList[g->Fields[i].Index];
			if (name.compare(0, prefix.length(), prefix) == 0)
			{
				auto field = FindConstraintExportField(cache, name.substr(prefix.length()));
				if (field != SIZE_MAX)
				{
					ConstraintExportListEntry entry;
					entry.EntryType = CONSTRAINT_EXPORT_FIELD;
					entry.Index = i;
					entry.Field = field;
					exportList->push_back(entry);
				}
			}
		}
	}

	RuntimeType* FindConstraintExportType(ConstraintCalculationCache* cache, const std::string& name)
	{
		if (name.length() == 0) return nullptr;
		auto& constraintName = cache->Source->ExportName;
		auto slash = name.find('/');
		if (slash == 0) return nullptr;
		if (slash == std::string::npos)
		{
			if (name == ".target")
			{
				assert(cache->Target.DeterminedType);
				return cache->Target.DeterminedType;
			}
			switch (cache->Source->Type)
			{
			case CONSTRAINT_TRAIT_ASSEMBLY:
			case CONSTRAINT_TRAIT_IMPORT:
				for (auto& e : cache->Trait->Types)
				{
					if (name == e.ExportName)
					{
						auto ct = ConstructConstraintTraitType(*cache, e.Index);
						SimplifyConstraintType(ct);
						assert(ct.CType == CTT_RT || ct.CType == CTT_EMPTY);
						if (ct.CType == CTT_RT)
						{
							assert(ct.DeterminedType);
							return ct.DeterminedType;
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
			case CONSTRAINT_TRAIT_ASSEMBLY:
			case CONSTRAINT_TRAIT_IMPORT:
			{
				auto& constraintList = cache->Trait->Generic.Constraints;
				assert(constraintList.size() == cache->Children.size());
				for (std::size_t i = 0; i < cache->Children.size(); ++i)
				{
					if (constraintList[i].ExportName == childName)
					{
						return FindConstraintExportType(cache->Children[i].get(), name.substr(slash + 1));
					}
				}
				return nullptr;
			}
			default:
				return nullptr;
			}
		}
	}

	RuntimeFunction* FindConstraintExportFunction(ConstraintCalculationCache* cache, const std::string& name)
	{
		if (name.length() == 0) return nullptr;
		auto& constraintName = cache->Source->ExportName;
		auto slash = name.find('/');
		if (slash == 0) return nullptr;
		if (slash == std::string::npos)
		{
			switch (cache->Source->Type)
			{
			case CONSTRAINT_TRAIT_ASSEMBLY:
			case CONSTRAINT_TRAIT_IMPORT:
				for (std::size_t i = 0; i < cache->Trait->Functions.size(); ++i)
				{
					auto& e = cache->Trait->Functions[i];
					auto& tf = cache->TraitFunctions[i];
					if (name == e.ExportName)
					{
						auto index = tf.Overloads[tf.CurrentOverload].Index;
						assert(cache->Target.DeterminedType);
						auto tt = FindTypeTemplate(cache->Target.DeterminedType->Args);
						LoadingRefArguments lg = { cache->Target.DeterminedType, tt->Generic };
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
			case CONSTRAINT_TRAIT_ASSEMBLY:
			case CONSTRAINT_TRAIT_IMPORT:
			{
				auto& constraintList = cache->Trait->Generic.Constraints;
				assert(constraintList.size() == cache->Children.size());
				for (std::size_t i = 0; i < cache->Children.size(); ++i)
				{
					if (constraintList[i].ExportName == childName)
					{
						return FindConstraintExportFunction(cache->Children[i].get(), name.substr(slash + 1));
					}
				}
			}
			default:
				return nullptr;
			}
		}
	}

	std::size_t FindConstraintExportField(ConstraintCalculationCache* cache, const std::string& name)
	{
		if (name.length() == 0) return SIZE_MAX;
		auto& constraintName = cache->Source->ExportName;
		auto slash = name.find('/');
		if (slash == 0) return SIZE_MAX;
		if (slash == std::string::npos)
		{
			switch (cache->Source->Type)
			{
			case CONSTRAINT_TRAIT_ASSEMBLY:
			case CONSTRAINT_TRAIT_IMPORT:
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
			case CONSTRAINT_TRAIT_ASSEMBLY:
			case CONSTRAINT_TRAIT_IMPORT:
			{
				auto& constraintList = cache->Trait->Generic.Constraints;
				assert(constraintList.size() == cache->Children.size());
				for (std::size_t i = 0; i < cache->Children.size(); ++i)
				{
					if (constraintList[i].ExportName == childName)
					{
						return FindConstraintExportField(cache->Children[i].get(), name.substr(slash + 1));
					}
				}
			}
			default:
				return SIZE_MAX;
			}
		}
	}
};

}

using ConstraintObjects::RuntimeLoaderConstraint;

inline bool RuntimeLoaderCore::CheckConstraints(const std::string& srcAssebly, GenericDeclaration* g,
	const MultiList<RuntimeType*>& args, ConstraintExportList* exportList)
{
	auto l = static_cast<RuntimeLoaderConstraint*>(this);
	return l->CheckConstraintsImpl(srcAssebly, g, args, exportList);
}

}
