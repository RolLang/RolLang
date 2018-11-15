#pragma once
#include <vector>
#include "Assembly.h"
#include "Spinlock.h"

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

//TODO use a separate class, support obtaining component name from loader
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

struct RuntimeReferences
{
	std::vector<RuntimeType*> Types;
	std::vector<RuntimeFunction*> Functions;
	//TODO constrain list
};

struct RuntimeType : Initializable
{
	struct RuntimeFieldInfo
	{
		RuntimeType* Type;
		std::size_t Offset;
		std::size_t Length;
	};

	struct InterfaceInfo
	{
		RuntimeType* Type;
		RuntimeType* VirtualTable;
	};

	RuntimeLoader* Parent;
	LoadingArguments Args;
	std::size_t TypeId;

	RuntimeReferences References;

	TypeStorageMode Storage;
	std::vector<RuntimeFieldInfo> Fields;
	std::size_t Alignment;
	std::size_t Size;

	RuntimeFunction* Initializer;
	RuntimeFunction* Finalizer;
	RuntimeType* PointerType;

	RuntimeType* BaseType;
	RuntimeType* VirtualTableType;
	std::vector<InterfaceInfo> Interfaces;

	inline std::size_t GetStorageSize();
	inline std::size_t GetStorageAlignment();

	bool IsBaseTypeOf(RuntimeType* rt)
	{
		do
		{
			if (this == rt)
			{
				return true;
			}
		} while ((rt = rt->BaseType));
		return false;
	}

	bool IsInterfaceOf(RuntimeType* rt)
	{
		assert(Storage == TSM_INTERFACE);
		for (auto& i : rt->Interfaces)
		{
			if (i.Type == this) return true;
			if (IsInterfaceOf(i.Type)) return true;
		}
		return false;
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

	RuntimeReferences References;
	std::vector<std::size_t> ReferencedFields;

	RuntimeType* ReturnValue;
	std::vector<RuntimeType*> Parameters;
};

struct RuntimeFunctionCodeStorage
{
	std::vector<std::shared_ptr<RuntimeFunctionCode>> Data;
};

//TODO Move to some other place
//Exception thrown internally within RuntimeLoader (and subclasses).
class RuntimeLoaderException : public std::runtime_error
{
public:
	RuntimeLoaderException(const std::string& msg)
		: std::runtime_error(msg)
	{
	}
};

//TODO Move to some other place
struct SubtypeLoadingArguments
{
	RuntimeType* Parent;
	std::string Name;
	std::vector<RuntimeType*> Arguments;

	bool operator == (const SubtypeLoadingArguments &b) const
	{
		return Parent == b.Parent && Name == b.Name && Arguments == b.Arguments;
	}

	bool operator != (const SubtypeLoadingArguments &b) const
	{
		return !(*this == b);
	}
};

//TODO move to some other place
struct LoadingRefArguments
{
	const GenericDeclaration& Declaration;
	const LoadingArguments& Arguments;
	RuntimeType* SelfType;
	const std::vector<RuntimeType*>* AdditionalArguments;

	LoadingRefArguments(RuntimeType* type, const GenericDeclaration& g)
		: Declaration(g), Arguments(type->Args), SelfType(type), AdditionalArguments(nullptr)
	{
	}

	LoadingRefArguments(RuntimeFunction* func, const GenericDeclaration& g)
		: Declaration(g), Arguments(func->Args), SelfType(nullptr), AdditionalArguments(nullptr)
	{
	}

	LoadingRefArguments(RuntimeType* type, const GenericDeclaration& g, const std::vector<RuntimeType*>& aa)
		: Declaration(g), Arguments(type->Args), SelfType(type), AdditionalArguments(&aa)
	{
	}
};