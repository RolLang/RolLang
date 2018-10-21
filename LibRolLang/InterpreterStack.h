#pragma once
#include "RuntimeLoader.h"

//TODO InterpreterException should be changed to include stacktrace info.
class InterpreterStack
{
public:
	InterpreterStack(std::size_t size = 1024 * 4 * 4)
	{
		_data = std::make_unique<char[]>(size);
		_top = (std::uintptr_t)_data.get();
		_end = _top + size;
	}

	char* Push(RuntimeType* type)
	{
		auto alignment = type->GetStorageAlignment();
		auto size = type->GetStorageSize();
		std::uintptr_t ret = (_top + alignment - 1) / alignment * alignment;
		if (ret + size > _end)
		{
			throw InterpreterException(ERR_RUNTIME, "Stack overflow");
		}
		_objects.push_back(_top);
		_typeInfo.push_back(type);
		_top = ret + size;
		return (char*)ret;
	}

	void Pop()
	{
		if (_objects.size() == 0)
		{
			throw InterpreterException(ERR_PROGRAM, "Stack empty");
		}
		assert(_top - _objects.back() >= _typeInfo.back()->GetStorageSize());
		
		_top = _objects.back();

		_objects.pop_back();
		_typeInfo.pop_back();
	}

	std::size_t GetSize()
	{
		return _objects.size();
	}

	void PopToSize(std::size_t size)
	{
		if (size > _objects.size())
		{
			throw InterpreterException(ERR_PROGRAM, "Invalid stack size");
		}
		auto n = _objects.size() - size;
		auto t = _top;
		for (std::size_t i = 0; i < n; ++i)
		{
			_top = _objects.back();
			_objects.pop_back();
			_typeInfo.pop_back();
		}
		_top = t;
	}

	void PopToSizeReturn(std::size_t size)
	{
		auto ret = GetPointer(0);
		auto retType = GetType(0);
		Pop();
		PopToSize(size);
		auto retPos = Push(retType);
		std::memmove(retPos, ret, retType->GetStorageSize());
	}

	//Pos 0 is the last. pos >= 0 and pos < size
	RuntimeType* GetType(std::size_t pos)
	{
		if (pos >= _typeInfo.size())
		{
			throw InterpreterException(ERR_PROGRAM, "Invalid stack index");
		}
		return _typeInfo[_typeInfo.size() - 1 - pos];
	}

	char* GetPointer(std::size_t pos)
	{
		if (pos >= _typeInfo.size())
		{
			throw InterpreterException(ERR_PROGRAM, "Invalid stack index");
		}
		auto index = _objects.size() - 1 - pos;
		auto alignment = _typeInfo[index]->GetStorageAlignment();
		auto top = _objects[index];

		return (char*)((top + alignment - 1) / alignment * alignment);
	}

	RuntimeType* GetTypeAbs(std::size_t pos)
	{
		if (pos >= _typeInfo.size())
		{
			throw InterpreterException(ERR_PROGRAM, "Invalid stack index");
		}
		return _typeInfo[pos];
	}

	char* GetPointerAbs(std::size_t pos)
	{
		if (pos >= _typeInfo.size())
		{
			throw InterpreterException(ERR_PROGRAM, "Invalid stack index");
		}
		auto alignment = _typeInfo[pos]->GetStorageAlignment();
		auto top = _objects[pos];

		return (char*)((top + alignment - 1) / alignment * alignment);
	}

private:
	std::unique_ptr<char[]> _data;
	std::uintptr_t _top;
	std::uintptr_t _end;
	std::vector<uintptr_t> _objects;
	std::vector<RuntimeType*> _typeInfo;
};
