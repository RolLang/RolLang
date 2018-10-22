#pragma once
#include "InterpreterBase.h"

class Interpreter : public InterpreterBase
{
public:
	Interpreter(AssemblyList assemblies) : InterpreterBase(assemblies, this, InterpreterEntry)
	{
	}

private:
	static bool InterpreterEntry(Interpreter* i, void* data)
	{
		RuntimeFunction* f = (RuntimeFunction*)data;
		try
		{
			auto nextPos = i->ExecuteNormal(f, 0, f->Code->Instruction.size());
			//The execution must end at a RET instruction.
			assert(nextPos == SIZE_MAX);
		}
		catch (InterpreterException& ex)
		{
			//Uncaught exception return to API.
			i->ReturnWithException(ex);
			return false;
		}
		return true;
	}

private:
	std::size_t ExecuteTry(RuntimeFunction* f, std::size_t startPos, std::size_t catchPos,
		std::size_t finPos, std::size_t stopPos)
	{
		std::size_t nextPos;
		try
		{
			nextPos = ExecuteNormal(f, startPos, catchPos);
		}
		catch (InterpreterException& ex)
		{
			//TODO setup data to let other functions know there is one exception processing
			try
			{
				auto nextPosCatch = ExecuteNormal(f, catchPos, finPos);
				//Exception successfully processed. We make nextPos = catchPos to continue.
				nextPos = catchPos;
			}
			catch (InterpreterException& ex2)
			{
				//New or old exception from catch block. Still try to execute finally.
				//We don't need try here because only a higher level catch can handle finally exception.
				auto nextPosFin = ExecuteNormal(f, finPos, stopPos);
				//No jmp allowed within finally block
				assert(nextPosFin == stopPos);
				//Now finally block has been executed, continue with ex2.
				throw ex2;
			}
		}

		//try block cannot terminate inside catch/finally block
		assert(nextPos <= catchPos || nextPos >= stopPos);

		auto nextPosFin = ExecuteNormal(f, finPos, stopPos);
		//No jmp allowed within finally block
		assert(nextPosFin == stopPos);

		if (nextPos == catchPos)
		{
			nextPos = stopPos;
		}
		return nextPos;
	}

	std::size_t ExecuteNormal(RuntimeFunction* f, std::size_t startPos, std::size_t stopPos)
	{
		//TODO
		return SIZE_MAX;
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
			return APICall(info);
		}
		return APICall(i->second);
	}

private:
	//Used by Call API
	bool APICall(const FuncInfo& i)
	{
		//TODO assert not in managed code
		if (i.FunctionData == InterpreterEntry)
		{
			CallingFrameNM frame(this, i.STInfo);
			return i.FunctionPtr(this, i.FunctionData);
		}
		else
		{
			CallingFrameNN frame(this, i);
			return DoCallNative(i);
		}
	}

	//Used by managed code to call native only
	bool ManagedCall(const FuncInfo& i)
	{
		//TODO assert in managed code
		assert(i.FunctionData != InterpreterEntry);
		CallingFrameMN frame(this, i);
		return DoCallNative(i);
	}

	bool DoCallNative(const FuncInfo& i)
	{
		//TODO still need to ensure no exception thrown
		return i.FunctionPtr(this, i.FunctionData);
	}

private:
	std::unordered_map<std::size_t, FuncInfo> _functionCache;

private:
	friend inline StacktraceInfo GetStacktraceInfo(Interpreter* i)
	{
		return i->_stacktracer.GetStacktrace();
	}
};
