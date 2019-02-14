#pragma once
#include "Serialization.h"

namespace RolLang {

enum ReferenceType_ : unsigned char
{
	REF_EMPTY,

	//Meta (note that these cannot be cloned)
	REF_LISTEND, //end of a argument list, used with REF_ASSEMBLY, REF_IMPORT and REF_SUBTYPE
	REF_SEGMENT, //start of a segment in the argument list
	REF_ARGUMENTSEG, //index = segment index. must follow REF_ARGUMENT

	REF_CLONE, //refer to another entry in the list. index = index in the same list
	REF_ASSEMBLY, //index = assembly type/function array index
	REF_IMPORT, //index = import #
	REF_CONSTRAINT, //import from constraint. index = index in name list
	//TODO REF_CONSTRAINTGENERIC for generic function from trait?

	REF_ARGUMENT, //index = generic parameter list index. must be followed by a REF_ARGUMENTSEG
	REF_SELF, //for type, the type itself. for trait, the target type
	REF_SUBTYPE, //sub type of the given type. index = index in name list
	//Note: REF_SUBTYPE can be used to implement reference to static type. (name = '.static')
	REF_CLONETYPE, //for function generic arguments, clone from the type list

	//For field reference only
	REF_FIELDID, //index = field id
	//TODO member field by name

	//Following 2 are only for constraint type list only
	REF_TRY, //same as CLONE except for that it allow the refered calculation to fail (not an error)
	REF_ANY, //undetermined generic type in trait constraints (can only be used as argument)

	REF_REFTYPES = 127,
	REF_FORCELOAD = 128,
};

struct ReferenceType
{
	ReferenceType_ _type;

	ReferenceType() = default;
	ReferenceType(ReferenceType_ type) : _type(type)
	{
#ifdef _DEBUG
		static const char* debugNames[] =
		{
			"EMPTY",
			"END", "SEGMENT", "ARGSEG",
			"CLONE", "ASSEMBLY", "IMPORT", "CONSTRAINT",
			"ARG", "SELF", "SUBTYPE", "CLONETYPE",
			"FIELDID",
			"TRY", "ANY",
		};
		DebugString = debugNames[type & REF_REFTYPES];
		if (type & REF_FORCELOAD)
		{
			DebugString += " *";
		}
#endif
	}
	ReferenceType(int val) : ReferenceType((ReferenceType_)val) {}

	operator ReferenceType_() const { return _type; }

#ifdef _DEBUG
	std::string DebugString;
#endif
};
template <>
struct Serializer<ReferenceType>
{
	static void Read(std::istream& s, ReferenceType& val)
	{
		ReferenceType_ val2;
		s.read((char*)&val2, sizeof(ReferenceType_));
		val = val2; //Trigger constructor
	}

	static void Write(std::ostream& s, const ReferenceType& val)
	{
		s.write((const char*)&val._type, sizeof(ReferenceType_));
	}
};

enum ConstraintType : unsigned char
{
	CONSTRAINT_EXIST, //(T) -> T exists
	CONSTRAINT_SAME, //<T1>(T) -> T1 == T
	CONSTRAINT_BASE, //<T1>(T) -> T1 == T or T1 is in the base type chain from T
	CONSTRAINT_INTERFACE, //<T1>(T) -> T implements T1
	CONSTRAINT_TRAIT_ASSEMBLY, //<...>(T) -> check trait (in the same assembly)
	CONSTRAINT_TRAIT_IMPORT, //import trait
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

struct GenericConstraint
{
	ConstraintType Type;
	std::size_t Index;

	std::vector<DeclarationReference> TypeReferences;
	std::vector<std::string> NamesList;
	std::size_t Target;
	std::vector<std::size_t> Arguments; //TODO support for segment

	std::string ExportName;
};
FIELD_SERIALIZER_BEGIN(GenericConstraint)
	SERIALIZE_FIELD(Type)
	SERIALIZE_FIELD(Index)
	SERIALIZE_FIELD(TypeReferences)
	SERIALIZE_FIELD(NamesList)
	SERIALIZE_FIELD(Target)
	SERIALIZE_FIELD(Arguments)
	SERIALIZE_FIELD(ExportName)
FIELD_SERIALIZER_END()

struct GenericArgumentListSegmentSize
{
	std::size_t Size;
	bool IsVariable;

	bool operator== (const GenericArgumentListSegmentSize& rhs) const
	{
		return Size == rhs.Size && IsVariable == rhs.IsVariable;
	}
	bool operator!= (const GenericArgumentListSegmentSize& rhs) const
	{
		return !(*this == rhs);
	}
};
FIELD_SERIALIZER_BEGIN(GenericArgumentListSegmentSize)
	SERIALIZE_FIELD(Size)
	SERIALIZE_FIELD(IsVariable)
FIELD_SERIALIZER_END()

struct GenericDefArgumentListSize
{
	std::vector<GenericArgumentListSegmentSize> Segments;

	bool IsEmpty() const { return Segments.size() == 0; }
	bool IsSingle() const
	{
		return Segments.size() == 1 && Segments[0].Size == 1 && !Segments[0].IsVariable;
	}

	bool CanMatch(const std::vector<std::size_t>& size) const
	{
		//For backward compatibility (temporary), ignore empty single dimension
		//TODO remove this (after support for multilist on Constraint arg)
		if (size.size() == 1 && size[0] == 0)
		{
			if (IsEmpty()) return true;
			if (Segments.size() == 1 && Segments[0].Size == 0 && !Segments[0].IsVariable) return true;
			return false;
		}

		if (Segments.size() != size.size()) return false;
		for (std::size_t i = 0; i < size.size(); ++i)
		{
			if (Segments[i].IsVariable)
			{
				if (size[i] < Segments[i].Size) return false;
			}
			else
			{
				if (size[i] != Segments[i].Size) return false;
			}
		}
		return true;
	}

	bool operator== (const GenericDefArgumentListSize& rhs) const
	{
		return Segments == rhs.Segments;
	}
	bool operator!= (const GenericDefArgumentListSize& rhs) const
	{
		return !(*this == rhs);
	}

	static GenericDefArgumentListSize Create(std::size_t n)
	{
		GenericDefArgumentListSize ret;
		//For backward compatibility (temporary), only add when n != 0
		if (n > 0) ret.Segments.push_back({ n, false });
		return std::move(ret);
	}
};
FIELD_SERIALIZER_BEGIN(GenericDefArgumentListSize)
	SERIALIZE_FIELD(Segments)
FIELD_SERIALIZER_END()

struct GenericDeclaration
{
	GenericDefArgumentListSize ParameterCount;
	std::vector<GenericConstraint> Constraints;

	std::vector<DeclarationReference> Types;
	std::vector<DeclarationReference> Functions;
	std::vector<DeclarationReference> Fields;
	std::vector<std::string> NamesList;
};
FIELD_SERIALIZER_BEGIN(GenericDeclaration)
	SERIALIZE_FIELD(ParameterCount)
	SERIALIZE_FIELD(Constraints)
	SERIALIZE_FIELD(Types)
	SERIALIZE_FIELD(Functions)
	SERIALIZE_FIELD(Fields)
	SERIALIZE_FIELD(NamesList)
FIELD_SERIALIZER_END()

}
