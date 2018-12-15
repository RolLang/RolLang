#pragma once
#include "LoaderObjects.h"
#include "Spinlock.h"
#include <deque>

namespace RolLang {

struct RuntimeLoaderLoadingData
{
public:
	void ClearLoadingLists()
	{
		_loadingTypes.clear();
		_loadingFunctions.clear();
		_loadingRefTypes.clear();
		_postLoadingTypes.clear();
		_finishedLoadingTypes.clear();
		_finishedLoadingFunctions.clear();
		_constrainCheckingTypes.clear();
		_constrainCheckingFunctions.clear();
	}

	void CheckLoadingSizeLimit(std::size_t limit)
	{
		if (_loadingTypes.size() + _loadingFunctions.size() +
			_loadingSubtypes.size() > limit)
		{
			throw RuntimeLoaderException("Loading object limit exceeded.");
		}
	}

public:
	std::vector<RuntimeType*> _loadingTypes;
	std::vector<SubMemberLoadingArguments> _loadingSubtypes;
	std::vector<LoadingArguments> _constrainCheckingTypes;
	std::vector<LoadingArguments> _constrainCheckingFunctions;

	//Loading queues. We need to keep order.
	std::deque<std::unique_ptr<RuntimeType>> _loadingRefTypes;
	std::deque<std::unique_ptr<RuntimeType>> _postLoadingTypes;
	std::deque<std::unique_ptr<RuntimeFunction>> _loadingFunctions;
	std::deque<std::unique_ptr<RuntimeType>> _finishedLoadingTypes;
	std::deque<std::unique_ptr<RuntimeFunction>> _finishedLoadingFunctions;
};

template <typename T>
struct LoadingStackScopeGuard
{
	LoadingStackScopeGuard(std::vector<T>& list, const T& newItem)
		: _list(list), _newItem(&newItem)
	{
		for (auto& item : list)
		{
			if (item == newItem)
			{
				throw RuntimeLoaderException("Circular reference detected");
			}
		}
		list.push_back(newItem);
	}

	~LoadingStackScopeGuard()
	{
		assert(_list.back() == *_newItem);
		_list.pop_back();
	}

	std::vector<T>& _list;
	const T* _newItem;
};

struct RuntimeLoaderData
{
public:
	virtual ~RuntimeLoaderData() {}

public:
	bool TryFindInternalTypeId()
	{
		if (auto a = FindAssemblyNoThrow("Core"))
		{
			for (auto& e : a->ExportTypes)
			{
				if (e.ExportName == "Core.Pointer")
				{
					if (e.InternalId >= a->Types.size() ||
						!CheckPointerTypeTemplate(&a->Types[e.InternalId]) ||
						_pointerTypeId != SIZE_MAX)
					{
						return false;
					}
					_pointerTypeId = e.InternalId;
				}
				else if (e.ExportName == "Core.Box")
				{
					if (e.InternalId >= a->Types.size() ||
						!CheckBoxTypeTemplate(&a->Types[e.InternalId]) ||
						_boxTypeId != SIZE_MAX)
					{
						return false;
					}
					_boxTypeId = e.InternalId;
				}
				else if (e.ExportName == "Core.Reference")
				{
					if (e.InternalId >= a->Types.size() ||
						!CheckReferenceTypeTemplate(&a->Types[e.InternalId]) ||
						_referenceTypeId != SIZE_MAX)
					{
						return false;
					}
					_referenceTypeId = e.InternalId;
				}
				else if (e.ExportName == "Core.Embed")
				{
					if (e.InternalId >= a->Types.size() ||
						!CheckEmbedTypeTemplate(&a->Types[e.InternalId]) ||
						_embedTypeId != SIZE_MAX)
					{
						return false;
					}
					_embedTypeId = e.InternalId;
				}
			}
		}
		return true;
	}

	void FindInternalTypeId()
	{
		_pointerTypeId = _boxTypeId = _referenceTypeId = _embedTypeId = SIZE_MAX;
		if (!TryFindInternalTypeId())
		{
			_pointerTypeId = _boxTypeId = _referenceTypeId = _embedTypeId = SIZE_MAX;
		}
	}

	bool CheckPointerTypeTemplate(Type* t)
	{
		if (!t->Generic.ParameterCount.IsSingle()) return false;
		if (t->GCMode != TSM_VALUE) return false;
		return true;
	}

