#pragma once
#include "GenericDeclaration.h"

enum Features
{
	//All others
	F_BASIC,
	//Allocate reference type objects, GC.
	F_REFTYPE,
	//Throw, catch exceptions (including try-finally)
	F_EXCEPTION,
	//Pointer arithmetic and conversion (not including reference type conversion)
	F_RAWPOINTER,
};

//high 5 bits: Opcodes. low 3 bits: OPR.
//if OPR >= 6, additional bytes are taken. 6: 4 more bytes. 7: 1 more bytes.
enum Opcodes : unsigned char
{
	//Level 1
	OP_NOP, //No-op. Stack: -0+0
	//Level 1
	OP_ERR, //Raise error. OPR: 0. Stack: -0+0
	OP_ERROBJ, //Raise error with exception object. OPR: error code. Stack: -1+0

	//Level 1
	OP_ARG, //Load arg ptr. OPR: index. Stack: -0+1
	OP_LOCAL, //Load local ptr. OPR: index. Stack: -0+1
	//Level 1
	OP_CONST, //Load constant ptr. OPR: index. Stack: -0+1
	OP_GLOBAL, //Load global ptr. OPR: type. Stack: -0+1

	//Level 1
	OP_LOAD, //Pop ptr (stack) and push data. Stack: -1+1
	OP_STORE, //Pop ptr and data, and store data to ptr. Stack: -2+0

	//Level 1
	OP_DUP, //Duplicate one stack item. OPR: index. Stack: -0+1
	//Level 1
	OP_POP, //Pop items from stack. OPR: number to pop. Stack: -x+0

	OP_INTRINSIC1, //1-operand calculation. OPR: code. Stack: -1+1
	OP_INTRINSIC2, //2-operand calculation for ints. OPR: code. Stack: -2+1

	OP_INITTYPE, //Call static initializer of a TSM_Global type. OPR: type. Stack: -0+0
	OP_ALLOC, //Allocate memory and push ptr. OPR: type. Stack: -0+1
	OP_MEMINIT, //Init memory to zeros. Stack: -1+0

	//TODO need one to throw if OP_CONVREF fails.
	OP_CONVREF, //Convert reference type. OPR: type. Stack: -1+1
	OP_CONVARTH, //Convert arithmetic type. OPR: type. Stack: -1+1
	OP_CONVPTR, //Conversion involving a pointer type. OPR: type. Stack: -1+1

	OP_SIZEOF, //Push size of type. OPR: type. Stack: -0+1
	OP_FIELD, //Pop ptr on stack and push field ptr. OPR: field index. Stack: -1+1
	OP_FUNCPTR, //Push function managed ptr. OPR: function. Stack: -0+1
	OP_FUNCNATIVE, //Pop managed ptr and push native ptr. Stack: -1+1

	//Level 1
	OP_CALL, //Call function by id. OPR: function. Stack: -x+1?
	OP_CALLPTR, //Call function by managed ptr. OPR: number of args. Stack: -x+1?
	OP_CALLNATIVE, //Call function by native ptr. OPR: number of args Stack: -x+1?
	
	OP_BRANCH_F, //Pop condition and if true, jump forward. OPR: relative (unsigned). Stack: -1+0
	OP_BRANCH_B, //Pop condition and if true, jump backward. OPR: relative (unsigned). Stack: -1+0
	//Level 1
	OP_RET, //Return to caller. Stack: -1?+0

	OP_BRANCHERR, //Set error handler dest. OPR: 1 or 4 byte address x 3. Stack: -0+0
	              //Following bytes (length depending on OPR): catch start, finally start, finally end.

	OP_OPCOUNT,
};

static_assert(OP_OPCOUNT <= 32, "Opcode too long");

enum Intrinsic2Codes
{
	I2_ADD,
};

struct FunctionConst
{
	std::size_t Offset;
	std::size_t Length;
	std::size_t TypeId;
};
FIELD_SERIALIZER_BEGIN(FunctionConst)
	SERIALIZE_FIELD(Offset)
	SERIALIZE_FIELD(Length)
	SERIALIZE_FIELD(TypeId)
FIELD_SERIALIZER_END()

struct FunctionParameter
{
	std::size_t TypeId;
};
FIELD_SERIALIZER_BEGIN(FunctionParameter)
	SERIALIZE_FIELD(TypeId)
FIELD_SERIALIZER_END()

struct FunctionLocal
{
	std::size_t TypeId;
};
FIELD_SERIALIZER_BEGIN(FunctionLocal)
	SERIALIZE_FIELD(TypeId)
FIELD_SERIALIZER_END()

struct FunctionReturnValue
{
	std::size_t TypeId;
};
FIELD_SERIALIZER_BEGIN(FunctionReturnValue)
	SERIALIZE_FIELD(TypeId)
FIELD_SERIALIZER_END()

struct Function
{
	GenericDeclaration Generic;

	FunctionReturnValue ReturnValue;
	std::vector<FunctionParameter> Parameters;

	std::vector<FunctionLocal> Locals;
	std::vector<unsigned char> Instruction;
	std::vector<unsigned char> ConstantData;
	std::vector<FunctionConst> ConstantTable;
};
FIELD_SERIALIZER_BEGIN(Function)
	SERIALIZE_FIELD(Generic)
	SERIALIZE_FIELD(ReturnValue)
	SERIALIZE_FIELD(Parameters)
	SERIALIZE_FIELD(Locals)
	SERIALIZE_FIELD(Instruction)
	SERIALIZE_FIELD(ConstantData)
	SERIALIZE_FIELD(ConstantTable)
FIELD_SERIALIZER_END()
