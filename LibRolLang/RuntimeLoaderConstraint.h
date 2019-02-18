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
		while (IsCacheUndetermined(cache))
		{
			if (TryDetermineConstraintArgument(*cache) != 1)
			{
				return false;
			}
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
		case CTT_PARAMETER:
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

	static bool IsCacheUndetermined(ConstraintCalculationCache* cache)
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
	void FindTraitFromConstraint(GenericConstraint& c, std::string& srcAssembly,
		Trait*& traitPtr, std::string& traitAssembly)
	{
		if (c.Type == CONSTRAINT_TRAIT_ASSEMBLY)
		{
			auto assembly = FindAssemblyThrow(srcAssembly);
			if (c.Index >= assembly->Traits.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid trait reference");
			}
			traitPtr = &assembly->Traits[c.Index];
			traitAssembly = srcAssembly;
		}
		else
		{
			assert(c.Type == CONSTRAINT_TRAIT_IMPORT);
			auto assembly = FindAssemblyThrow(srcAssembly);
			LoadingArguments la;
			if (c.Index >= assembly->ImportTraits.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid trait reference");
			}
			if (!FindExportTrait(assembly->ImportTraits[c.Index], la))
			{
				throw RuntimeLoaderException(ERR_L_LINK, "Invalid trait reference");
			}
			traitPtr = &FindAssemblyThrow(la.Assembly)->Traits[la.Id];
			traitAssembly = la.Assembly;
		}
	}

	void InitTraitConstraintCache(ConstraintCalculationCache& cache)
	{
		FindTraitFromConstraint(*cache.Source, cache.SrcAssembly, cache.Trait, cache.TraitAssembly);

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
		case CTT_FAIL:
			//Although we don't know whether they come from the same type,
			//since they both fail, they will lead to the same result (and
			//keep failing in children).
			return true;
		case CTT_PARAMETER:
			return a.ParameterSegment == b.ParameterSegment &&
				a.ParameterIndex == b.ParameterIndex &&
				a.SubtypeName == b.SubtypeName;
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
			func_info.Type.ReturnType = ConstructConstraintTraitType(parent, func.ReturnType);
			for (auto p : func.ParameterTypes)
			{
				func_info.Type.ParameterTypes.push_back(ConstructConstraintTraitType(parent, p));
			}
			parent.TraitFunctions.emplace_back(std::move(func_info));
		}

		//Generic functions
		//TODO temporary support for fixed segment for parent.Arguments
		const MultiList<ConstraintCheckType>* parentArguments = &parent.Arguments;
		MultiList<ConstraintCheckType> emptyArgumentList;
		if (parent.Arguments.GetTotalSize() == 0)
		{
			parentArguments = &emptyArgumentList;
		}
		for (auto& func : trait->GenericFunctions)
		{
			TraitCacheFunctionInfo func_info = {};
			LoadTraitGenericFunctionInfo(parent, trait->Generic, func.Index, *parentArguments,
				func_info.Type, func_info.AdditionalGenericNumber);
			parent.TraitGenericFunctions.emplace_back(std::move(func_info));
		}

		parent.TraitMemberResolved = false;
		parent.TraitCacheCreated = true;
	}

	//ret 1: all members successfully resolved. 0: cannot resolve (not determined). -1: constraint fails
	int TryResolveTraitSubMember(ConstraintCalculationCache& parent)
	{
		EnsureSubConstraintCached(parent);
		assert(parent.TraitCacheCreated);

		if (parent.TraitMemberResolved) return 1;
		auto trait = parent.Trait;

		SimplifyConstraintType(parent.Target);
		if (parent.Target.CType != CTT_RT)
		{
			return 0;
		}

		auto target = parent.Target.DeterminedType;
		if (target == nullptr)
		{
			//Trait on void type (REF_EMPTY).
			return -1;
		}

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
			for (auto func : TypeTemplateFunctionList(tt))
			{
				if (func.Name != trait->Functions[i].ElementName) continue;
				TraitCacheFunctionOverloadInfo fi = {};
				fi.Index = func.Index;
				ud.clear();
				if (!CheckTraitTargetFunctionOverload(parent, i, tt, ud, fi))
				{
					continue;
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

		//Generic function matching needs the argument list (as ConstraintCheckType)
		MultiList<ConstraintCheckType> targetLoadingArgs;
		{
			auto root = parent.Root;
			target->Args.Arguments.CopyList(targetLoadingArgs,
				[root](RuntimeType* rt) { return ConstraintCheckType::Determined(root, rt); });
		}

		for (std::size_t i = 0; i < trait->GenericFunctions.size(); ++i)
		{
			//Virtual function lists can have generic functions
			for (auto func : tt->PublicFunctions)
			{
				if (func.Name != trait->GenericFunctions[i].ElementName) continue;
				TraitCacheFunctionOverloadInfo fi = {};
				fi.Index = func.Id;
				if (!CheckTraitTargetGenericFunctionOverload(parent, i, tt, targetLoadingArgs, fi))
				{
					continue;
				}
				parent.TraitGenericFunctions[i].Overloads.emplace_back(std::move(fi));
			}
			if (parent.TraitGenericFunctions[i].Overloads.size() == 0)
			{
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
		if (!LoadTraitFunctionOverloadInfo(parent, tt->Generic, target->Args.Assembly, fi, ud))
		{
			return false;
		}
		//TODO handle parameter pack
		if (fi.Type.ParameterTypes.size() != f.ParameterTypes.size())
		{
			return false;
		}
		if (!CheckTypePossiblyEqual(fi.Type.ReturnType, parent.TraitFunctions[i].Type.ReturnType))
		{
			return false;
		}
		for (std::size_t k = 0; k < fi.Type.ParameterTypes.size(); ++k)
		{
			if (!CheckTypePossiblyEqual(fi.Type.ParameterTypes[k],
				parent.TraitFunctions[i].Type.ParameterTypes[k]))
			{
				return false;
			}
		}
		return true;
	}

	bool CheckTraitTargetGenericFunctionOverload(ConstraintCalculationCache& parent, std::size_t i, Type* tt,
		const MultiList<ConstraintCheckType>& args, TraitCacheFunctionOverloadInfo& fi)
	{
		auto& gf = parent.TraitGenericFunctions[i];
		std::vector<std::size_t> additional;
		LoadTraitGenericFunctionInfo(parent, tt->Generic, fi.Index, args, fi.Type, additional);

		if (additional != gf.AdditionalGenericNumber) return false;
		if (!CheckTypePossiblyEqual(gf.Type.ReturnType, fi.Type.ReturnType)) return false;

		if (gf.Type.ParameterTypes.size() != fi.Type.ParameterTypes.size()) return false;
		for (std::size_t j = 0; j < gf.Type.ParameterTypes.size(); ++j)
		{
			if (!CheckTypePossiblyEqual(gf.Type.ParameterTypes[j], fi.Type.ParameterTypes[j])) return false;
		}

		if (gf.Type.ConstraintTypeList != fi.Type.ConstraintTypeList ||
			gf.Type.ConstraintTraitList != fi.Type.ConstraintTraitList ||
			gf.Type.EqualTypeNumberList != fi.Type.EqualTypeNumberList)
		{
			return false;
		}

		if (gf.Type.ConstraintEqualTypes.size() != fi.Type.ConstraintEqualTypes.size()) return false;
		for (std::size_t j = 0; j < gf.Type.ConstraintEqualTypes.size(); ++j)
		{
			if (!CheckTypePossiblyEqual(gf.Type.ConstraintEqualTypes[j], fi.Type.ConstraintEqualTypes[j])) return false;
		}

		return true;
	}

	bool LoadTraitFunctionOverloadInfo(ConstraintCalculationCache& parent, GenericDeclaration& g,
		const std::string& src_assembly, TraitCacheFunctionOverloadInfo& result,
		std::vector<ConstraintCheckType>& additionalUd)
	{
		assert(&g == &FindTypeTemplate(parent.Target.DeterminedType->Args)->Generic);

		auto target = parent.Target.DeterminedType;
		assert(target);

		auto id = result.Index;
		if (id >= g.RefList.size())
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
		}

		//First resolve any REF_CLONE so that we don't need to worry it any more.
		//TODO detect circular REF_CLONE, move to reflist
		while (g.RefList[id].Type == REF_CLONE)
		{
			id = g.RefList[id].Index;
			if (id >= g.RefList.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
			}
		}

		//Find the function template.
		LoadingArguments la;
		switch (g.RefList[id].Type & REF_REFTYPES)
		{
		case REF_FUNC_INTERNAL:
			la.Assembly = src_assembly;
			la.Id = g.RefList[id].Index;
			break;
		case REF_FUNC_EXTERNAL:
		{
			auto a = FindAssemblyThrow(src_assembly);
			if (g.RefList[id].Index >= a->ImportFunctions.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
			}
			auto i = a->ImportFunctions[g.RefList[id].Index];
			if (!FindExportFunction(i, la))
			{
				throw RuntimeLoaderException(ERR_L_LINK, "Import function not found");
			}
			break;
		}
		case REF_FUNC_CONSTRAINT:
		case REF_FUNC_G_CONSTRAINT:
			//TODO support constraint function here
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
		}

		//About the arguments:
		//We are in a type. Its Functions[id] specify a function. The argument to the
		//function is the arg list in the RefList plus some additional REF_ANY.
		//To calculate those in the RefList, we need the args to the type itself.

		//TODO fix the name: it's not additional but all arguments
		auto additional = GetFunctionGenericArgumentNumber(g, id);

		MultiList<ConstraintCheckType> typeArgs;
		target->Args.Arguments.CopyList(typeArgs, [&parent](RuntimeType* ta)
		{
			return ConstraintCheckType::Determined(parent.Root, ta);
		});
		//Note that additional arguments are appended to type arguments.
		for (std::size_t i = 0; i < target->Args.Arguments.GetSizeList().size(); ++i)
		{
			//TODO support variable size
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
		for (auto&& e : GetRefArgList(g.RefList, id, funcArgs))
		{
			assert(e.Entry.Type == REF_CLONETYPE);
			funcArgs.AppendLast(ConstructConstraintRefListType(parent.Root,
				g, type_assembly, e.Entry.Index, typeArgs, target, nullptr, nullptr));
		}

		Function* ft = FindFunctionTemplate(la.Assembly, la.Id);
		if (!ft->Generic.ParameterCount.CanMatch(funcArgs.GetSizeList()))
		{
			//Argument count mismatch.
			return false;
		}

		//Construct ConstraintCheckType for ret and params.
		result.Type.ReturnType = ConstructConstraintRefListType(parent.Root, ft->Generic,
			la.Assembly, ft->ReturnValue.TypeId, funcArgs, nullptr, nullptr, &result.ExportTypes);
		for (auto& parameter : ft->Parameters)
		{
			result.Type.ParameterTypes.emplace_back(ConstructConstraintRefListType(parent.Root, ft->Generic,
				la.Assembly, parameter.TypeId, funcArgs, nullptr, nullptr, &result.ExportTypes));
		}

		result.FunctionTemplate = ft;
		result.GenericArguments = std::move(funcArgs);
		result.FunctionTemplateAssembly = la.Assembly;

		return true;
	}

	//TODO additionalNumber support variable size
	void LoadTraitGenericFunctionInfo(ConstraintCalculationCache& parent, GenericDeclaration& g, std::size_t id,
		const MultiList<ConstraintCheckType>& args, TraitFunctionType& type, std::vector<std::size_t>& additionalNumber)
	{
		if (id >= g.RefList.size())
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
		}

		//TODO detect circular REF_CLONE, move to reflist
		while (g.RefList[id].Type == REF_CLONE)
		{
			id = g.RefList[id].Index;
			if (id >= g.RefList.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
			}
		}

		//Find the function template.
		LoadingArguments la;
		switch (g.RefList[id].Type & REF_REFTYPES)
		{
		case REF_FUNC_INTERNAL:
			la.Assembly = parent.TraitAssembly;
			la.Id = g.RefList[id].Index;
			break;
		case REF_FUNC_EXTERNAL:
		{
			auto a = FindAssemblyThrow(parent.TraitAssembly);
			if (g.RefList[id].Index >= a->ImportFunctions.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
			}
			auto i = a->ImportFunctions[g.RefList[id].Index];
			if (!FindExportFunction(i, la))
			{
				throw RuntimeLoaderException(ERR_L_LINK, "Import function not found");
			}
			break;
		}
		case REF_FUNC_CONSTRAINT:
		case REF_FUNC_G_CONSTRAINT:
			//TODO support constraint function here
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
		}

		auto genericArgs = GetFunctionGenericArgumentNumber(g, id);

		//TODO support multilist
		auto traitArgNum = args.GetSizeList().size();
		for (std::size_t i = 0; i < traitArgNum; ++i) //TODO check Arguments
		{
			//TODO support variable size
			if (genericArgs[i] > args.GetSizeList()[i])
			{
				throw RuntimeLoaderException(ERR_L_GENERIC, "Invalid function reference");
			}
		}

		auto additional = std::vector<std::size_t>(genericArgs.begin() + traitArgNum, genericArgs.end());

		//Use trait arguments for the first part and add additional types
		MultiList<ConstraintCheckType> refListArgs = args;
		for (std::size_t i = 0; i < additional.size(); ++i)
		{
			refListArgs.NewList();
			for (std::size_t j = 0; j < additional[i]; ++j)
			{
				refListArgs.AppendLast(ConstraintCheckType::Parameter(parent.Root, i, j));
			}
		}

		MultiList<ConstraintCheckType> funcArgs;
		std::vector<TraitCacheFunctionConstrainExportInfo> traitExportTypes;
		for (auto&& e : GetRefArgList(g.RefList, id, funcArgs))
		{
			assert(e.Entry.Type == REF_CLONETYPE);
			funcArgs.AppendLast(ConstructConstraintRefListType(parent.Root,
				g, parent.TraitAssembly, e.Entry.Index, refListArgs, nullptr, nullptr, &traitExportTypes));
		}
		if (traitExportTypes.size() != 0)
		{
			//TODO should we support this? Need to be consistent with other places.
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Constraint exporting of trait not supported");
		}

		Function* ft = FindFunctionTemplate(la.Assembly, la.Id);
		if (!ft->Generic.ParameterCount.CanMatch(funcArgs.GetSizeList()))
		{
			//Argument count mismatch.
			throw RuntimeLoaderException(ERR_L_GENERIC, "Generic argument number mismatches");
		}
		type.ReturnType = ConstructConstraintRefListType(parent.Root, ft->Generic,
			la.Assembly, ft->ReturnValue.TypeId, funcArgs, nullptr, nullptr, nullptr);
		for (auto& parameter : ft->Parameters)
		{
			type.ParameterTypes.emplace_back(ConstructConstraintRefListType(parent.Root, ft->Generic,
				la.Assembly, parameter.TypeId, funcArgs, nullptr, nullptr, nullptr));
		}

		for (auto& c : ft->Generic.Constraints)
		{
			type.ConstraintTypeList.push_back(c.Type);
			type.EqualTypeNumberList.push_back(c.Arguments.size() + 1); //TODO support variable size
			if (c.Type == CONSTRAINT_TRAIT_ASSEMBLY ||
				c.Type == CONSTRAINT_TRAIT_IMPORT)
			{
				Trait* t;
				std::string ta;
				FindTraitFromConstraint(c, la.Assembly, t, ta);
				type.ConstraintTraitList.push_back(t);
			}
			
			//To use ConstructConstraintRefListType, we create a fake GenericDecl.
			//TODO use another struct to represent type list in GenericDecl
			GenericDeclaration g;
			g.NamesList = c.NamesList;
			g.RefList = c.TypeReferences;

			std::size_t udCount = 0;
			type.ConstraintEqualTypes.emplace_back(ConstructConstraintRefListType(parent.Root, g,
				la.Assembly, c.Target, funcArgs, nullptr, &udCount, nullptr));
			for (std::size_t i = 0; i < c.Arguments.size(); ++i)
			{
				type.ConstraintEqualTypes.emplace_back(ConstructConstraintRefListType(parent.Root, g,
					la.Assembly, c.Arguments[i], funcArgs, nullptr, &udCount, nullptr));
			}
		}

		additionalNumber = std::move(additional);
	}

	//Scan the function reference. Make sure it's valid. Return the total number of args needed.
	std::vector<std::size_t> GetFunctionGenericArgumentNumber(GenericDeclaration& g, std::size_t id)
	{
		std::vector<std::size_t> ret;
		GetFunctionGenericArgumentNumberInternal(g, id, ret);
		return ret;
	}

	//TODO consider move to RefList
	void GetFunctionGenericArgumentNumberInternal(GenericDeclaration& g, std::size_t id,
		std::vector<std::size_t>& result)
	{
		if (id >= g.RefList.size())
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
		}
		std::size_t ret = 0;
		switch (g.RefList[id].Type & REF_REFTYPES)
		{
		case REF_CLONE:
			//TODO detect circular REF_CLONE
			GetFunctionGenericArgumentNumberInternal(g, g.RefList[id].Index, result);
			break;
		case REF_FUNC_INTERNAL:
		case REF_FUNC_EXTERNAL:
		case REF_FUNC_G_CONSTRAINT:
		{
			MultiList<int> notUsed;
			for (auto&& e : GetRefArgList(g.RefList, id, notUsed))
			{
				GetTypeGenericArgumentNumberInternal(g, e.Entry.Index, result);
			}
			break;
		}
		case REF_FUNC_CONSTRAINT:
			break;
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
		}
	}

	//TODO consider move to RefList
	void GetTypeGenericArgumentNumberInternal(GenericDeclaration& g, std::size_t id,
		std::vector<std::size_t>& result)
	{
		if (id >= g.RefList.size())
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
		}
		std::size_t ret = 0;
		auto t = g.RefList[id];
		switch (t.Type & REF_REFTYPES)
		{
		case REF_CLONE:
			//TODO detect circular REF_CLONE
			GetTypeGenericArgumentNumberInternal(g, t.Index, result);
			break;
		case REF_TYPE_INTERNAL:
		case REF_TYPE_EXTERNAL:
		{
			MultiList<int> notUsed;
			for (auto&& e : GetRefArgList(g.RefList, id, notUsed))
			{
				GetTypeGenericArgumentNumberInternal(g, e.Index, result);
			}
			break;
		}
		case REF_TYPE_SUBTYPE:
		{
			MultiList<int> notUsed;
			GetTypeGenericArgumentNumberInternal(g, id + 1, result);
			for (auto&& e : GetRefArgList(g.RefList, id + 1, notUsed))
			{
				GetTypeGenericArgumentNumberInternal(g, e.Index, result);
			}
			break;
		}
		case REF_TYPE_ARGUMENT:
		{
			if (id + 1 >= g.RefList.size() || g.RefList[id + 1].Type != REF_ARGUMENTSEG)
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid RefList entry");
			}
			auto seg = g.RefList[id + 1].Index;
			auto i = g.RefList[id].Index;
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
		case REF_TYPE_SELF:
		case REF_TYPE_CONSTRAINT:
		case REF_EMPTY:
			break;
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
		}
	}

	//TODO merge ConstructXXXType
	ConstraintCheckType ConstructConstraintRefListType(ConstraintCalculationCacheRoot* root, GenericDeclaration& g,
		const std::string& src, std::size_t i, MultiList<ConstraintCheckType>& arguments, RuntimeType* selfType, std::size_t* udCount,
		std::vector<TraitCacheFunctionConstrainExportInfo>* exportList)
	{
		if (i >= g.RefList.size())
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
		}
		while (g.RefList[i].Type == REF_CLONE)
		{
			i = g.RefList[i].Index;
			if (i >= g.RefList.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
			}
		}
		switch (g.RefList[i].Type & REF_REFTYPES)
		{
		case REF_EMPTY:
			return ConstraintCheckType::Empty(root);
		case REF_TYPE_ARGUMENT:
			return GetRefArgument(g.RefList, i, arguments);
		case REF_TYPE_SELF:
			if (selfType != nullptr)
			{
				return ConstraintCheckType::Determined(root, selfType);
			}
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
		case REF_TYPE_INTERNAL:
		{
			auto ret = ConstraintCheckType::Generic(root, src, g.RefList[i].Index);
			for (auto&& e : GetRefArgList(g.RefList, i, ret.Args))
			{
				ret.Args.AppendLast(ConstructConstraintRefListType(ret.Root,
					g, src, e.Index, arguments, selfType, udCount, exportList));
			}
			return ret;
		}
		case REF_TYPE_EXTERNAL:
		{
			LoadingArguments la;
			auto a = FindAssemblyThrow(src);
			if (g.RefList[i].Index >= a->ImportTypes.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
			}
			auto import_info = a->ImportTypes[g.RefList[i].Index];
			if (!FindExportType(import_info, la))
			{
				throw RuntimeLoaderException(ERR_L_LINK, "Import type not found");
			}
			auto ret = ConstraintCheckType::Generic(root, la.Assembly, la.Id);
			for (auto&& e : GetRefArgList(g.RefList, i, ret.Args))
			{
				ret.Args.AppendLast(ConstructConstraintRefListType(ret.Root,
					g, src, e.Index, arguments, selfType, udCount, exportList));
			}
			return ret;
		}
		case REF_TYPE_SUBTYPE:
		{
			auto ret = ConstraintCheckType::Subtype(root, g.NamesList[g.RefList[i].Index]);
			ret.ParentType.emplace_back(ConstructConstraintRefListType(ret.Root, g, src, i + 1,
				arguments, selfType, udCount, exportList));
			for (auto&& e : GetRefArgList(g.RefList, i + 1, ret.Args))
			{
				ret.Args.AppendLast(ConstructConstraintRefListType(ret.Root,
					g, src, e.Index, arguments, selfType, udCount, exportList));
			}
			return ret;
		}
		case REF_TYPE_CONSTRAINT:
		{
			if (selfType != nullptr)
			{
				//Calling for determined type. Calculate REF_CONSTRAINT.
				for (auto& ct : selfType->ConstraintExportList)
				{
					if (ct.EntryType == CONSTRAINT_EXPORT_TYPE && ct.Index == g.RefList[i].Index)
					{
						return ConstraintCheckType::Determined(root, ct.Type);
					}
				}
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid REF_CONSTRAINT reference");
			}
			else if (exportList != nullptr)
			{
				//Calling with export list (when doing function overload matching), create CTT_ANY type.
				//First find through cache to ensure one REF_CONSTRAINT use one CTT_ANY type.
				for (auto& ct : *exportList)
				{
					if (ct.NameIndex == g.RefList[i].Index)
					{
						return ct.UndeterminedType;
					}
				}
				auto newType = ConstraintCheckType::Undetermined(root);
				exportList->push_back({ i, newType });
				return newType;
			}
			else
			{
				//Calling with neither. We are creating types for generic function matching in trait.
				//Create CTT_PARAM with SIZE_MAX as segment index. Because all constraints are checked 
				//later to be exactly matching, we only need to ensure the name is the same. Name is
				//stored in SubtypeName.
				return ConstraintCheckType::Parameter(root, SIZE_MAX, 0, g.NamesList[g.RefList[i].Index]);
			}
		}
		case REF_TYPE_C_ANY:
			if (udCount == nullptr)
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid REF_ANY");
			}
			return ConstraintCheckType::Parameter(root, SIZE_MAX - 1, (*udCount)++);
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
		}
	}

	ConstraintCheckType ConstructConstraintTraitType(ConstraintCalculationCache& cache, std::size_t i)
	{
		auto trait = cache.Trait;
		assert(trait);
		auto& list = trait->Generic.RefList;
		auto& t = list[i];

		switch (t.Type & REF_REFTYPES)
		{
		case REF_CLONE:
			//TODO detect circular REF_CLONE
			return ConstructConstraintTraitType(cache, t.Index);
		case REF_TYPE_ARGUMENT:
			return GetRefArgument(list, i, cache.Arguments);
		case REF_TYPE_SELF:
			return cache.Target;
		case REF_TYPE_INTERNAL:
		{
			auto ret = ConstraintCheckType::Generic(cache.Root, cache.TraitAssembly, t.Index);
			for (auto&& e : GetRefArgList(list, i, ret.Args))
			{
				ret.Args.AppendLast(ConstructConstraintTraitType(cache, e.Index));
			}
			return ret;
		}
		case REF_TYPE_EXTERNAL:
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
		case REF_TYPE_SUBTYPE:
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
		case REF_TYPE_C_ANY:
		case REF_TYPE_C_TRY:
		case REF_TYPE_CONSTRAINT: //TODO check
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
		case REF_TYPE_C_ANY:
			return ConstraintCheckType::Undetermined(cache.Root);
		case REF_TYPE_C_TRY:
			return ConstraintCheckType::Try(ConstructConstraintArgumentType(cache, constraint, t.Index));
		case REF_CLONE:
			//TODO detect circular REF_CLONE
			return ConstructConstraintArgumentType(cache, constraint, t.Index);
		case REF_TYPE_ARGUMENT:
			return GetRefArgument(list, i, cache.CheckArguments);
		case REF_TYPE_SELF:
			if (cache.CheckTarget.CType == CTT_FAIL)
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid use of REF_SELF");
			}
			return cache.CheckTarget;
		case REF_TYPE_INTERNAL:
		{
			auto ret = ConstraintCheckType::Generic(cache.Root, cache.SrcAssembly, t.Index);
			for (auto&& e : GetRefArgList(list, i, ret.Args))
			{
				ret.Args.AppendLast(ConstructConstraintArgumentType(cache, constraint, e.Index));
			}
			return ret;
		}
		case REF_TYPE_EXTERNAL:
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
		case REF_TYPE_SUBTYPE:
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
		case REF_EMPTY:
			return ConstraintCheckType::Empty(cache.Root);
		case REF_TYPE_CONSTRAINT: //TODO check
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
		}
	}
	
	bool CheckTypePossiblyEqual(ConstraintCheckType& a, ConstraintCheckType& b)
	{
		//We only need a quick check to eliminate most overloads. Don't simplify.
		if (a.CType == CTT_FAIL || a.CType == CTT_FAIL) return false;
		if (a.CType == CTT_PARAMETER || b.CType == CTT_PARAMETER)
		{
			//Note that CTT_ANY/CTT_SUBTYPE does not match CTT_PARAMETER.
			if (a.CType != b.CType) return false;
			return a.ParameterSegment == b.ParameterSegment &&
				a.ParameterIndex == b.ParameterIndex &&
				a.SubtypeName == b.SubtypeName;
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
			if (a.DeterminedType == nullptr)
			{
				return false; //empty type != any generic type.
			}
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
		if (a.CType == CTT_FAIL || b.CType == CTT_FAIL) return -1;
		if (a.CType == CTT_PARAMETER || b.CType == CTT_PARAMETER)
		{
			if (!CheckTypePossiblyEqual(a, b)) return -1;
			return 0;
		}
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
			if (a.DeterminedType == nullptr)
			{
				return -1;
			}
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
			if (!cache.TraitMemberResolved)
			{
				return TryResolveTraitSubMember(cache);
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
				assert(f.Overloads.size() != 0);
				if (f.Overloads.size() == 1)
				{
					assert(f.CurrentOverload == 0);
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

			//Generic functions (in one pass)
			for (auto& f : cache.TraitGenericFunctions)
			{
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
		
		ret = TryDetermineEqualTypes(f.Type.ReturnType, overload.Type.ReturnType);
		if (ret != 0) return ret;

		assert(f.Type.ParameterTypes.size() == overload.Type.ParameterTypes.size());
		for (std::size_t i = 0; i < f.Type.ParameterTypes.size(); ++i)
		{
			ret = TryDetermineEqualTypes(f.Type.ParameterTypes[i], overload.Type.ParameterTypes[i]);
			if (ret != 0) return ret;
		}

		assert(f.Type.ConstraintEqualTypes.size() == overload.Type.ConstraintEqualTypes.size());
		for (std::size_t i = 0; i < f.Type.ConstraintEqualTypes.size(); ++i)
		{
			ret = TryDetermineEqualTypes(f.Type.ConstraintEqualTypes[i], overload.Type.ConstraintEqualTypes[i]);
			if (ret != 0) return ret;
		}

		return 0;
	}

	//Can only be used in SimplifyConstraintType.
	bool TrySimplifyConstraintType(ConstraintCheckType& t, ConstraintCheckType& parent)
	{
		SimplifyConstraintType(t);
		if (t.CType != CTT_RT)
		{
			//Only return true for CTT_RT, not CTT_PARAMETER.
			if (t.CType == CTT_FAIL)
			{
				parent.DeductFail();
			}
			return false;
		}
		return true;
	}

	void SimplifyConstraintType(ConstraintCheckType& t)
	{
		switch (t.CType)
		{
		case CTT_RT:
		case CTT_FAIL:
		case CTT_PARAMETER:
			//Elemental type. Can't simplify.
			return;
		case CTT_ANY:
			if (t.Root->IsDetermined(t.UndeterminedId))
			{
				t.DeductRT(t.Root->GetDetermined(t.UndeterminedId));
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
			if (lg.Parent == nullptr)
			{
				t.DeductFail();
				return;
			}
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

	//TODO merge with TrySimplifyConstraintType
	bool CheckSimplifiedConstraintType(ConstraintCheckType& t)
	{
		//Should not have CTT_PARAMETER here.
		assert(t.CType != CTT_PARAMETER);
		SimplifyConstraintType(t);
		if (t.CType != CTT_RT)
		{
			assert(t.CType == CTT_FAIL);
			return false;
		}
		return true;
	}

	bool CheckTraitDetermined(ConstraintCalculationCache& cache)
	{
		EnsureSubConstraintCached(cache);
		if (TryResolveTraitSubMember(cache) != 1)
		{
			//Resolving submember only requires Target to be determined,
			//which should be success if it goes here.
			return false;
		}

		auto target = cache.Target.DeterminedType;
		assert(target);

		//Fields
		for (auto& tf : cache.TraitFields)
		{
			if (!CheckDeterminedTypesEqual(tf.Type, tf.TypeInTarget))
			{
				return false;
			}
		}

		//Functions
		for (auto& tf : cache.TraitFunctions)
		{
			auto& overload = tf.Overloads[tf.CurrentOverload];

			//Match function types (return & parameters).
			//Note that there might still be undetermined (because of how we resolve REF_CONSTRAINT).
			if (!CheckTypePossiblyEqual(tf.Type.ReturnType, overload.Type.ReturnType))
			{
				return false;
			}
			assert(tf.Type.ParameterTypes.size() == overload.Type.ParameterTypes.size());
			for (std::size_t i = 0; i < tf.Type.ParameterTypes.size(); ++i)
			{
				if (!CheckTypePossiblyEqual(tf.Type.ParameterTypes[i], overload.Type.ParameterTypes[i]))
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
			//TODO detect circular calculation
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
						assert(IsTypeUndetermined(cache.Root, ct.UndeterminedType));
						ct.UndeterminedType.Root->Determine(ct.UndeterminedType.UndeterminedId, et.Type);
						break;
					}
				}
			}

			//Final check (now there should be no REF_ANY).
			if (!CheckDeterminedTypesEqual(tf.Type.ReturnType, overload.Type.ReturnType))
			{
				return false;
			}
			for (std::size_t i = 0; i < tf.Type.ParameterTypes.size(); ++i)
			{
				if (!CheckDeterminedTypesEqual(tf.Type.ParameterTypes[i], overload.Type.ParameterTypes[i]))
				{
					return false;
				}
			}
		}

		//Generic functions
		for (auto& tf : cache.TraitGenericFunctions)
		{
			auto& overload = tf.Overloads[tf.CurrentOverload];

			if (!CheckDeterminedTypesWithParameterEqual(tf.Type.ReturnType, overload.Type.ReturnType))
			{
				return false;
			}
			assert(tf.Type.ParameterTypes.size() == overload.Type.ParameterTypes.size());
			for (std::size_t i = 0; i < tf.Type.ParameterTypes.size(); ++i)
			{
				if (!CheckDeterminedTypesWithParameterEqual(tf.Type.ParameterTypes[i], overload.Type.ParameterTypes[i]))
				{
					return false;
				}
			}
			assert(tf.Type.ConstraintEqualTypes.size() == overload.Type.ConstraintEqualTypes.size());
			for (std::size_t i = 0; i < tf.Type.ConstraintEqualTypes.size(); ++i)
			{
				if (!CheckDeterminedTypesWithParameterEqual(tf.Type.ConstraintEqualTypes[i], overload.Type.ConstraintEqualTypes[i]))
				{
					return false;
				}
			}
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

		return true;
	}

	bool CheckDeterminedTypesEqual(ConstraintCheckType& a, ConstraintCheckType& b)
	{
		if (!CheckSimplifiedConstraintType(a)) return false;
		if (!CheckSimplifiedConstraintType(b)) return false;
		return a.DeterminedType == b.DeterminedType;
	}

	bool CheckDeterminedTypesWithParameterEqual(ConstraintCheckType& a, ConstraintCheckType& b)
	{
		SimplifyConstraintType(a);
		SimplifyConstraintType(b);
		return CheckDeterminedTypesWithParameterEqualRecursive(a, b);
	}

	bool CheckDeterminedTypesWithParameterEqualRecursive(ConstraintCheckType& a, ConstraintCheckType& b)
	{
		assert(a.CType != CTT_FAIL);
		assert(a.CType != CTT_ANY);
		assert(b.CType != CTT_FAIL);
		assert(b.CType != CTT_ANY);
		if (a.CType != b.CType) return false;
		switch (a.CType)
		{
		case CTT_RT:
			return a.DeterminedType == b.DeterminedType;
		case CTT_GENERIC:
		{
			if (a.TypeTemplateAssembly != b.TypeTemplateAssembly ||
				a.TypeTemplateIndex != b.TypeTemplateIndex)
			{
				return false;
			}
			break;
		}
		case CTT_PARAMETER:
			return a.ParameterSegment == b.ParameterSegment &&
				a.ParameterIndex == b.ParameterIndex &&
				a.SubtypeName == b.SubtypeName;
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
			return false;
		}

		//TODO consider merge with AreConstraintTypesEqual
		auto& sa = a.Args.GetSizeList();
		auto& sb = b.Args.GetSizeList();
		if (sa != sb) return false;
		for (std::size_t i = 0; i < sa.size(); ++i)
		{
			for (std::size_t j = 0; j < sa[i]; ++j)
			{
				if (!CheckDeterminedTypesWithParameterEqualRecursive(a.Args.GetRef(i, j), b.Args.GetRef(i, j))) return false;
			}
		}
		return true;
	}

	bool CheckLoadingTypeBase(RuntimeType* typeChecked, RuntimeType* typeBase)
	{
		if (typeChecked == nullptr || typeBase == nullptr) return false;
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
		if (typeChecked == nullptr || typeBase == nullptr) return false;
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
		for (std::size_t i = 0; i < g->RefList.size(); ++i)
		{
			if ((g->RefList[i].Type & REF_REFTYPES) != REF_TYPE_CONSTRAINT) continue;
			auto& name = g->NamesList[g->RefList[i].Index];
			if (name.compare(0, prefix.length(), prefix) == 0)
			{
				RuntimeType* result;
				if (FindConstraintExportType(cache, name.substr(prefix.length()), &result))
				{
					ConstraintExportListEntry entry;
					entry.EntryType = CONSTRAINT_EXPORT_TYPE;
					entry.Index = i;
					entry.Type = result;
					exportList->push_back(entry);
				}
			}
		}

		//Export functions
		for (std::size_t i = 0; i < g->RefList.size(); ++i)
		{
			if ((g->RefList[i].Type & REF_REFTYPES) != REF_FUNC_CONSTRAINT) continue;
			auto& name = g->NamesList[g->RefList[i].Index];
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

		//Export generic functions
		for (std::size_t i = 0; i < g->RefList.size(); ++i)
		{
			if ((g->RefList[i].Type & REF_REFTYPES) != REF_FUNC_G_CONSTRAINT) continue;
			auto& name = g->NamesList[g->RefList[i].Index];
			if (name.compare(0, prefix.length(), prefix) == 0)
			{
				auto funcId = FindConstraintExportGenericFunction(cache, name.substr(prefix.length()));
				if (funcId != SIZE_MAX)
				{
					ConstraintExportListEntry entry;
					entry.EntryType = CONSTRAINT_EXPORT_GENERICFUNCTION;
					entry.Index = i;
					assert(cache->Target.CType == CTT_RT && cache->Target.DeterminedType != nullptr);
					entry.GenericFunction = { cache->Target.DeterminedType, funcId };
					exportList->push_back(entry);
				}
			}
		}

		//Export field
		for (std::size_t i = 0; i < g->RefList.size(); ++i)
		{
			if ((g->RefList[i].Type & REF_REFTYPES) != REF_FIELD_CONSTRAINT) continue;
			auto& name = g->NamesList[g->RefList[i].Index];
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

	bool FindConstraintExportType(ConstraintCalculationCache* cache, const std::string& name, RuntimeType** result)
	{
		if (name.length() == 0) return false;
		auto& constraintName = cache->Source->ExportName;
		auto slash = name.find('/');
		if (slash == 0) return false;
		if (slash == std::string::npos)
		{
			if (name == ".target")
			{
				*result = cache->Target.DeterminedType;
				return true;
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
						assert(ct.CType == CTT_RT);
						*result = ct.DeterminedType;
						return true;
					}
				}
				return false;
			default:
				return false;
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
						return FindConstraintExportType(cache->Children[i].get(), name.substr(slash + 1), result);
					}
				}
				return false;
			}
			default:
				return false;
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

	std::size_t FindConstraintExportGenericFunction(ConstraintCalculationCache* cache, const std::string& name)
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
				for (std::size_t i = 0; i < cache->Trait->GenericFunctions.size(); ++i)
				{
					auto& e = cache->Trait->GenericFunctions[i];
					auto& tf = cache->TraitGenericFunctions[i];
					if (name == e.ExportName)
					{
						return tf.Overloads[tf.CurrentOverload].Index;
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
						return FindConstraintExportGenericFunction(cache->Children[i].get(), name.substr(slash + 1));
					}
				}
			}
			default:
				return SIZE_MAX;
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

} }

namespace RolLang {

using ConstraintObjects::RuntimeLoaderConstraint;

inline bool RuntimeLoaderCore::CheckConstraints(const std::string& srcAssebly, GenericDeclaration* g,
	const MultiList<RuntimeType*>& args, ConstraintExportList* exportList)
{
	auto l = static_cast<RuntimeLoaderConstraint*>(this);
	return l->CheckConstraintsImpl(srcAssebly, g, args, exportList);
}

}
