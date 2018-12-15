#include "stdafx.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace LibRolLangTest
{
	using Builder = TestAssemblyListBuilder;
	using TypeReference = Builder::TypeReference;
	using namespace RuntimeLoaderHelper;

	TEST_CLASS(LoadFunctionTest)
	{
	private:
		static void SetupEmptyFunction(Builder& builder)
		{
			builder.BeginFunction("Test.EmptyFunc");
			builder.Link(true, false);
			builder.Signature({}, {});
			builder.EndFunction();
		}

		static void CheckEmptyFunction(RuntimeLoader* loader)
		{
			auto f = LoadFunction(loader, "Test", "Test.EmptyFunc", false);
			CheckFunctionBasic(loader, f);
			CheckFunctionTypes(f, 0, {});
		}

		static void SetupSimpleFunction(Builder& builder)
		{
			auto t = builder.BeginType(TSM_VALUE, "Test.Native4");
			builder.Link(false, true);
			builder.EndType();

			builder.BeginFunction("Test.TestFunc1");
			builder.Link(true, false);
			builder.Signature(t, { t, t });
			builder.EndFunction();
		}

		static void CheckSimpleFunction(RuntimeLoader* loader)
		{
			LoadNativeType(loader, "Test", "Test.Native4", 4);

			auto f = LoadFunction(loader, "Test", "Test.TestFunc1", false);
			CheckFunctionBasic(loader, f);
			CheckFunctionTypes(f, 4, { 4, 4 });
		}

		static void SetupReferencingTypeFunction(Builder& builder)
		{
			auto t1 = builder.ForwardDeclareType();
			auto f1 = builder.BeginFunction("Test.TestFunc1");
			builder.Link(false, false);
			builder.Signature({}, { t1 });
			builder.EndFunction();

			builder.BeginType(TSM_REFERENCE, "Test.TestType1", t1);
			builder.Link(false, false);
			builder.SetTypeHandlers({}, f1);
			builder.EndType();

			builder.BeginFunction("Test.TestFunc2");
			builder.Link(true, false);
			builder.Signature(t1, {});
			builder.EndFunction();
		}

		static void CheckReferencingTypeFunction(RuntimeLoader* loader)
		{
			auto f2 = LoadFunction(loader, "Test", "Test.TestFunc2", false);
			auto t1 = f2->ReturnValue;
			Assert::IsNotNull(t1);
			auto f1 = t1->Finalizer;
			Assert::IsNotNull(f1);

			CheckFunctionBasic(loader, f1);
			CheckFunctionTypes(f1, 0, { sizeof(void*) });

			CheckFunctionBasic(loader, f2);
			CheckFunctionTypes(f2, sizeof(void*), {});
		}

		static void SetupCyclicFunction(Builder& builder)
		{
			auto f2 = builder.ForwardDeclareFunction();
			auto f1 = builder.BeginFunction("Test.TestFunc1");
			builder.Link(false, false);
			builder.Signature({}, {});
			builder.AddFunctionRef(f2);
			builder.EndFunction();

			builder.BeginFunction("Test.TestFunc2", f2);
			builder.Link(true, false);
			builder.Signature({}, {});
			builder.AddFunctionRef(f1);
			builder.EndFunction();
		}

		static void CheckCyclicFunction(RuntimeLoader* loader)
		{
			auto f2 = LoadFunction(loader, "Test", "Test.TestFunc2", false);
			auto f1 = f2->References.Functions[0];
			Assert::IsNotNull(f1);

			CheckFunctionBasic(loader, f1);
			CheckFunctionTypes(f1, 0, {});

			CheckFunctionBasic(loader, f2);
			CheckFunctionTypes(f2, 0, {});
		}

	public:
		TEST_METHOD(LoadEmptyFunction)
		{
			Builder builder;

			builder.BeginAssembly("Test");
			SetupEmptyFunction(builder);
			builder.EndAssembly();

			RuntimeLoader loader(builder.Build());
			CheckEmptyFunction(&loader);
		}

		TEST_METHOD(LoadSimpleFunction)
		{
			Builder builder;

			builder.BeginAssembly("Test");
			SetupSimpleFunction(builder);
			builder.EndAssembly();

			RuntimeLoader loader(builder.Build());
			CheckSimpleFunction(&loader);
		}

		TEST_METHOD(LoadReferencingTypeFunction)
		{
			Builder builder;

			builder.BeginAssembly("Test");
			SetupReferencingTypeFunction(builder);
			builder.EndAssembly();

			RuntimeLoader loader(builder.Build());
			CheckReferencingTypeFunction(&loader);
		}

		TEST_METHOD(LoadCyclicFunction)
		{
			Builder builder;

			builder.BeginAssembly("Test");
			SetupCyclicFunction(builder);
			builder.EndAssembly();

			RuntimeLoader loader(builder.Build());
			CheckCyclicFunction(&loader);
			CheckCyclicFunction(&loader);
		}
	};
}
