#pragma once
#include <vector>
#include "RuntimeLoader.h"
#include "InterpreterCommon.h"

class InterpreterStacktracer
{
	struct StacktracerFrameInfo
	{
		RuntimeFunction* Function;
		std::size_t* PC;
		NativeFunction Native;
	};

public:
	void BeginFrameInterpreted(RuntimeFunction* func, std::size_t* pc)
	{
		_stack.push_back({ func, pc, nullptr });
	}

	void EndFrameInterpreted()
	{
		_stack.pop_back();
	}

	void BeginNativeFrame(RuntimeFunction* func, NativeFunction n)
	{
		_stack.push_back({ func, nullptr, n });
	}

	void EndNativeFrame()
	{
		_stack.pop_back();
	}

	StacktraceInfo GetStacktrace()
	{
		StacktraceInfo ret;
		for (auto& f : _stack)
		{
			ret.push_back({ f.Function, f.PC ? *f.PC : SIZE_MAX });
		}
		return std::move(ret);
	}

private:
	std::vector<StacktracerFrameInfo> _stack;
};
