#pragma once
#include "LoaderObjects.h"

enum ErrorClass : unsigned char
{
	ERR_UNSPECIFIED,
	ERR_API, //Native API argument check.
	ERR_USER, //User defined exception.

	//Following err classes should be mapped to managed system-defined exception types.
	ERR_PROGRAM, //Related with type/function loading. Not recoverable.
	ERR_INST, //Caused by an ERR instruction.
	ERR_UNSUPPORT, //Using a feature not supported by the environment.
	ERR_STACKOVERFLOW, //Thrown by InterpreterStack when managed data stack is full.
};

class Interpreter;
typedef bool(*NativeFunction)(Interpreter* interpreter, void* userData);

struct StackFrameInfo
{
	RuntimeObjectSymbol Function;
	std::size_t Position;
};
using StacktraceInfo = std::vector<StackFrameInfo>;
inline StacktraceInfo GetStacktraceInfo(Interpreter* i);

//Thrown by wrapped user-defined native functions in interpreter.
//Caught by native wrapper.
//Note that normal NativeFunction is not allowed to throw. It
//is only allowed to return false to indicate error.
class ExternalException : public std::runtime_error
{
public:
	ExternalException(const std::string& msg)
		: std::runtime_error(msg)
	{
	}
};

struct InterpreterExceptionData
{
	ErrorClass Error;
	std::string Message;
	StacktraceInfo Stacktrace;
};

//Thrown by interpreter. Processed by internal error handler.
//Different from ExternalException because it's used in managed
//part instead of native part of a calling hierarchy.
class InterpreterException : public std::runtime_error
{
public:
	InterpreterException(StacktraceInfo st, ErrorClass e, const std::string& msg)
		: std::runtime_error(msg), Stacktrace(std::move(st)), ErrorCode(e)
	{
	}

	InterpreterException(Interpreter* i, ErrorClass e, const std::string& msg)
		: std::runtime_error(msg), Stacktrace(GetStacktraceInfo(i)), ErrorCode(e)
	{
	}

public:
	InterpreterExceptionData CopyData()
	{
		return { ErrorCode, what(), Stacktrace };
	}

public:
	StacktraceInfo Stacktrace;
	ErrorClass ErrorCode;
};
