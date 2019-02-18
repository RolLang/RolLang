#pragma once
#include <unordered_map>
#include "InterpreterRuntimeLoader.h"
#include "InterpreterStacktracer.h"
#include "InterpreterStack.h"

namespace RolLang {

class InterpreterBase
{
protected:
	using FuncInfo = InterpreterRuntimeLoader::InterpreterRuntimeFunctionInfo;
	using TypeInfo = InterpreterRuntimeLoader::InterpreterRuntimeTypeInfo;

public:
	InterpreterBase(AssemblyList assemblies, Interpreter* i, NativeFunction interpreterEntry)
		: _loader(std::make_shared<InterpreterRuntimeLoader>(interpreterEntry, std::move(assemblies))),
			_stacktracer(_loader.get()),
			_stack(i)
	{
		RegisterNativeTypes();
	}

	virtual ~InterpreterBase() {}

protected: //Exception handling
	//Convert the internal error info to some exception (no need for additional info)
	void ReturnWithException(StacktraceInfo st, ErrorClass code, std::string err)
	{
		_lastError = { code, err, st };
	}

	void ReturnWithException(InterpreterException& ex)
	{
		_lastError = ex.CopyData();
	}

	InterpreterException RethrowLastException()
	{
		//TODO handle user exception object?
		return InterpreterException(_lastError.Stacktrace, _lastError.Error, _lastError.Message);
	}

	void PushLastException()
	{
		//TODO
	}

public:
	InterpreterExceptionData GetLastError()
	{
		return _lastError;
	}

public: //Loader API
	std::size_t GetType(const std::string& assemblyName, const std::string& exportName,
		const MultiList<std::size_t>& genericArgs, const GenericDefArgumentListSize& genericSize = {})
	{
		LoadingArguments args;
		AssemblyImport importInfo = { assemblyName, exportName, genericSize };
		if (!_loader->FindExportType(importInfo, args))
		{
			ReturnWithException({}, ERR_PROGRAM, "Export type not found");
			return SIZE_MAX;
		}
		auto&& argsSize = genericArgs.GetSizeList();
		for (std::size_t i = 0; i < argsSize.size(); ++i)
		{
			args.Arguments.NewList();
			for (std::size_t j = 0; j < argsSize[i]; ++j)
			{
				args.Arguments.AppendLast(_loader->GetTypeById(genericArgs.Get(i, j)));
			}
		}
		LoaderErrorInformation err;
		auto ret = _loader->GetType(args, err);
		if (!ret)
		{
			ReturnWithException({}, ERR_PROGRAM, err.Message);
			return SIZE_MAX;
		}
		return ret->TypeId;
	}

	std::size_t GetFunction(const std::string& assemblyName, const std::string& exportName,
		const MultiList<std::size_t>& genericArgs, const GenericDefArgumentListSize& genericSize = {})
	{
		LoadingArguments args;
		AssemblyImport importInfo = { assemblyName, exportName, genericSize };
		if (!_loader->FindExportFunction(importInfo, args))
		{
			ReturnWithException({}, ERR_PROGRAM, "Export function not found");
			return SIZE_MAX;
		}
		auto&& argsSize = genericArgs.GetSizeList();
		for (std::size_t i = 0; i < argsSize.size(); ++i)
		{
			args.Arguments.NewList();
			for (std::size_t j = 0; j < argsSize[i]; ++j)
			{
				args.Arguments.AppendLast(_loader->GetTypeById(genericArgs.Get(i, j)));
			}
		}
		LoaderErrorInformation err;
		auto ret = _loader->GetFunction(args, err);
		if (!ret)
		{
			ReturnWithException({}, ERR_PROGRAM, err.Message);
			return SIZE_MAX;
		}
		return ret->FunctionId;
	}

	bool RegisterNativeType(const std::string& assemblyName, const std::string& name,
		std::size_t size, std::size_t alignment)
	{
		return RegisterNativeTypeInternal(assemblyName, name, size, alignment);
	}

	bool RegisterNativeFunction(const std::string& assemblyName, const std::string& name,
		NativeFunction func, void* data)
	{
		size_t id;
		if (!CheckNativeFunctionBasic(assemblyName, name, &id))
		{
			ReturnWithException({}, ERR_PROGRAM, "Invalid native function");
			return false;
		}
		_loader->AddNativeFunction(assemblyName, id, func, data);
		return true;
	}

