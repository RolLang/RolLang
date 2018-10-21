#pragma once

enum ErrorClass : unsigned char
{
	ERR_UNSPECIFIED,
	ERR_API, //Native API argument check.
	ERR_USER, //User defined exception.

	//Following err classes should be mapped to managed system-defined exception types.
	ERR_PROGRAM, //Related with type/function loading. Not recoverable.
	ERR_UNSUPPORT, //Using a feature not supported by the environment.
	ERR_STACKOVERFLOW,
};

class Interpreter;
typedef bool(*NativeFunction)(Interpreter* interpreter, void* userData);

struct RuntimeFunction;
struct StackFrameInfo
{
	//TODO should contain string info
	RuntimeFunction* Function;
	std::size_t Position;
};
using StacktraceInfo = std::vector<StackFrameInfo>;

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

//Thrown by interpreter. Processed by internal error handler.
//Different from ExternalException because it's used in managed
//part instead of native part of a calling hierarchy.
class InterpreterException : public std::runtime_error
{
public:
	InterpreterException(StacktraceInfo st, unsigned char e, const std::string& msg)
		: std::runtime_error(msg), Stacktrace(std::move(st)), ErrorCode(e)
	{
	}

	//TODO delete this ctor. It does not allow us to obtain stacktrace info.
	InterpreterException(unsigned char e, const std::string& msg)
		: std::runtime_error(msg), ErrorCode(e)
	{
	}

public:
	StacktraceInfo Stacktrace;
	const unsigned char ErrorCode;
};