	bool CheckBoxTypeTemplate(Type* t)
	{
		if (!t->Generic.ParameterCount.IsSingle()) return false;
		if (t->GCMode != TSM_REFERENCE) return false;
		return true;
	}

	bool CheckReferenceTypeTemplate(Type* t)
	{
		if (!t->Generic.ParameterCount.IsSingle()) return false;
		if (t->GCMode != TSM_VALUE) return false;
		return true;
	}

	bool CheckEmbedTypeTemplate(Type* t)
	{
		if (!t->Generic.ParameterCount.IsSingle()) return false;
		if (t->GCMode != TSM_VALUE) return false;
		return true;
	}

public:
	std::shared_ptr<RuntimeFunctionCode> GetCode(const std::string& a, std::size_t id)
	{
		for (auto& c : _codeStorage.Data)
		{
			if (c->AssemblyName == a && c->Id == id)
			{
				return c;
			}
		}
		auto f = FindFunctionTemplate(a, id);
		if (f->Instruction.size() == 0 && f->ConstantData.size() == 0 &&
			f->ConstantTable.size() == 0)
		{
			return nullptr;
		}
		auto ret = std::make_shared<RuntimeFunctionCode>();
		ret->AssemblyName = a;
		ret->Id = id;
		ret->Instruction = f->Instruction;
		ret->ConstantData = f->ConstantData;
		ret->ConstantTable = f->ConstantTable;
		ret->LocalVariables = f->Locals;

		//Append some nop at the end
		for (int i = 0; i < 16; ++i)
		{
			ret->Instruction.push_back(OP_NOP);
		}

		//Process import constant
		auto assembly = FindAssemblyThrow(a);
		for (auto& k : ret->ConstantTable)
		{
			if (k.Length == 0)
			{
				auto kid = k.Offset;
				auto value = LoadImportConstant(assembly, kid);
				auto offset = ret->ConstantData.size();
				auto pValue = (unsigned char*)&value;
				ret->ConstantData.insert(ret->ConstantData.end(), pValue, pValue + 4);
				k.Length = 4;
				k.Offset = offset;
			}
		}

		_codeStorage.Data.push_back(ret);
		return std::move(ret);
	}

	void AddLoadedType(std::unique_ptr<RuntimeType> t)
	{
		SetValueInList(_loadedTypes, t->TypeId, std::move(t));
	}

	void AddLoadedFunction(std::unique_ptr<RuntimeFunction> f)
	{
		SetValueInList(_loadedFunctions, f->FunctionId, std::move(f));
	}

public:
	Assembly* FindAssemblyNoThrow(const std::string& name)
	{
		for (auto& a : _assemblies.Assemblies)
		{
			if (a.AssemblyName == name)
			{
				return &a;
			}
		}
		return nullptr;
	}

	Assembly* FindAssemblyThrow(const std::string& name)
	{
		auto ret = FindAssemblyNoThrow(name);
		if (ret == nullptr)
		{
			throw RuntimeLoaderException("Referenced assembly not found");
		}
		return ret;
	}

	std::size_t FindNativeIdNoThrow(const std::vector<AssemblyExport>& list,
		const std::string name)
	{
		for (std::size_t i = 0; i < list.size(); ++i)
		{
			if (list[i].ExportName == name)
			{
				return list[i].InternalId;
			}
		}
		return SIZE_MAX;
	}

	std::size_t FindNativeIdThrow(const std::vector<AssemblyExport>& list,
		const std::string name)
	{
		auto ret = FindNativeIdNoThrow(list, name);
		if (ret == SIZE_MAX)
		{
			throw RuntimeLoaderException("Native object not found");
		}
		return ret;
	}

	Type* FindTypeTemplate(const LoadingArguments& args)
	{
		auto a = FindAssemblyThrow(args.Assembly);
		if (args.Id >= a->Types.size())
		{
			throw RuntimeLoaderException("Invalid type reference");
		}
		return &a->Types[args.Id];
	}

	Function* FindFunctionTemplate(const std::string& assembly, std::size_t id)
	{
		auto a = FindAssemblyThrow(assembly);
		if (id >= a->Functions.size())
		{
			throw RuntimeLoaderException("Invalid function reference");
		}
		return &a->Functions[id];
	}

public:
	bool FindExportType(const AssemblyImport& args, LoadingArguments& result)
	{
		auto a = FindAssemblyThrow(args.AssemblyName);
		for (auto& e : a->ExportTypes)
		{
			if (e.ExportName == args.ImportName)
			{
				if (e.InternalId >= a->Types.size())
				{
					auto importId = e.InternalId - a->Types.size();
					if (importId >= a->ImportTypes.size())
					{
						return false;
					}
					return FindExportType(a->ImportTypes[importId], result);
				}
				if (a->Types[e.InternalId].Generic.ParameterCount != args.GenericParameters)
				{
					return false;
				}
				result.Assembly = args.AssemblyName;
				result.Id = e.InternalId;
				return true;
			}
		}
		return false;
	}

