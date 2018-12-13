#pragma once
#include "Type.h"
#include "Function.h"
#include "Trait.h"

namespace RolLang {

struct AssemblyImport
{
	std::string AssemblyName;
	std::string ImportName;
	GenericDefArgumentListSize GenericParameters;
};
FIELD_SERIALIZER_BEGIN(AssemblyImport)
	SERIALIZE_FIELD(AssemblyName)
	SERIALIZE_FIELD(ImportName)
	SERIALIZE_FIELD(GenericParameters)
FIELD_SERIALIZER_END()

struct AssemblyExport
{
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
	std::vector<AssemblyExport> ExportFunctions, ExportTypes, ExportConstants, ExportTraits;
	std::vector<AssemblyImport> ImportFunctions, ImportTypes, ImportConstants, ImportTraits;
	std::vector<AssemblyExport> NativeFunctions, NativeTypes;
	std::vector<Function> Functions;
	std::vector<Type> Types;
	std::vector<std::uint32_t> Constants;
	std::vector<Trait> Traits;
};
FIELD_SERIALIZER_BEGIN(Assembly)
	SERIALIZE_FIELD(AssemblyName)
	SERIALIZE_FIELD(ExportFunctions)
	SERIALIZE_FIELD(ExportTypes)
	SERIALIZE_FIELD(ExportConstants)
	SERIALIZE_FIELD(ExportTraits)
	SERIALIZE_FIELD(ImportFunctions)
	SERIALIZE_FIELD(ImportTypes)
	SERIALIZE_FIELD(ImportConstants)
	SERIALIZE_FIELD(ImportTraits)
	SERIALIZE_FIELD(NativeFunctions)
	SERIALIZE_FIELD(NativeTypes)
	SERIALIZE_FIELD(Functions)
	SERIALIZE_FIELD(Types)
	SERIALIZE_FIELD(Constants)
	SERIALIZE_FIELD(Traits)
FIELD_SERIALIZER_END()

}
