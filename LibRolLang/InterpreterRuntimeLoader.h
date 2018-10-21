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
		FindPointerTypeId();
	}

	struct InterpreterRuntimeFunctionInfo
	{
		NativeFunction FunctionPtr;
		void* FunctionData;
		RuntimeFunction* STInfo;
	};

private:
	void FindPointerTypeId()
	{
		auto a = FindAssemblyNoThrow("Core");
		if (a != nullptr)
		{
			for (auto& e : a->ExportTypes)
			{
				if (e.ExportName == "Core.Pointer")
				{
					_pointerTypeId = e.InternalId;
					return;
				}
			}
		}
		//This is actually an error, but we don't want to throw in ctor.
		//Let's wait for the type loading to fail.
		_pointerTypeId = SIZE_MAX;
	}

public:
	RuntimeType* LoadPointerType(RuntimeType* t, std::string& err)
	{
		assert(t->PointerType == nullptr);
		LoadingArguments args;
		args.Assembly = "Core";
		args.Id = _pointerTypeId;
		args.Arguments.push_back(t);
		return GetType(args, err);
	}

protected:
	virtual void OnTypeLoaded(RuntimeType* type) override
	{
		if (type->Args.Assembly == "Core" && type->Args.Id == _pointerTypeId)
		{
			assert(type->Args.Arguments.size() == 1);
			auto elementType = type->Args.Arguments[0];
			assert(elementType->PointerType == nullptr);
			elementType->PointerType = type;
		}
	}

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
		auto a = FindAssemblyNoThrow(assemblyName);
		if (a == nullptr) return false;
		auto i = FindNativeIdNoThrow(a->NativeFunctions, name);
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
	std::size_t _pointerTypeId;
};
