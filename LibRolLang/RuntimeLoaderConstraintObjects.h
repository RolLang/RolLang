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
	CTT_PARAMETER, //Used only for generic function matching
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
	std::size_t ParameterSegment, ParameterIndex;

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

	static ConstraintCheckType Parameter(ConstraintCalculationCacheRoot* root, std::size_t s, std::size_t i, std::string name = "")
	{
		ConstraintCheckType ret(root, CTT_PARAMETER);
		ret.ParameterSegment = s;
		ret.ParameterIndex = i;
		ret.SubtypeName = name;
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
		//TODO check the CTT allows to deduct
		assert(CLevel == 0);
		OType = CType;
		CLevel = Root->GetCurrentLevel();
		CType = CTT_FAIL;
		Root->BacktrackList.push_back(this);
	}

	void DeductRT(RuntimeType* rt)
	{
		//TODO check the CTT allows to deduct
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
	std::vector<ConstraintCheckType> ConstraintEqualTypes;
	std::vector<TraitCacheFunctionConstrainExportInfo> ExportTypes;
};
struct TraitCacheFunctionInfo
{
	std::vector<TraitCacheFunctionOverloadInfo> Overloads;
	std::size_t CurrentOverload;
	ConstraintCheckType TraitReturnType;
	std::vector<ConstraintCheckType> TraitParameterTypes;

	std::vector<ConstraintCheckType> TraitConstraintEqualTypes;
	std::vector<std::size_t> AdditionalGenericNumber;
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
	std::vector<TraitCacheFunctionInfo> TraitGenericFunctions;
	std::vector<ConstraintCheckType> TraitFunctionUndetermined;
};

struct TypeTemplateFunctionEntry
{
	const std::string& Name;
	std::size_t Index;
};

struct TypeTemplateFunctionIterator
{
public:
	TypeTemplateFunctionIterator(Type* tt, std::size_t stage)
		: _typeTemplate(tt), _stage(stage), _interfaceIndex(0), _functionIndex(0)
	{
	}

public:
	bool operator!=(const TypeTemplateFunctionIterator& rhs) const
	{
		assert(_typeTemplate == rhs._typeTemplate);
		return _stage != rhs._stage ||
			_interfaceIndex != rhs._interfaceIndex ||
			_functionIndex != rhs._functionIndex;
	}

	TypeTemplateFunctionIterator& operator++()
	{
		switch (_stage)
		{
		case 0:
			_stage = 1;
			_functionIndex = SIZE_MAX;
			//fall through
		case 1:
			if (++_functionIndex < _typeTemplate->PublicFunctions.size())
			{
				break;
			}
			_functionIndex = SIZE_MAX;
			_stage = 2;
			//fall through
		case 2:
			if (++_functionIndex < _typeTemplate->Base.VirtualFunctions.size())
			{
				break;
			}
			if (_typeTemplate->Interfaces.size() == 0)
			{
				_stage = 4;
				break;
			}
			_interfaceIndex = 0;
			_functionIndex = SIZE_MAX;
			_stage = 3;
			//fall through
		case 3:
			//TODO make the logic more clear
			while (1)
			{
				if (++_functionIndex == _typeTemplate->Interfaces[_interfaceIndex].VirtualFunctions.size())
				{
					_functionIndex = SIZE_MAX;
					if (++_interfaceIndex == _typeTemplate->Interfaces.size())
					{
						_stage = 4;
						break;
					}
					//continue loop
				}
				else
				{
					break;
				}
			}
			break;
		case 4:
			break;
		}
		return *this;
	}

	TypeTemplateFunctionEntry operator*() const
	{
		assert(_stage > 0);
		assert(_stage < 4);
		switch (_stage)
		{
		case 1:
		{
			auto& e = _typeTemplate->PublicFunctions[_functionIndex];
			return { e.Name, e.Id };
		}
		case 2:
		{
			auto& e = _typeTemplate->Base.VirtualFunctions[_functionIndex];
			//Bind to the virtual version.
			return { e.Name, e.VirtualFunction };
		}
		default: //3
		{
			auto& e = _typeTemplate->Interfaces[_interfaceIndex].VirtualFunctions[_functionIndex];
			return { e.Name, e.VirtualFunction };
		}
		}
	}

private:
	Type* _typeTemplate;
	std::size_t _stage;
	std::size_t _interfaceIndex;
	std::size_t _functionIndex;
};

struct TypeTemplateFunctionList
{
	TypeTemplateFunctionList(Type* tt)
		: typeTemplate(tt)
	{
	}

	TypeTemplateFunctionIterator begin()
	{
		return ++TypeTemplateFunctionIterator(typeTemplate, 0);
	}

	TypeTemplateFunctionIterator end()
	{
		return { typeTemplate, 4 };
	}

private:
	Type* typeTemplate;
};

} }