	bool RegisterNativeFunction(const std::string& assemblyName, const std::string& name,
		int retCheck, int argsCheck[],
		NativeFunction func, void* data)
	{
		size_t id;
		if (!CheckNativeFunctionTypeCompatibility(assemblyName, name, retCheck, argsCheck, &id))
		{
			ReturnWithException({}, ERR_PROGRAM, "Invalid native function");
			return false;
		}
		_loader->AddNativeFunction(assemblyName, id, func, data);
		return true;
	}

protected:
	RuntimeType* RegisterNativeTypeInternal(const std::string& assemblyName, const std::string& name,
		std::size_t size, std::size_t alignment)
	{
		std::string err;
		auto ret = _loader->AddNativeType(assemblyName, name, size, alignment, err);
		if (ret == nullptr)
		{
			ReturnWithException({}, ERR_PROGRAM, err);
		}
		return ret;
	}

	bool CheckNativeFunctionBasic(const std::string& assemblyName, const std::string& name, size_t* id)
	{
		Function* f;
		if (!_loader->GetNativeFunctionByName(assemblyName, name, &f, id))
		{
			return false;
		}
		return DoCheckNativeFunctionBasic(f);
	}

	bool DoCheckNativeFunctionBasic(Function* f)
	{
		if (f == nullptr) return false;
		if (f->Instruction.size() || f->ConstantTable.size() || f->ConstantData.size()) return false;
		if (!f->Generic.ParameterCount.IsEmpty()) return false;
		//TODO check reflist? (function & field)
		return true;
	}

	bool CheckNativeFunctionTypeCompatibility(const std::string& assemblyName, const std::string& name,
		int ret, int args[], size_t* id)
	{
		Function* f;
		if (!_loader->GetNativeFunctionByName(assemblyName, name, &f, id))
		{
			return false;
		}
		if (!DoCheckNativeFunctionBasic(f)) return false;
		//TODO (args is 0-terminated)
		return true;
	}

	RuntimeType* GetPointerType(RuntimeType* type)
	{
		if (type->PointerType != nullptr)
		{
			return type->PointerType;
		}
		LoaderErrorInformation err;
		auto ret = _loader->LoadPointerType(type, err);
		if (ret == nullptr)
		{
			throw InterpreterException(_stacktracer.GetStacktrace(), ERR_PROGRAM, err.Message);
		}
		return ret;
	}

protected: //Native type registration
	enum NativeTypes : int
	{
		NT_NONE,
		NT_BOOL,
		NT_I32,
		NT_F32,
		NT_RAWPTR,
		NT_REF, //not supported yet

		NT_COUNT,
	};

	void RegisterNativeTypes()
	{
		_nativeTypes[NT_BOOL] = RegisterNativeTypeInternal("Core", "Core.Boolean", 1, 1);
		_nativeTypes[NT_I32] = RegisterNativeTypeInternal("Core", "Core.Int32", 4, 4);
		_nativeTypes[NT_F32] = RegisterNativeTypeInternal("Core", "Core.Float", 4, 4);
		_nativeTypes[NT_RAWPTR] = RegisterNativeTypeInternal("Core", "Core.RawPtr", sizeof(void*), sizeof(void*));
	}

	template <typename T> struct NativeTypeId { static const int Value = NT_NONE; };
	template <> struct NativeTypeId<bool> { static const int Value = NT_BOOL; };
	template <> struct NativeTypeId<std::int32_t> { static const int Value = NT_I32; };
	template <> struct NativeTypeId<float> { static const int Value = NT_F32; };
	template <> struct NativeTypeId<void*> { static const int Value = NT_RAWPTR; };


public:
	//Interpreter API part.
	//Note that these APIs don't throw. Don't use in Interpreter environment
	//where we may need to propagate the error back to a handler.

public: //Interpreter API (push & pop)

	bool Push(bool val)
	{
		return PushInternal(val);
	}

	bool Push(std::int32_t val)
	{
		return PushInternal(val);
	}

	bool Push(float val)
	{
		return PushInternal(val);
	}

	bool Push(void* val)
	{
		return PushInternal(val);
	}

	bool Pop(bool* val)
	{
		return PopInternal(val);
	}

	bool Pop(std::int32_t* val)
	{
		return PopInternal(val);
	}

	bool Pop(float* val)
	{
		return PopInternal(val);
	}

