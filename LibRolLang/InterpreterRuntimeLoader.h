#pragma once
#include <cassert>
#include "RuntimeLoader.h"
#include "InterpreterCommon.h"

struct NativeWrapperData
{
	virtual ~NativeWrapperData() {}
};

struct NativeFunctionDeclaration
{
	std::string AssemblyName;
	std::size_t Id;
	NativeFunction FunctionPtr;
	void* Data;
};

class InterpreterRuntimeLoader : public RuntimeLoader
{
public:
	InterpreterRuntimeLoader(NativeFunction interpreterWrapper, AssemblyList assemblies)
		: RuntimeLoader(std::move(assemblies)), _interpreterWrapper(interpreterWrapper)
	{
	}

	struct InterpreterRuntimeFunctionInfo
	{
		NativeFunction EntryPtr;
		void* Data;
		RuntimeFunction* RuntimePtr;
	};

	struct InterpreterRuntimeTypeInfo
	{
		void* StaticPointer; //Memory block for TSM_GLOBAL.
		bool Valid; //Remove this once we have other fields to use
	};

protected:
	virtual void OnTypeLoaded(RuntimeType* type) override
	{
		InterpreterRuntimeTypeInfo info = {};
		info.Valid = true;

		//Allocate global storage before returning
		if (type->Storage == TSM_GLOBAL)
		{
			info.StaticPointer = AllocateNewGlobalStorage(type);
		}

		if (type->TypeId >= _interpreterTypeInfo.size())
		{
			while (type->TypeId > _interpreterTypeInfo.size())
			{
				_interpreterTypeInfo.push_back({});
			}
			_interpreterTypeInfo.push_back(info);
		}
		else
		{
			_interpreterTypeInfo[type->TypeId] = info;
		}
	}

	virtual void OnFunctionLoaded(RuntimeFunction* func) override
	{
		InterpreterRuntimeFunctionInfo info;
		info.RuntimePtr = func;
		info.EntryPtr = nullptr;
		for (auto& n : _nativeFunctions)
		{
			if (func->Args.Assembly == n.AssemblyName && func->Args.Id == n.Id)
			{
				if (func->Args.Arguments.size() != 0)
				{
					throw RuntimeLoaderException("Invalid native function.");
				}
				info.EntryPtr = n.FunctionPtr;
				info.Data = n.Data;
				break;
			}
		}
		if (info.EntryPtr == nullptr)
		{
			info.EntryPtr = _interpreterWrapper;
			info.Data = func;
		}
		if (func->FunctionId >= _interpreterFuncInfo.size())
		{
			while (func->FunctionId > _interpreterFuncInfo.size())
			{
				_interpreterFuncInfo.push_back({});
			}
			_interpreterFuncInfo.push_back(info);
		}
		else
		{
			_interpreterFuncInfo[func->FunctionId] = info;
		}
	}

private:
	static void* AllocateNewGlobalStorage(RuntimeType* type)
	{
		std::size_t alignment = type->GetStorageAlignment();
		std::size_t totalSize = type->GetStorageSize() + alignment;
		std::unique_ptr<char[]> ptr = std::make_unique<char[]>(totalSize);
		uintptr_t rawPtr = (uintptr_t)ptr.get();
		uintptr_t alignedPtr = (rawPtr + alignment - 1) / alignment * alignment;
		return (void*)alignedPtr;
	}

public:
	bool GetNativeFunctionByName(const std::string& assemblyName, const std::string& name,
		Function** f, std::size_t* id)
	{
		auto a = FindAssembly(assemblyName);
		if (a == nullptr) return false;
		auto i = FindNativeId(a->NativeFunctions, name);
		if (i == SIZE_MAX) return false;
		if (i >= a->Functions.size()) return false;
		*f = &a->Functions[i];
		*id = i;
		return true;
	}

	void AddNativeFunction(const std::string& assemblyName, std::size_t id,
		NativeFunction functionPtr, void* userData)
	{
		std::lock_guard<Spinlock> lock(_loaderLock);
		_nativeFunctions.push_back({ assemblyName, id, functionPtr, userData });
	}

	void AddNativeWrapperData(std::unique_ptr<NativeWrapperData> data)
	{
		std::lock_guard<Spinlock> lock(_loaderLock);
		_nativeWrapperData.emplace_back(std::move(data));
	}

	bool TryFindFunctionInfoById(std::size_t id, InterpreterRuntimeFunctionInfo* r)
	{
		std::lock_guard<Spinlock> lock(_loaderLock);
		if (r == nullptr)
		{
			return false;
		}
		if (id >= _interpreterFuncInfo.size())
		{
			return false;
		}
		auto& info = _interpreterFuncInfo[id];
		if (info.EntryPtr == nullptr)
		{
			return false;
		}
		*r = info;
		return true;
	}

	bool TryFindTypeInfoById(std::size_t id, InterpreterRuntimeTypeInfo* r)
	{
		std::lock_guard<Spinlock> lock(_loaderLock);
		if (r == nullptr)
		{
			return false;
		}
		if (id >= _interpreterTypeInfo.size())
		{
			return false;
		}
		auto& info = _interpreterTypeInfo[id];
		if (!info.Valid)
		{
			return false;
		}
		*r = info;
		return true;
	}

private:
	NativeFunction _interpreterWrapper;
	std::vector<NativeFunctionDeclaration> _nativeFunctions;
	std::vector<InterpreterRuntimeFunctionInfo> _interpreterFuncInfo;
	std::vector<InterpreterRuntimeTypeInfo> _interpreterTypeInfo;
	std::vector<std::unique_ptr<NativeWrapperData>> _nativeWrapperData;
};
