#pragma once
#include "InterpreterBase.h"

class Interpreter : public InterpreterBase
{
	using FuncInfo = InterpreterRuntimeLoader::InterpreterRuntimeFunctionInfo;

public:
	Interpreter(AssemblyList assemblies) : InterpreterBase(assemblies, InterpreterEntry)
	{
	}

private:
	static bool InterpreterEntry(Interpreter* i, void* data)
	{
		//TODO note that no Interpreter can pass this bondary.
		//Maybe we need an internal function to 'proceed' the function and
		//throw exception when needed and caught somewhere else.
		return false;
	}

public:
	bool Call(std::size_t id)
	{
		auto i = _functionCache.find(id);
		if (i == _functionCache.end())
		{
			FuncInfo info;
			if (!_loader->TryFindFunctionInfoById(id, &info))
			{
				ReturnWithException({}, ERR_API, "Invalid function id");
				return false;
			}
			_functionCache[id] = info;
			return DoCall(info);
		}
		return DoCall(i->second);
	}

private:
	bool DoCall(const FuncInfo& i)
	{
		if (i.FunctionData != InterpreterEntry)
		{
			_stacktracer.BeginNativeFrame(i.STInfo, i.FunctionPtr);
		}
		//TODO still need to ensure no exception thrown
		auto ret = i.FunctionPtr(this, i.FunctionData);
		if (i.FunctionData != InterpreterEntry)
		{
			_stacktracer.EndNativeFrame();
		}
		return ret;
	}

private:
	std::unordered_map<std::size_t, FuncInfo> _functionCache;
};
