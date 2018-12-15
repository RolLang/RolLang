#include "stdafx.h"

namespace LibRolLangTest
{
	TEST_CLASS(ExecuteSimpleProgramTest)
	{
		static bool AddInt32(Interpreter* i, void* data)
		{
			Assert::IsNull(data);
			int a, b;
			Assert::IsTrue(i->Pop(&a));
			Assert::IsTrue(i->Pop(&b));
			Assert::IsTrue(i->Push(a + b));
			return true;
		}

		TEST_METHOD(EchoInt32)
		{
			TestAssemblyListBuilder builder;
			builder.BeginAssembly("Core");

			TestAssemblyListBuilder::TypeReference tInt32;
			builder.WriteCoreCommon(&tInt32, nullptr, nullptr);

			builder.BeginFunction("Core.EchoInt32");
			builder.Link(true, false);
			builder.Signature(tInt32, { tInt32 });
			builder.AddInstruction(OP_ARG, 0);
			builder.AddInstruction(OP_LOAD, 0);
			builder.AddInstruction(OP_RET, 0);
			builder.EndFunction();

			builder.EndAssembly();

			Interpreter i(builder.Build());
			Assert::IsTrue(i.RegisterNativeType("Core", "Core.Int32", 4, 4));

			auto fid = i.GetFunction("Core", "Core.EchoInt32", {});
			Assert::AreNotEqual(fid, SIZE_MAX);

			int result;
			Assert::IsTrue(i.Push(100));
			Assert::IsTrue(i.Call(fid));
			Assert::IsTrue(i.Pop(&result));
			Assert::AreEqual(100, result);
		}

		TEST_METHOD(Add100)
		{
			TestAssemblyListBuilder builder;
			builder.BeginAssembly("Core");

			TestAssemblyListBuilder::TypeReference tInt32;
			builder.WriteCoreCommon(&tInt32, nullptr, nullptr);

			auto funcAdd = builder.BeginFunction("Core.AddInt32");
			builder.Link(false, true);
			builder.Signature(tInt32, { tInt32, tInt32 });
			builder.EndFunction();

			builder.BeginFunction("Core.Add100");
			auto funcAddId = builder.AddFunctionRef(funcAdd);
			builder.Link(true, false);
			builder.Signature(tInt32, { tInt32 });
			auto constantId = builder.AddFunctionConstant(tInt32, 100);
			builder.AddInstruction(OP_ARG, 0);
			builder.AddInstruction(OP_LOAD, 0);
			builder.AddInstruction(OP_CONST, (std::uint32_t)constantId);
			builder.AddInstruction(OP_LOAD, 0);
			builder.AddInstruction(OP_CALL, (std::uint32_t)funcAddId);
			builder.AddInstruction(OP_RET, 0);
			builder.EndFunction();

			builder.EndAssembly();

			Interpreter i(builder.Build());
			Assert::IsTrue(i.RegisterNativeType("Core", "Core.Int32", 4, 4));
			Assert::IsTrue(i.RegisterNativeFunction("Core", "Core.AddInt32", AddInt32, nullptr));

			auto fid = i.GetFunction("Core", "Core.Add100", {});
			Assert::AreNotEqual(fid, SIZE_MAX);

			int result;
			Assert::IsTrue(i.Push(1000));
			Assert::IsTrue(i.Call(fid));
			Assert::IsTrue(i.Pop(&result));
			Assert::AreEqual(1100, result);
		}
	};
}
