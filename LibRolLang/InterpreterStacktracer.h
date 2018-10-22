#pragma once
#include <vector>
#include "RuntimeLoader.h"
#include "InterpreterCommon.h"

class InterpreterStacktracer
{
	struct StacktracerFrameInfo
	{
		RuntimeFunction* Function;
		std::size_t PC;
		NativeFunction Native;
	};

public:
	void BeginFrameInterpreted(RuntimeFunction* func)
	{
		_stack.push_back({ func, 0, nullptr });
	}

	void EndFrameInterpreted()
	{
		_stack.pop_back();
	}

	void BeginNativeFrame(RuntimeFunction* func, NativeFunction n)
	{
		_stack.push_back({ func, SIZE_MAX, n });
	}

	void EndNativeFrame()
	{
		_stack.pop_back();
	}

	void SetReturnAddress(std::size_t pc)
	{
		_stack[_stack.size() - 1].PC = pc;
	}

	void GetReturnAddress(RuntimeFunction** f, std::size_t* pc)
	{
		*f = _stack.back().Function;
		*pc = _stack.back().PC;
	}

	StacktraceInfo GetStacktrace()
	{
		StacktraceInfo ret;
		for (auto& f : _stack)
		{
			ret.push_back({ f.Function->Args.ConvertToSymbol(), f.PC });
		}
		return std::move(ret);
	}

private:
	std::vector<StacktracerFrameInfo> _stack;
};
