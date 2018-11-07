#pragma once
#include "GenericDeclaration.h"

struct TraitSubItem
{
	std::string ElementName;
	std::string ExportName;
	std::size_t Index;
};
FIELD_SERIALIZER_BEGIN(TraitSubItem)
	SERIALIZE_FIELD(ElementName)
	SERIALIZE_FIELD(ExportName)
	SERIALIZE_FIELD(Index)
FIELD_SERIALIZER_END()

struct Trait
{
	GenericDeclaration Generic;

	//TODO use a different struct
	std::vector<TraitSubItem> Traits; //ElementName not used, only for export
	std::vector<TraitSubItem> Types; //ElementName not used, only for export
	std::vector<TraitSubItem> Functions;
	std::vector<TraitSubItem> Fields;
};
FIELD_SERIALIZER_BEGIN(Trait)
	SERIALIZE_FIELD(Generic)
	SERIALIZE_FIELD(Traits)
	SERIALIZE_FIELD(Types)
	SERIALIZE_FIELD(Functions)
	SERIALIZE_FIELD(Fields)
FIELD_SERIALIZER_END()

/*

Note that for one trait on one GenericDeclaration, we have 3 different
type lists:
1. TypeReferences in GenericConstrain
	Not only for trait constrain, but also for system defined constrains. 
	Elements in this list are calculated as required by Target and Arguments 
	before evaluation of the constrain. 
	This list is to allow something like
		class A<T> requires T : IComparable<List<T>>
	Any error in calculating the elements of this list is an loader error.
	(Again, we don't have SFINAE.)
	In order to ensure IComparable<List<T>> exists, we need to use REF_TRY 
	in the type list:
		[0]: REF_TRY, 0
		[1]: REF_IMPORT, `IComparable<?>`
		[2]: REF_IMPORT, `List<?>`
		[3]: REF_ARG, 0
		[4]: REF_EMPTY
		[5]: REF_EMPTY
		Type = CONSTRAIN_BASE
		Target = #3
		Arguments = [#0]
	Note that the elements that is not used are not calculated, and all results 
	of this list are never recorded, so they cannot be referenced by parent.
	Note that this list has no function counterpart.
	We should allow this list to reference exported types from another constrain
	(limited to constrains that are already checked).

2. Types in GenericDeclaration of the trait
	This list helps the calculation of the trait:
	(1) Have additional constrains.
		If any constrain of a trait fails, the trait will not be instantiated 
		and therefore the constrain check fails. The target of sub-constrain 
		can be different from parent.
	(2) Contain reference types used by the trait.
		When defining a function in a trait, we need some other types.
	Note that this list cannot contain REF_TRY. Doing so leads to an error.

3. Types in Trait
	This list is not a type reference list. 
	Export types that has been verified to exist.

*/
