#pragma once
#include "Type.h"
#include "Function.h"

struct AssemblyImport
{
	std::string AssemblyName;
	std::string ImportName;
	std::size_t GenericParameters;
};
FIELD_SERIALIZER_BEGIN(AssemblyImport)
	SERIALIZE_FIELD(AssemblyName)
	SERIALIZE_FIELD(ImportName)
	SERIALIZE_FIELD(GenericParameters)
FIELD_SERIALIZER_END()

struct AssemblyExport
{
	//TODO only for export, when id > size, it points to the import table to allow redirecting
	std::size_t InternalId;
	std::string ExportName;
};
FIELD_SERIALIZER_BEGIN(AssemblyExport)
	SERIALIZE_FIELD(InternalId)
	SERIALIZE_FIELD(ExportName)
FIELD_SERIALIZER_END()

struct Assembly
{
	std::string AssemblyName;
	std::vector<AssemblyExport> ExportFunctions, ExportTypes;
	std::vector<AssemblyImport> ImportFunctions, ImportTypes;
	std::vector<AssemblyExport> NativeFunctions, NativeTypes;
	std::vector<AssemblyExport> ExportConstants;
	std::vector<AssemblyImport> ImportConstants;
	std::vector<Function> Functions;
	std::vector<Type> Types;
};
FIELD_SERIALIZER_BEGIN(Assembly)
	SERIALIZE_FIELD(AssemblyName)
	SERIALIZE_FIELD(ExportFunctions)
	SERIALIZE_FIELD(ExportTypes)
	SERIALIZE_FIELD(ImportFunctions)
	SERIALIZE_FIELD(ImportTypes)
	SERIALIZE_FIELD(NativeFunctions)
	SERIALIZE_FIELD(NativeTypes)
	SERIALIZE_FIELD(ExportConstants)
	SERIALIZE_FIELD(ImportConstants)
	SERIALIZE_FIELD(Functions)
	SERIALIZE_FIELD(Types)
FIELD_SERIALIZER_END()
