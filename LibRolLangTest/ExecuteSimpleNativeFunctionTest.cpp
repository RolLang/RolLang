#include "stdafx.h"

namespace LibRolLangTest
{
	TEST_CLASS(ExecuteSimpleNativeFunctionTest)
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

		TEST_METHOD(ExecuteNativeFunction)
		{
			TestAssemblyListBuilder builder;
			builder.BeginAssembly("Core");

			auto t = builder.BeginType(TSM_VALUE, "Core.Int32");
			builder.Link(true, true);
			builder.EndType();

			builder.BeginFunction("Core.AddInt32");
			builder.Link(true, true);
			builder.Signature(t, { t, t });
			builder.EndFunction();

			builder.EndAssembly();
			
			Interpreter i(builder.Build());
			Assert::IsTrue(i.RegisterNativeType("Core", "Core.Int32", 4, 4));
			Assert::IsTrue(i.RegisterNativeFunction("Core", "Core.AddInt32", AddInt32, nullptr));
			
			auto fid = i.GetFunction("Core", "Core.AddInt32", {});
			Assert::AreNotEqual(fid, SIZE_MAX);

			int result;
			Assert::IsTrue(i.Push(100));
			Assert::IsTrue(i.Push(1000));
			Assert::IsTrue(i.Call(fid));
			Assert::IsTrue(i.Pop(&result));
			Assert::AreEqual(1100, result);
		}
	};
}