	bool FindExportFunction(const AssemblyImport& args, LoadingArguments& result)
	{
		auto a = FindAssemblyThrow(args.AssemblyName);
		for (auto& e : a->ExportFunctions)
		{
			if (e.ExportName == args.ImportName)
			{
				if (e.InternalId >= a->Functions.size())
				{
					auto importId = e.InternalId - a->Functions.size();
					if (importId >= a->ImportFunctions.size())
					{
						return false;
					}
					return FindExportFunction(a->ImportFunctions[importId], result);
				}
				if (a->Functions[e.InternalId].Generic.ParameterCount != args.GenericParameters)
				{
					return false;
				}
				result.Assembly = args.AssemblyName;
				result.Id = e.InternalId;
				return true;
			}
		}
		return false;
	}

	bool FindExportConstant(const AssemblyImport& args, std::uint32_t& result)
	{
		auto a = FindAssemblyThrow(args.AssemblyName);
		for (auto& e : a->ExportConstants)
		{
			if (e.ExportName == args.ImportName)
			{
				if (e.InternalId >= a->Constants.size())
				{
					auto importId = e.InternalId - a->Constants.size();
					if (importId >= a->ImportConstants.size())
					{
						return false;
					}
					return FindExportConstant(a->ImportConstants[importId], result);
				}
				result = a->Constants[e.InternalId];
				return true;
			}
		}
		return false;
	}

	bool FindExportTrait(const AssemblyImport& args, LoadingArguments& result)
	{
		auto a = FindAssemblyThrow(args.AssemblyName);
		for (auto& e : a->ExportTraits)
		{
			if (e.ExportName == args.ImportName)
			{
				if (e.InternalId >= a->Traits.size())
				{
					auto importId = e.InternalId - a->Traits.size();
					if (importId >= a->ImportTraits.size())
					{
						return false;
					}
					return FindExportTrait(a->ImportTraits[importId], result);
				}
				if (a->Traits[e.InternalId].Generic.ParameterCount != args.GenericParameters)
				{
					return false;
				}
				result.Assembly = args.AssemblyName;
				result.Id = e.InternalId;
				return true;
			}
		}
		return false;
	}

	std::uint32_t FindExportConstant(const std::string& assemblyName, const std::string& n)
	{
		std::uint32_t ret;
		if (!FindExportConstant({ assemblyName, n, {} }, ret))
		{
			throw RuntimeLoaderException("Constant export not found");
		}
		return ret;
	}

	std::uint32_t LoadImportConstant(Assembly* a, std::size_t index)
	{
		if (index >= a->ImportConstants.size())
		{
			throw RuntimeLoaderException("Invalid constant import");
		}
		auto info = a->ImportConstants[index];
		if (!info.GenericParameters.IsEmpty())
		{
			throw RuntimeLoaderException("Invalid constant import");
		}
		return FindExportConstant(info.AssemblyName, info.ImportName);
	}

public:
	template <typename T>
	static void SetValueInList(std::vector<T>& v, std::size_t n, T&& val)
	{
		while (v.size() < n)
		{
			v.push_back({});
		}
		if (v.size() == n)
		{
			v.push_back(std::forward<T>(val));
		}
		else
		{
			assert(v[n] == T());
			v[n] = std::forward<T>(val);
		}
	}

public:
	RuntimeLoader* _loader;
	AssemblyList _assemblies;

	std::size_t _ptrSize, _itabPtrSize;
	std::size_t _loadingLimit;

	std::vector<std::unique_ptr<RuntimeType>> _loadedTypes;
	std::vector<std::unique_ptr<RuntimeFunction>> _loadedFunctions;
	RuntimeFunctionCodeStorage _codeStorage;

	std::size_t _nextFunctionId = 1, _nextTypeId = 1;
	std::size_t _pointerTypeId, _boxTypeId, _referenceTypeId, _embedTypeId;

	RuntimeLoaderLoadingData* _loading;
};

}
