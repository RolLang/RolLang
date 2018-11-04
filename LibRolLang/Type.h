#pragma once
#include "GenericDeclaration.h"

enum TypeStorageMode : unsigned char
{
	TSM_INVALID,
	TSM_GLOBAL,
	TSM_VALUE,
	TSM_REFERENCE,
	TSM_INTERFACE,
};

struct TypeInheritance
{
	std::size_t InheritedType;
	std::size_t VirtualTableType;
};
FIELD_SERIALIZER_BEGIN(TypeInheritance)
	SERIALIZE_FIELD(InheritedType)
	SERIALIZE_FIELD(VirtualTableType)
FIELD_SERIALIZER_END()

struct Type
{
	GenericDeclaration Generic;

	TypeStorageMode GCMode;
	std::vector<std::size_t> Fields;

	TypeInheritance Base;
	std::vector<TypeInheritance> Interfaces;

	std::size_t Initializer; //void(void)
	std::size_t Finalizer; //void(T)
};
FIELD_SERIALIZER_BEGIN(Type)
	SERIALIZE_FIELD(Generic)
	SERIALIZE_FIELD(GCMode)
	SERIALIZE_FIELD(Fields)
	SERIALIZE_FIELD(Base)
	SERIALIZE_FIELD(Initializer)
	SERIALIZE_FIELD(Finalizer)
FIELD_SERIALIZER_END()
