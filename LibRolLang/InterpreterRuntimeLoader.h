#pragma once
#include <cassert>
#include "RuntimeLoader.h"

class Interpreter;
typedef bool(*NativeFunction)(Interpreter* interpreter, void* userData);

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
		NativeFunction FunctionPtr;
		void* FunctionData;
		RuntimeFunction* STInfo;
	};

protected:
	virtual void OnFunctionLoaded(RuntimeFunction* func) override
	{
		InterpreterRuntimeFunctionInfo info;
		info.STInfo = func;
		info.FunctionPtr = nullptr;
		for (auto& n : _nativeFunctions)
		{
			if (func->Args.Assembly == n.AssemblyName && func->Args.Id == n.Id)
			{
				if (func->Args.Arguments.size() != 0)
				{
					throw RuntimeLoaderException("Invalid native function.");
				}
				info.FunctionPtr = n.FunctionPtr;
				info.FunctionData = n.Data;
				break;
			}
		}
		if (info.FunctionPtr == nullptr)
		{
			info.FunctionPtr = _interpreterWrapper;
			info.FunctionData = func;
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

	bool GetFunctionInfoById(std::size_t id, InterpreterRuntimeFunctionInfo* r)
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
		if (info.FunctionPtr == nullptr)
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
	std::vector<std::unique_ptr<NativeWrapperData>> _nativeWrapperData;
};
