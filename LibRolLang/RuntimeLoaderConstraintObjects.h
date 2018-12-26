#pragma once
#include "LoaderObjects.h"

namespace RolLang { namespace ConstraintObjects {

enum ConstraintCheckTypeType
{
	CTT_FAIL,
	CTT_ANY,
	CTT_GENERIC,
	CTT_SUBTYPE,
	CTT_RT,
};

struct ConstraintUndeterminedTypeInfo
{
	RuntimeType* Result;
	bool IsDetermined;
};
struct ConstraintUndeterminedTypeSource
{
	std::vector<ConstraintUndeterminedTypeInfo> UndeterminedTypes;
};

struct ConstraintCheckType;
struct ConstraintCalculationCacheRoot
{
	ConstraintUndeterminedTypeSource* UndeterminedList;
	std::size_t Size;
	std::vector<ConstraintCheckType*> BacktrackList;
	std::vector<std::size_t> BacktrackListSize;

	ConstraintCalculationCacheRoot(ConstraintUndeterminedTypeSource& udList)
		: UndeterminedList(&udList), Size(0)
	{
	}

	void Clear()
	{
		Size = 0;
		BacktrackList.clear();
		BacktrackListSize.clear();
	}

	RuntimeType* GetDetermined(std::size_t i)
	{
		return UndeterminedList->UndeterminedTypes[i].Result;
	}

	bool IsDetermined(std::size_t i)
	{
		return UndeterminedList->UndeterminedTypes[i].IsDetermined;
	}

	void Determine(std::size_t i, RuntimeType* t)
	{
		auto& e = UndeterminedList->UndeterminedTypes[i];
		e.Result = t;
		e.IsDetermined = true;
	}

	std::size_t StartBacktrackPoint()
	{
		auto id = BacktrackListSize.size();
		BacktrackListSize.push_back(BacktrackList.size());
		return id;
	}

	inline void DoBacktrack(std::size_t level);

	std::size_t GetCurrentLevel()
	{
		return BacktrackListSize.size();
	}
};

struct ConstraintCheckType
{
	ConstraintCalculationCacheRoot* Root;
	ConstraintCheckTypeType CType;

	RuntimeType* DeterminedType;
	std::string TypeTemplateAssembly;
	std::size_t TypeTemplateIndex;
	std::string SubtypeName;
	MultiList<ConstraintCheckType> Args;
	std::size_t UndeterminedId;
	bool TryArgumentConstraint;
	std::vector<ConstraintCheckType> ParentType; //TODO any better idea?

	//Following 2 fields are related to backtracking.
	ConstraintCheckTypeType OType;
	std::size_t CLevel;

public:
	ConstraintCheckType()
		: ConstraintCheckType(nullptr, CTT_FAIL)
	{
	}
private:
	ConstraintCheckType(ConstraintCalculationCacheRoot* root, ConstraintCheckTypeType ctt)
		: Root(root), CType(ctt), DeterminedType(nullptr), TryArgumentConstraint(false), OType(CTT_FAIL), CLevel(0)
	{
	}

public:
	static ConstraintCheckType Fail(ConstraintCalculationCacheRoot* root)
	{
		return { root, CTT_FAIL };
	}

	static ConstraintCheckType Determined(ConstraintCalculationCacheRoot* root, RuntimeType* rt)
	{
		ConstraintCheckType ret(root, CTT_RT);
		ret.DeterminedType = rt;
		return ret;
	}

	static ConstraintCheckType Undetermined(ConstraintCalculationCacheRoot* root)
	{
		ConstraintCheckType ret(root, CTT_ANY);
		ret.UndeterminedId = root->UndeterminedList->UndeterminedTypes.size();
		root->UndeterminedList->UndeterminedTypes.push_back({});
		return ret;
	}

	static ConstraintCheckType Generic(ConstraintCalculationCacheRoot* root, const std::string& a, std::size_t i)
	{
		ConstraintCheckType ret(root, CTT_GENERIC);
		ret.TypeTemplateAssembly = a;
		ret.TypeTemplateIndex = i;
		return ret;
	}

	static ConstraintCheckType Subtype(ConstraintCalculationCacheRoot* root, const std::string& n)
	{
		ConstraintCheckType ret(root, CTT_SUBTYPE);
		ret.SubtypeName = n;
		return ret;
	}

	static ConstraintCheckType Try(ConstraintCheckType&& t)
	{
		auto ret = ConstraintCheckType(std::move(t));
		ret.TryArgumentConstraint = true;
		return ret;
	}

	static ConstraintCheckType Empty(ConstraintCalculationCacheRoot* root)
	{
		return Determined(root, nullptr);
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
		DeterminedType = rt;
		Root->BacktrackList.push_back(this);
	}
};

inline void ConstraintCalculationCacheRoot::DoBacktrack(std::size_t level)
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
			t->DeterminedType = nullptr;
		}
		BacktrackList.pop_back();
	}
}

struct TraitCacheFieldInfo
{
	ConstraintCheckType Type;
	ConstraintCheckType TypeInTarget;
	std::size_t FieldIndex;
};

struct TraitCacheFunctionConstrainExportInfo
{
	std::size_t NameIndex;
	ConstraintCheckType UndeterminedType;
};

struct TraitCacheFunctionOverloadInfo
{
	std::size_t Index;
	std::string FunctionTemplateAssembly;
	Function* FunctionTemplate;
	MultiList<ConstraintCheckType> GenericArguments;
	ConstraintCheckType ReturnType;
	std::vector<ConstraintCheckType> ParameterTypes;
	std::vector<TraitCacheFunctionConstrainExportInfo> ExportTypes;
};
struct TraitCacheFunctionInfo
{
	std::vector<TraitCacheFunctionOverloadInfo> Overloads;
	std::size_t CurrentOverload;
	ConstraintCheckType TraitReturnType;
	std::vector<ConstraintCheckType> TraitParameterTypes;
};

struct ConstraintCalculationCache
{
	ConstraintCalculationCacheRoot* Root;
	ConstraintCalculationCache* Parent;

	GenericConstraint* Source;
	MultiList<ConstraintCheckType> CheckArguments;
	ConstraintCheckType CheckTarget;

	std::string SrcAssembly;
	ConstraintCheckType Target;
	MultiList<ConstraintCheckType> Arguments;

	//Following fields are only for trait constraints.
	std::vector<std::unique_ptr<ConstraintCalculationCache>> Children;
	bool TraitCacheCreated;
	bool TraitMemberResolved;
	Trait* Trait;
	std::string TraitAssembly;
	std::vector<TraitCacheFieldInfo> TraitFields;
	std::vector<TraitCacheFunctionInfo> TraitFunctions;
	std::vector<ConstraintCheckType> TraitFunctionUndetermined;
};

} }
