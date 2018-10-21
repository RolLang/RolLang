#pragma once
#include <exception>
#include <string>

class ExceptionBase : public std::exception
{
public:
	ExceptionBase(const std::string& msg) : _msg(msg)
	{
	}

	virtual char const* what() const override
	{
		return _msg.c_str();
	}

private:
	std::string _msg;
};

//Exception thrown and caught internally by RuntimeLoader
class RuntimeLoaderException : public ExceptionBase
{
public:
	RuntimeLoaderException(const std::string& msg)
		: ExceptionBase(msg)
	{
	}
};

//Thrown by wrapped user-defined native functions in interpreter.
//Caught by native wrapper.
//Note that normal NativeFunction is not allowed to throw. It
//is only allowed to return false to indicate error.
class ExternalException : public ExceptionBase
{
public:
	ExternalException(const std::string& msg) : ExceptionBase(msg)
	{
	}
};

struct RuntimeFunction;
struct StackFrameInfo
{
	//TODO should contain string info
	RuntimeFunction* Function;
	std::size_t Position;
};
using StacktraceInfo = std::vector<StackFrameInfo>;

//Thrown by interpreter. Processed by internal error handler.
//Different from ExternalException because it's used in managed
//part instead of native part of a calling hierarchy.
class InterpreterException : public ExceptionBase
{
public:
	InterpreterException(StacktraceInfo st, unsigned char e, const std::string& msg)
		: ExceptionBase(msg), Stacktrace(std::move(st)), ErrorCode(e)
	{
	}

	//TODO delete this ctor. It does not allow us to obtain stacktrace info.
	InterpreterException(unsigned char e, const std::string& msg)
		: ExceptionBase(msg), ErrorCode(e)
	{
	}

public:
	StacktraceInfo Stacktrace;
	const unsigned char ErrorCode;
};
