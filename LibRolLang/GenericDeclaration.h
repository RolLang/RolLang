#pragma once
#include "Serialization.h"

enum ReferenceType : unsigned char
{
	//For REF_Assembly and REF_Import, the items after this item is the generic arguments

	REF_EMPTY,
	REF_CLONE, //refer to another entry in the list. index = index in the same list
	REF_ASSEMBLY, //index = assembly type/function array index
	REF_IMPORT, //index = import #
	REF_ARGUMENT, //index = generic parameter list index
	REF_CLONETYPE, //for function generic arguments, clone from the type list
	REF_SELF, //for type/traits, the type itself
};
//Note that for generic function, the generic arguments should use REF_CloneType

struct GenericParameter
{
};
FIELD_SERIALIZER_BEGIN(GenericParameter)
FIELD_SERIALIZER_END()

struct DeclarationReference
{
	ReferenceType Type;
	std::size_t Index;
};
FIELD_SERIALIZER_BEGIN(DeclarationReference)
	SERIALIZE_FIELD(Type)
	SERIALIZE_FIELD(Index)
FIELD_SERIALIZER_END()

struct GenericDeclaration
{
	std::vector<GenericParameter> Parameters;
	std::vector<DeclarationReference> Types;
	std::vector<DeclarationReference> Functions;
	//Contains index in import constant table. Exported values are field index, not offset.
	std::vector<std::size_t> Fields;
};
FIELD_SERIALIZER_BEGIN(GenericDeclaration)
	SERIALIZE_FIELD(Parameters)
	SERIALIZE_FIELD(Types)
	SERIALIZE_FIELD(Functions)
	SERIALIZE_FIELD(Fields)
FIELD_SERIALIZER_END()
