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
		//TODO check argument number and type
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
		assert(startPos < catchPos);
		assert(catchPos <= finPos);
		assert(finPos <= stopPos);
		std::size_t nextPos;
		std::size_t stackLimit = _stack.GetLimitSize();
		try
		{
			nextPos = ExecuteNormal(f, startPos, catchPos);
			assert(_stack.GetLimitSize() == stackLimit);
		}
		catch (InterpreterException& ex)
		{
			assert(_stack.GetLimitSize() >= stackLimit);
			_stack.SetLimitSize(stackLimit);

			if (finPos == catchPos)
			{
				//Empty catch block.
				auto nextPosFin = ExecuteNormal(f, finPos, stopPos);
				assert(_stack.GetLimitSize() == stackLimit);
				//No jmp allowed within finally block
				assert(nextPosFin == stopPos);
				throw ex;
			}
			try
			{
				auto nextPosCatch = ExecuteNormal(f, catchPos, finPos);
				assert(_stack.GetLimitSize() == stackLimit);
				//Exception successfully processed. We make nextPos = catchPos to continue.
				nextPos = nextPosCatch;
			}
			catch (InterpreterException& ex2)
			{
				assert(_stack.GetLimitSize() >= stackLimit);
				_stack.SetLimitSize(stackLimit);

				//New or old exception from catch block. Still try to execute finally.
				//We don't need try here because only a higher level catch can handle finally exception.
				auto nextPosFin = ExecuteNormal(f, finPos, stopPos);
				assert(_stack.GetLimitSize() == stackLimit);
				//No jmp allowed within finally block
				assert(nextPosFin == stopPos);
				//Now finally block has been executed, continue with ex2.
				throw ex2;
			}
		}

		//try block cannot terminate inside catch/finally block
		assert(nextPos <= catchPos || nextPos >= stopPos);

		auto nextPosFin = ExecuteNormal(f, finPos, stopPos);
		assert(_stack.GetLimitSize() == stackLimit);
		//No jmp allowed within finally block
		assert(nextPosFin == stopPos);

		if (nextPos == catchPos)
		{
			nextPos = stopPos;
		}
		return nextPos;
	}

	void SetupLocalStack(RuntimeFunction* f)
	{
		assert(_stack.GetSize() >= _stack.GetLimitSize() + f->Parameters.size());
		_stack.SetLimitSize(_stack.GetSize() - f->Parameters.size());
	}

	std::size_t ExecuteNormal(RuntimeFunction* f, std::size_t startPos, std::size_t stopPos)
	{
		assert(startPos <= stopPos);
		if (startPos == stopPos)
		{
			//An empty segment of code (e.g. empty finally block)
			return stopPos;
		}

		//Additional frame data
		int callingLevel = 0;
		std::size_t pc = startPos;
		auto code = f->Code.get();
		unsigned char* prog = code->Instruction.data();
		std::size_t argStart = _stack.GetLimitSize();
		std::size_t localStart = argStart + f->Parameters.size();

		while (true)
		{
			unsigned char opcode = prog[pc++];
			unsigned char opr3 = opcode & 7u;
			std::uint32_t opr = opr3;
			if (opr3 == 7)
			{
				opr = *(int*)prog;
				pc += 4;
			}
			else if (opr3 == 6)
			{
				opr = prog[pc++];
			}
			switch (opcode >> 3)
			{
			case OP_NOP:
				break;
			case OP_ERR:
				assert(opr3 == 0);
				throw InterpreterException(this, ERR_INST, "OP_ERR instruction");
			case OP_ARG:
			{
				assert(opr < f->Parameters.size());
				auto type = _stack.GetTypeAbs(argStart + opr);
				auto ptr = _stack.GetPointerAbs(argStart + opr);
				auto dest = _stack.Push(GetPointerType(type));
				*(void**)dest = ptr;
				break;
			}
			case OP_CONST:
			{
				assert(opr < code->ConstantTable.size());
				auto& k = code->ConstantTable[opr];
				assert(k.Offset < code->ConstantData.size());
				assert(k.Offset + k.Length <= code->ConstantData.size());
				assert(k.TypeId < f->ReferencedType.size());
				assert(k.Length == f->ReferencedType[k.TypeId]->GetStorageSize());

				auto type = f->ReferencedType[k.TypeId];
				auto ptr = &code->ConstantData[k.Offset];
				auto dest = _stack.Push(GetPointerType(type));
				*(void**)dest = ptr;
				break;
			}
			case OP_LOAD:
			{
				auto type = _stack.GetType(0);
				assert(_loader->IsPointerType(type));
				auto eleType = type->Args.Arguments[0];
				auto ptr = *(void**)_stack.GetPointer(0);
				_stack.Pop();
				auto dest = _stack.Push(eleType);
				std::memcpy(dest, ptr, eleType->GetStorageSize());
				break;
			}
			case OP_DUP:
			{
				assert(opr < _stack.GetSize() - argStart);
				auto type = _stack.GetType(opr);
				auto ptr = _stack.GetPointer(opr);
				auto dest = _stack.Push(type);
				std::memcpy(dest, ptr, type->GetStorageSize());
				break;
			}
			case OP_POP:
			{
				assert(opr < _stack.GetSize() - argStart);
				for (std::size_t i = 0; i < opr; ++i)
				{
					_stack.Pop();
				}
				break;
			}
			case OP_CALL:
			{
				assert(opr < f->ReferencedFunction.size());
				auto callee = f->ReferencedFunction[opr];
				assert(callee);
				FuncInfo calleeInfo;
				auto suc = _loader->TryFindFunctionInfoById(callee->FunctionId, &calleeInfo);
				assert(suc);
				assert(argStart + callee->Parameters.size() <= _stack.GetSize());
				if (calleeInfo.FunctionPtr == InterpreterEntry)
				{
					//enter new frame
					_stacktracer.SetReturnAddress(pc, argStart);
					_stacktracer.BeginFrameInterpreted(callee);

					f = callee;
					callingLevel++;
					pc = 0;
					code = f->Code.get();
					prog = code->Instruction.data();
					localStart = _stack.GetSize();
					argStart = localStart - f->Parameters.size();
					_stack.SetLimitSize(argStart);
					break;
				}
				else
				{
					ManagedCall(calleeInfo);
				}
				break;
			}
			case OP_RET:
			{
				_stack.PopToSizeReturn(argStart);

				if (callingLevel == 0)
				{
					PauseCheckInterpreter();
					return SIZE_MAX;
				}

				callingLevel--;
				_stacktracer.EndFrameInterpreted();
				_stacktracer.GetReturnAddress(&f, &pc, &argStart);
				code = f->Code.get();
				prog = code->Instruction.data();

				localStart = argStart + f->Parameters.size();
				_stack.SetLimitSize(argStart);
				break;
			}
			}

			PauseCheckInterpreter();
		}
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
		if (i.FunctionPtr == InterpreterEntry)
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
