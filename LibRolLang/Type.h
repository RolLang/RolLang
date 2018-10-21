#pragma once
#include <vector>
#include "GenericDeclaration.h"

enum TypeStorageMode
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
	//TODO type initializer for Global types.
	std::size_t OnFinalize; //function id
};
FIELD_SERIALIZER_BEGIN(Type)
	SERIALIZE_FIELD(Generic)
	SERIALIZE_FIELD(GCMode)
	SERIALIZE_FIELD(Fields)
	SERIALIZE_FIELD(OnFinalize)
FIELD_SERIALIZER_END()
