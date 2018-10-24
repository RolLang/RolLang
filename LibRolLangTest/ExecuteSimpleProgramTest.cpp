#include "stdafx.h"
#include "../LibRolLang/Interpreter.h"
#include "TestAssemblyListBuilder.h"

namespace LibRolLangTest
{
	TEST_CLASS(ExecuteSimpleProgramTest)
	{
		TEST_METHOD(EchoInt32)
		{
			TestAssemblyListBuilder builder;
			builder.BeginAssembly("Core");

			TestAssemblyListBuilder::TypeReference tInt32;
			builder.WriteCoreCommon(&tInt32, nullptr, nullptr);

			builder.BeginFunction("Core.EchoInt32");
			builder.Link(true, true);
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
			Assert::AreEqual(result, 100);
		}
	};
}
