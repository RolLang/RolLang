#include "stdafx.h"

namespace LibRolLangTest
{
	TEST_CLASS(ImportTest)
	{
		TEST_METHOD(ImportConstant)
		{
			TestAssemblyListBuilder builder;
			builder.BeginAssembly("Test");
			builder.ExportConstant("Test.Value", 101);
			builder.EndAssembly();

			builder.BeginAssembly("Core");
			auto importId = builder.ImportConstant("Test", "Test.Value");
			TestAssemblyListBuilder::TypeReference tInt32;
			builder.WriteCoreCommon(&tInt32, nullptr, nullptr);

			builder.BeginFunction("Core.GetValue");
			builder.Link(true, false);
			builder.Signature(tInt32, { tInt32 });
			auto kid = builder.AddFunctionImportConstant(tInt32, importId);
			builder.AddInstruction(OP_CONST, kid);
			builder.AddInstruction(OP_LOAD, 0);
			builder.AddInstruction(OP_RET, 0);
			builder.EndFunction();

			builder.EndAssembly();

			Interpreter i(builder.Build());
			Assert::IsTrue(i.RegisterNativeType("Core", "Core.Int32", 4, 4));

			auto fid = i.GetFunction("Core", "Core.GetValue", {});
			Assert::AreNotEqual(fid, SIZE_MAX);

			int result;
			Assert::IsTrue(i.Call(fid));
			Assert::IsTrue(i.Pop(&result));
			Assert::AreEqual(result, 101);
		}
	};
}
