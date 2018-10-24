#pragma once
#include <vector>

class RuntimeLoader;
struct RuntimeType;
struct RuntimeFunction;

struct AssemblyList
{
	std::vector<Assembly> Assemblies;
};

struct LoadingArgumentElement
{
	std::string Assembly;
	std::size_t Id;
	std::size_t ArgumentCount;
};
typedef std::vector<LoadingArgumentElement> RuntimeObjectSymbol;

struct LoadingArguments
{
	std::string Assembly;
	std::size_t Id;
	std::vector<RuntimeType*> Arguments;

	bool operator == (const LoadingArguments &b) const
	{
		return Assembly == b.Assembly && Id == b.Id && Arguments == b.Arguments;
	}

	bool operator != (const LoadingArguments &b) const
	{
		return !(*this == b);
	}

	RuntimeObjectSymbol ConvertToSymbol()
	{
		RuntimeObjectSymbol ret;
		AppendToSymbol(ret);
		return ret;
	}

private:
	inline void AppendToSymbol(RuntimeObjectSymbol s);
};

struct Initializable
{
	std::unique_lock<Spinlock> BeginInit()
	{
		std::unique_lock<Spinlock> l(Lock);
		if (InitFlag)
		{
			l.unlock();
		}
		return std::move(l);
	}

private:
	Spinlock Lock;
	bool InitFlag;
};

struct RuntimeType : Initializable
{
	struct RuntimeFieldInfo
	{
		RuntimeType* Type;
		std::size_t Offset;
		std::size_t Length;
	};

	RuntimeLoader* Parent;
	LoadingArguments Args;
	std::size_t TypeId;

	TypeStorageMode Storage;
	std::vector<RuntimeFieldInfo> Fields;
	std::size_t Alignment;
	std::size_t Size;

	RuntimeFunction* Initializer;
	RuntimeFunction* Finalizer;
	void* StaticPointer;
	RuntimeType* PointerType;

	std::size_t GetStorageSize()
	{
		return Storage == TypeStorageMode::TSM_REF ? sizeof(void*) : Size;
	}

	std::size_t GetStorageAlignment()
	{
		return Storage == TypeStorageMode::TSM_REF ? sizeof(void*) : Alignment;
	}
};

//TODO move RuntimeObjectSymbol to after LoadingArguments
inline void LoadingArguments::AppendToSymbol(RuntimeObjectSymbol s)
{
	s.push_back({ Assembly, Id, Arguments.size() });
	for (std::size_t i = 0; i < Arguments.size(); ++i)
	{
		Arguments[i]->Args.AppendToSymbol(s);
	}
}

struct RuntimeFunctionCode
{
	std::string AssemblyName;
	std::size_t Id;
	std::vector<unsigned char> Instruction;
	std::vector<unsigned char> ConstantData;
	std::vector<FunctionConst> ConstantTable;
	std::vector<FunctionLocal> LocalVariables;
};

struct RuntimeFunction : Initializable
{
	RuntimeLoader* Parent;
	LoadingArguments Args;
	std::size_t FunctionId;

	std::shared_ptr<RuntimeFunctionCode> Code;

	std::vector<RuntimeType*> ReferencedType;
	std::vector<RuntimeFunction*> ReferencedFunction;

	RuntimeType* ReturnValue;
	std::vector<RuntimeType*> Parameters;
};
