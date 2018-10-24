#pragma once
#include "GenericDeclaration.h"

enum TypeStorageMode : unsigned char
{
	TSM_GLOBAL,
	TSM_VALUE,
	TSM_REF,
};

struct Type
{
	GenericDeclaration Generic;

	TypeStorageMode GCMode;
	std::vector<std::size_t> Fields;

	std::size_t Initializer; //void(void)
	std::size_t Finalizer; //void(Pointer<T>)
};
FIELD_SERIALIZER_BEGIN(Type)
	SERIALIZE_FIELD(Generic)
	SERIALIZE_FIELD(GCMode)
	SERIALIZE_FIELD(Fields)
	SERIALIZE_FIELD(Initializer)
	SERIALIZE_FIELD(Finalizer)
FIELD_SERIALIZER_END()
