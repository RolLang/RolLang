#include "stdafx.h"

namespace LibRolLangTest
{
	using namespace RuntimeLoaderHelper;

	//TODO import multiple objects (id > 0)
	//TODO redirect method in builder (instead of manually +1)
	//TODO redirect more objects

	TEST_CLASS(ImportTest)
	{
		TEST_METHOD(ImportType)
		{
			TestAssemblyListBuilder builder;
			std::size_t fid1, fid2;
			{
				builder.BeginAssembly("Core");
				auto n1 = builder.BeginType(TSM_VALUE, "Core.NativeType");
				builder.Link(false, true);
				builder.EndType();
				builder.BeginType(TSM_VALUE, "Core.ExportType1");
				builder.Link(true, false);
				builder.AddField(n1);
				builder.EndType();
				builder.BeginType(TSM_VALUE, "Core.ExportType2");
				builder.Link(true, false);
				auto gt = builder.AddGenericParameter();
				builder.AddField(n1);
				builder.AddField(gt);
				builder.EndType();
				builder.BeginFunction("Core.ExportFunction1");
				builder.Signature({}, {});
				builder.Link(true, false);
				builder.EndFunction();
				builder.BeginFunction("Core.ExportFunction2");
				auto gf = builder.AddGenericParameter();
				builder.Signature(gf, {});
				builder.Link(true, false);
				builder.EndFunction();
				builder.EndAssembly();

				builder.BeginAssembly("Test");
				auto t1 = builder.ImportType("Core", "Core.ExportType1", GenericDefArgumentListSize::Create(0));
				auto t2 = builder.ImportType("Core", "Core.ExportType2", GenericDefArgumentListSize::Create(1));
				auto f1 = builder.ImportFunction("Core", "Core.ExportFunction1", GenericDefArgumentListSize::Create(0));
				auto f2 = builder.ImportFunction("Core", "Core.ExportFunction2", GenericDefArgumentListSize::Create(1));
				auto n2 = builder.BeginType(TSM_VALUE, "Test.NativeType");
				builder.Link(false, true);
				builder.EndType();
				builder.BeginType(TSM_VALUE, "Test.TestType");
				builder.Link(true, false);
				builder.AddField(t1); //1,1@0-1
				builder.AddField(builder.MakeType(t2, { t1 })); //1+1,1@1-3
				builder.AddField(builder.MakeType(t2, { n2 })); //1+(1+)2,2@4-8
				builder.EndType();
				builder.BeginFunction("Test.TestFunction");
				builder.Signature({}, {});
				builder.Link(true, false);
				fid1 = builder.AddFunctionRef(f1);
				fid2 = builder.AddFunctionRef(builder.MakeFunction(f2, { n2 }));
				builder.EndAssembly();
			}

			RuntimeLoader loader(builder.Build());
			{
				LoadNativeType(&loader, "Core", "Core.NativeType", 1);
				LoadNativeType(&loader, "Test", "Test.NativeType", 2);

				auto t = LoadType(&loader, "Test", "Test.TestType", false);
				CheckValueTypeBasic(&loader, t);
				CheckValueTypeSize(t, 8, 2);
				CheckFieldOffset(t, { 0, 1, 4 });

				auto f = LoadFunction(&loader, "Test", "Test.TestFunction", false);
				CheckFunctionBasic(&loader, f);
				CheckFunctionTypes(f, 0, {});
				auto f1 = f->References.Functions[fid1];
				auto f2 = f->References.Functions[fid2];
				CheckFunctionBasic(&loader, f1);
				CheckFunctionTypes(f1, 0, {});
				CheckFunctionBasic(&loader, f2);
				CheckFunctionTypes(f2, 2, {});
			}
		}

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
			builder.AddInstruction(OP_CONST, (std::uint32_t)kid);
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
			Assert::AreEqual(101, result);
		}

		TEST_METHOD(ImportTrait)
		{
			TestAssemblyListBuilder builder;
			{
				builder.BeginAssembly("Test");
				auto tr = builder.BeginTrait("Test.Trait");
				builder.Link(true, false);
				builder.AddTraitFunction({}, {}, "F", "F");
				builder.EndTrait();
				builder.EndAssembly();
				builder.BeginAssembly("Core");
				auto f = builder.BeginFunction("Core.Function");
				builder.Signature({}, {});
				builder.EndFunction();
				auto tri = builder.ImportTrait("Test", "Test.Trait", GenericDefArgumentListSize::Create(0));
				auto tt1 = builder.BeginType(TSM_VALUE, "Core.TargetType1");
				builder.AddMemberFunction("F", f);
				builder.EndType();
				builder.BeginType(TSM_VALUE, "Core.TestType1");
				builder.Link(true, false);
				builder.AddConstrain(tt1, {}, CONSTRAIN_TRAIT_IMPORT, tri.Id);
				builder.EndType();
				auto tt2 = builder.BeginType(TSM_VALUE, "Core.TargetType2");
				builder.EndType();
				builder.BeginType(TSM_VALUE, "Core.TestType2");
				builder.Link(true, false);
				builder.AddConstrain(tt2, {}, CONSTRAIN_TRAIT_IMPORT, tri.Id);
				builder.EndType();
				builder.EndAssembly();
			}
			RuntimeLoader loader(builder.Build());
			{
				LoadType(&loader, "Core", "Core.TestType1", false);
				LoadType(&loader, "Core", "Core.TestType2", true);
			}
		}

		TEST_METHOD(Redirect)
		{
			TestAssemblyListBuilder builder;
			{
				builder.BeginAssembly("Core");
				auto n = builder.BeginType(TSM_VALUE, "Core.TestType");
				builder.Link(true, true);
				builder.EndType();
				builder.BeginFunction("Core.TestFunction");
				builder.Signature({}, { n });
				builder.Link(true, false);
				builder.EndFunction();
				builder.EndAssembly();

				builder.BeginAssembly("Test");
				builder.BeginType(TSM_VALUE, "Test.NotUsedType");
				builder.EndType();
				builder.BeginFunction("Test.NotUsedFunction");
				builder.EndFunction();
				auto t = builder.ImportType("Core", "Core.TestType");
				builder.ExportType("Test.ExportType", 1 + t.Id);
				auto f = builder.ImportFunction("Core", "Core.TestFunction");
				builder.ExportFunction("Test.ExportFunction", 1 + f.Id);
				builder.EndAssembly();
			}

			RuntimeLoader loader(builder.Build());
			{
				LoadNativeType(&loader, "Core", "Core.TestType", 1);
				auto t = LoadType(&loader, "Test", "Test.ExportType", false);
				CheckValueTypeBasic(&loader, t);
				CheckValueTypeSize(t, 1, 1);
				auto f = LoadFunction(&loader, "Test", "Test.ExportFunction", false);
				CheckFunctionBasic(&loader, f);
				CheckFunctionTypes(f, 0, { 1 });
			}
		}
	};
}
