#pragma once

class RuntimeLoader;
struct RuntimeType;
struct RuntimeFunction;

struct AssemblyList
{
	std::vector<Assembly> Assemblies;
};

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
};

struct RuntimeType
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

	RuntimeFunction* GCFinalizer;
	void* StaticPointer;

	//For runtime system to quickly find pointer type. Not used by Loader.
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

struct RuntimeFunctionCode
{
	std::string AssemblyName;
	std::size_t Id;
	std::vector<unsigned char> Instruction;
	std::vector<unsigned char> ConstantData;
	std::vector<FunctionConst> ConstantTable;
	std::vector<FunctionLocal> LocalVariables;
};

struct RuntimeFunction
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
