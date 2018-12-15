#pragma once
#include <vector>
#include "Assembly.h"
#include "Spinlock.h"
#include "LoaderMultiList.h"

namespace RolLang {

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

struct RuntimeObjectSymbol
{
	RuntimeLoader* Loader;
	std::vector<LoadingArgumentElement> Hierarchy;
};

struct LoadingArguments
{
	std::string Assembly;
	std::size_t Id;
	//std::vector<RuntimeType*> Arguments;
	MultiList<RuntimeType*> Arguments;

	bool operator== (const LoadingArguments &b) const
	{
		return Assembly == b.Assembly && Id == b.Id && Arguments == b.Arguments;
	}

	bool operator!= (const LoadingArguments &b) const
	{
		return !(*this == b);
	}

	RuntimeObjectSymbol ConvertToSymbol(RuntimeLoader* loader)
	{
		RuntimeObjectSymbol ret;
		ret.Loader = loader;
		AppendToSymbol(ret);
		return ret;
	}

private:
	inline void AppendToSymbol(RuntimeObjectSymbol& s);
};

enum ConstraintExportListEntryType
{
	CONSTRAINT_EXPORT_TYPE = 1,
	CONSTRAINT_EXPORT_FUNCTION = 2,
	CONSTRAINT_EXPORT_FIELD = 3,
};

struct ConstraintExportListEntry
{
	ConstraintExportListEntryType EntryType;
	std::size_t Index;
	union
	{
		RuntimeFunction* Function;
		RuntimeType* Type;
		std::size_t Field;
	};
};

typedef std::vector<ConstraintExportListEntry> ConstraintExportList;

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
	std::vector<std::size_t> Fields;
};

struct RuntimeType : Initializable
{
	struct RuntimeFieldInfo
	{
		RuntimeType* Type;
		std::size_t Offset;
		std::size_t Length;
	};

	struct VirtualFunctionInfo
	{
		std::string Name;
		RuntimeFunction* V;
		RuntimeFunction* I;
	};

	struct InheritanceInfo
	{
		RuntimeType* Type;
		std::vector<VirtualFunctionInfo> VirtualFunctions;
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
	RuntimeType* BoxType;
	RuntimeType* ReferenceType;
	RuntimeType* EmbedType;

	InheritanceInfo BaseType;
	std::vector<InheritanceInfo> Interfaces;

	//TODO move to internal structure
	ConstraintExportList ConstraintExportList;

#if _DEBUG
	std::string Fullname;
#endif

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
		} while ((rt = rt->BaseType.Type));
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

	inline std::string GetFullname();
};

//TODO move RuntimeObjectSymbol to after LoadingArguments
inline void LoadingArguments::AppendToSymbol(RuntimeObjectSymbol& s)
{
	auto size = Arguments.GetSizeList();
	s.Hierarchy.push_back({ Assembly, Id, Arguments.GetTotalSize() + size.size() });
	for (std::size_t i = 0; i < size.size(); ++i)
	{
		s.Hierarchy.push_back({ "", SIZE_MAX, 0 });
		for (std::size_t j = 0; j < size[i]; ++j)
		{
			Arguments.Get(i, j)->Args.AppendToSymbol(s);
		}
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

struct RuntimeVirtualFunction
{
	RuntimeType* Type;
	std::size_t Slot;
};

struct RuntimeFunction : Initializable
{
	RuntimeLoader* Parent;
	LoadingArguments Args;
	std::size_t FunctionId;

	std::shared_ptr<RuntimeFunctionCode> Code;
	std::unique_ptr<RuntimeVirtualFunction> Virtual;

	RuntimeReferences References;

	RuntimeType* ReturnValue;
	std::vector<RuntimeType*> Parameters;

	//TODO move to internal structure
	ConstraintExportList ConstraintExportList;

#if _DEBUG
	std::string Fullname;
#endif

	inline std::string GetFullname();
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
struct SubMemberLoadingArguments
{
	RuntimeType* Parent;
	std::string Name;
	MultiList<RuntimeType*> Arguments;

	bool operator == (const SubMemberLoadingArguments &b) const
	{
		return Parent == b.Parent && Name == b.Name && Arguments == b.Arguments;
	}

	bool operator != (const SubMemberLoadingArguments &b) const
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
	RuntimeFunction* SelfFunction;
	const MultiList<RuntimeType*>* AdditionalArguments;

	LoadingRefArguments(RuntimeType* type, const GenericDeclaration& g)
		: Declaration(g), Arguments(type->Args), SelfType(type),
			SelfFunction(nullptr), AdditionalArguments(nullptr)
	{
	}

	LoadingRefArguments(RuntimeFunction* func, const GenericDeclaration& g)
		: Declaration(g), Arguments(func->Args), SelfFunction(func),
			SelfType(nullptr), AdditionalArguments(nullptr)
	{
	}

	LoadingRefArguments(RuntimeType* type, const GenericDeclaration& g, const MultiList<RuntimeType*>& aa)
		: Declaration(g), Arguments(type->Args), SelfType(type), 
			SelfFunction(nullptr), AdditionalArguments(&aa)
	{
	}
};

}