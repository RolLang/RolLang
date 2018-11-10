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
	REF_SELF, //for type, the type itself. for trait, the target type
	REF_SUBTYPE, //sub type of the given type. index = index in name list

	//Following 2 are only for constrain type list only
	REF_TRY, //same as CLONE except for that it allow the refered calculation to fail (not an error)
	REF_ANY, //undetermined generic type in trait constrains (can only be used as argument)

	REF_REFTYPES = 127,
	REF_FORCELOAD = 128,
};
//Note that for generic function, the generic arguments should use REF_CLONETYPE

enum ConstrainType : unsigned char
{
	CONSTRAIN_EXIST, //(T) -> T exists
	CONSTRAIN_SAME, //<T1>(T) -> T1 == T
	CONSTRAIN_BASE, //<T1>(T) -> T1 == T or T1 is in the base type chain from T
	CONSTRAIN_INTERFACE, //<T1>(T) -> T implements T1
	CONSTRAIN_TRAIT_ASSEMBLY, //<...>(T) -> check trait (in the same assembly)
	CONSTRAIN_TRAIT_IMPORT, //import trait
};

struct DeclarationReference
{
	ReferenceType Type;
	std::size_t Index;
};
FIELD_SERIALIZER_BEGIN(DeclarationReference)
	SERIALIZE_FIELD(Type)
	SERIALIZE_FIELD(Index)
FIELD_SERIALIZER_END()

struct GenericConstrain
{
	ConstrainType Type;
	std::size_t Index;

	std::vector<DeclarationReference> TypeReferences;
	std::size_t Target;
	std::vector<std::size_t> Arguments;
};
FIELD_SERIALIZER_BEGIN(GenericConstrain)
	SERIALIZE_FIELD(Type)
	SERIALIZE_FIELD(Index)
	SERIALIZE_FIELD(TypeReferences)
	SERIALIZE_FIELD(Target)
	SERIALIZE_FIELD(Arguments)
FIELD_SERIALIZER_END()

struct GenericDeclaration
{
	std::size_t ParameterCount;
	std::vector<GenericConstrain> Constrains;

	std::vector<DeclarationReference> Types;
	std::vector<DeclarationReference> Functions;
	//Contains index in import constant table. Exported values are field index, not offset.
	std::vector<std::size_t> Fields;
	std::vector<std::string> SubtypeNames;
};
FIELD_SERIALIZER_BEGIN(GenericDeclaration)
	SERIALIZE_FIELD(ParameterCount)
	SERIALIZE_FIELD(Constrains)
	SERIALIZE_FIELD(Types)
	SERIALIZE_FIELD(Functions)
	SERIALIZE_FIELD(Fields)
	SERIALIZE_FIELD(SubtypeNames)
FIELD_SERIALIZER_END()
