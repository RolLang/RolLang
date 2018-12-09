#pragma once
#include "GenericDeclaration.h"

struct TraitField
{
	std::string ElementName;
	std::size_t Type;

	std::string ExportName;
};
FIELD_SERIALIZER_BEGIN(TraitField)
	SERIALIZE_FIELD(ElementName)
	SERIALIZE_FIELD(Type)
	SERIALIZE_FIELD(ExportName)
FIELD_SERIALIZER_END()

struct TraitFunction
{
	std::string ElementName;
	std::size_t ReturnType;
	std::vector<std::size_t> ParameterTypes;

	std::string ExportName;
};
FIELD_SERIALIZER_BEGIN(TraitFunction)
	SERIALIZE_FIELD(ElementName)
	SERIALIZE_FIELD(ReturnType)
	SERIALIZE_FIELD(ParameterTypes)
	SERIALIZE_FIELD(ExportName)
FIELD_SERIALIZER_END()

struct TraitExportItem
{
	std::string ExportName;
	std::size_t Index;
};
FIELD_SERIALIZER_BEGIN(TraitExportItem)
	SERIALIZE_FIELD(ExportName)
	SERIALIZE_FIELD(Index)
FIELD_SERIALIZER_END()

struct Trait
{
	GenericDeclaration Generic;
	std::vector<TraitExportItem> Types;
	std::vector<TraitFunction> Functions;
	std::vector<TraitField> Fields;
};
FIELD_SERIALIZER_BEGIN(Trait)
	SERIALIZE_FIELD(Generic)
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
		[0]: REF_TRY, 1
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
	Note that list list cannot contain REF_SELF. Using it potentially leads to 
	cyclic referencing:
		class A<T> requires A<T> : SomeTrait
	Here A<T> must be exist before we can check SomeTrait. So, for A<int> for 
	example, A<int> exists => SomeTrait(A<int>) => A<int> exists
	Only this list can contain REF_ANY, which gives an undetermined type to the 
	constrain (can be trait or system-defined constrain).

2. Types in GenericDeclaration of the trait
	This list helps the calculation of the trait:
	(1) Have additional constrains.
		If any constrain of a trait fails, the trait will not be instantiated 
		and therefore the constrain check fails. The target of sub-constrain 
		can be different from parent.
	(2) Contain reference types used by the trait.
		When defining a function in a trait, we need some other types.
	Note that this list cannot contain REF_TRY. Doing so leads to an error.
	This list cannot contain REF_ANY.

3. Types in Trait
	This list is not a type reference list. 
	Used to export types that has been verified to exist.

*/
/*

Type export from constrain to parent generic declaration:
<name>/<name>/.../<object_name>
For type and <object_name> = '.target', export the target type. This is very
useful for CONSTRAIN_EXIST.

*/