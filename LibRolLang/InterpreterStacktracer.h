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
		std::size_t StackBase;
		NativeFunction Native;
	};

public:
	void BeginFrameInterpreted(RuntimeFunction* func)
	{
		_stack.push_back({ func, 0, 0, nullptr });
	}

	void EndFrameInterpreted()
	{
		_stack.pop_back();
	}

	void BeginNativeFrame(RuntimeFunction* func, NativeFunction n)
	{
		_stack.push_back({ func, SIZE_MAX, 0, n });
	}

	void EndNativeFrame()
	{
		_stack.pop_back();
	}

	void SetReturnAddress(std::size_t pc, std::size_t stack)
	{
		_stack[_stack.size() - 1].PC = pc;
		_stack[_stack.size() - 1].StackBase = stack;
	}

	void GetReturnAddress(RuntimeFunction** f, std::size_t* pc, std::size_t* stack)
	{
		*f = _stack.back().Function;
		*pc = _stack.back().PC;
		*stack = _stack.back().StackBase;
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