	bool Pop(void** val)
	{
		return PopInternal(val);
	}

protected:
	template <typename T>
	bool PushInternal(T val)
	{
		try
		{
			static_assert(NativeTypeId<T>::Value != NT_NONE, "Internal type not supported");
			auto ptr = (T*)_stack.Push(_nativeTypes[NativeTypeId<T>::Value]);
			*ptr = val;
			return true;
		}
		catch (InterpreterException& ex)
		{
			ReturnWithException(ex);
			return false;
		}
	}

	template <typename T>
	bool PopInternal(T* ret)
	{
		try
		{
			static_assert(NativeTypeId<T>::Value != NT_NONE, "Internal type not supported");
			auto type = _stack.GetType(0);
			if (type != _nativeTypes[NativeTypeId<T>::Value])
			{
				return false;
			}
			auto ptr = (T*)_stack.GetPointer(0);
			*ret = *ptr;
			_stack.Pop();
			return true;
		}
		catch (InterpreterException& ex)
		{
			ReturnWithException(ex);
			return false;
		}
	}

public: //Interpreter API (dup & pop without result)
	bool Dup(std::size_t index)
	{
		try
		{
			auto type = _stack.GetType(index);
			auto ptr = _stack.GetPointer(index);
			auto newPtr = _stack.Push(type);
			std::memcpy(newPtr, ptr, type->GetStorageSize());
			return true;
		}
		catch (InterpreterException& ex)
		{
			ReturnWithException(ex);
			return false;
		}
	}

	bool Pop(std::size_t count)
	{
		try
		{
			for (std::size_t i = 0; i < count; ++i) _stack.Pop();
		}
		catch (InterpreterException& ex)
		{
			ReturnWithException(ex);
			return false;
		}
	}

protected: //native/managed code switching. Related to GC pause.
	//TODO do we need to throw from the destructor?

	struct CallingFrameNN
	{
		CallingFrameNN(InterpreterBase* i, FuncInfo f)
			: _i(i), _f(i->_stack.GetLimitSize()), _ret(f.RuntimePtr->ReturnValue)
		{
			_i->_stacktracer.BeginNativeFrame(f.RuntimePtr, f.EntryPtr);
			_i->_stack.SetLimitSize(_i->_stack.GetSize() - f.RuntimePtr->Parameters.size());
		}
		~CallingFrameNN()
		{
			//TODO check return value
			if (_ret)
			{
				_i->_stack.PopToSizeReturn(_i->_stack.GetLimitSize());
			}
			else
			{
				_i->_stack.PopToSize(_i->_stack.GetLimitSize());
			}
			_i->_stack.SetLimitSize(_f);
			_i->_stacktracer.EndNativeFrame();
		}
	private:
		InterpreterBase* const _i;
		std::size_t const _f;
		bool const _ret;
	};

	struct CallingFrameNM
	{
		CallingFrameNM(InterpreterBase* i, RuntimeFunction* f) : _i(i)
		{
			_i->_stacktracer.BeginFrameInterpreted(f);
		}
		~CallingFrameNM()
		{
			_i->_stacktracer.EndFrameInterpreted();
		}
	private:
		InterpreterBase* const _i;
	};

	struct CallingFrameMN
	{
		CallingFrameMN(InterpreterBase* i, FuncInfo f)
			: _i(i), _f(i->_stack.GetLimitSize()), _ret(f.RuntimePtr->ReturnValue)
		{
			_i->_stacktracer.BeginNativeFrame(f.RuntimePtr, f.EntryPtr);
			_i->_stack.SetLimitSize(_i->_stack.GetSize() - f.RuntimePtr->Parameters.size());
		}
		~CallingFrameMN()
		{
			//TODO check return value
			if (_ret)
			{
				_i->_stack.PopToSizeReturn(_i->_stack.GetLimitSize());
			}
			else
			{
				_i->_stack.PopToSize(_i->_stack.GetLimitSize());
			}
			_i->_stack.SetLimitSize(_f);
			_i->_stacktracer.EndNativeFrame();
		}
	private:
		InterpreterBase* const _i;
		std::size_t const _f;
		bool const _ret;
	};

protected:

	//TODO add to all API that affects GC
	void PauseCheckAPI()
	{

	}

	void PauseCheckInterpreter()
	{

	}

protected:
	std::shared_ptr<InterpreterRuntimeLoader> _loader;
	InterpreterStack _stack;
	InterpreterStacktracer _stacktracer;
	//TODO GC
	InterpreterExceptionData _lastError;
	//TODO custom exception object
	RuntimeType* _nativeTypes[NT_COUNT];
};

}
