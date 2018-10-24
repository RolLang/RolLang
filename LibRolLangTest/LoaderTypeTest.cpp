#include "stdafx.h"
#include "../LibRolLang/RuntimeLoader.h"
#include "TestAssemblyListBuilder.h"

namespace LibRolLangTest
{
	using Builder = TestAssemblyListBuilder;
	using TypeReference = Builder::TypeReference;

	TEST_CLASS(LoadTypeTest)
	{
	private:
		static RuntimeType* LoadType(RuntimeLoader* loader,
			const std::string& a, const std::string& n,
			std::vector<RuntimeType*>args, bool shouldFail)
		{
			LoadingArguments la;
			la.Assembly = a;
			la.Id = loader->FindExportType(a, n);
			la.Arguments = args;
			std::string err;
			auto ret = loader->GetType(la, err);
			if (shouldFail)
			{
				Assert::IsNull(ret, ToString(err.c_str()).c_str());
			}
			else
			{
				Assert::IsNotNull(ret, ToString(err.c_str()).c_str());
			}
			return ret;
		}

		static RuntimeType* LoadType(RuntimeLoader* loader,
			const std::string& a, const std::string& n, bool shouldFail)
		{
			return LoadType(loader, a, n, {}, shouldFail);
		}

		static RuntimeType* LoadNativeType(RuntimeLoader* loader, 
			const std::string& a, const std::string& n, std::size_t size)
		{
			std::string err;
			auto ret = loader->AddNativeType(a, n, size, size, err);
			Assert::IsNotNull(ret, ToString(err.c_str()).c_str());
			return ret;
		}

		static void CheckValueTypeBasic(RuntimeLoader* loader, RuntimeType* t)
		{
			Assert::AreEqual((std::size_t)loader, (std::size_t)t->Parent);
			Assert::AreEqual((int)TSM_VALUE, (int)t->Storage);
			Assert::IsNull(t->Initializer);
			Assert::IsNull(t->Finalizer);
			Assert::IsNull(t->StaticPointer);
		}

		static void CheckValueTypeSize(RuntimeType* t, std::size_t s, std::size_t a)
		{
			Assert::AreEqual(s, t->Size);
			Assert::AreEqual(a, t->Alignment);
		}

		static void CheckFieldOffset(RuntimeType* t, std::vector<std::size_t> v)
		{
			Assert::AreEqual(v.size(), t->Fields.size());
			for (std::size_t i = 0; i < v.size(); ++i)
			{
				Assert::AreEqual(v[i], t->Fields[i].Offset);
			}
		}

		static void CheckReferenceTypeBasic(RuntimeLoader* loader, RuntimeType* t)
		{
			Assert::AreEqual((std::size_t)loader, (std::size_t)t->Parent);
			Assert::AreEqual((int)TSM_REF, (int)t->Storage);
			Assert::AreEqual(sizeof(void*), t->GetStorageAlignment());
			Assert::AreEqual(sizeof(void*), t->GetStorageSize());
			Assert::IsNull(t->Initializer);
			Assert::IsNull(t->Finalizer);
			Assert::IsNull(t->StaticPointer);
		}

		static void CheckGlobalTypeBasic(RuntimeLoader* loader, RuntimeType* t)
		{
			Assert::AreEqual((std::size_t)loader, (std::size_t)t->Parent);
			Assert::AreEqual((int)TSM_GLOBAL, (int)t->Storage);
			Assert::IsNull(t->Initializer);
			Assert::IsNull(t->Finalizer);
			Assert::IsNotNull(t->StaticPointer);
			std::memset(t->StaticPointer, 0, t->Size);
		}

	private:
		static void SetupEmptyType(Builder& builder)
		{
			builder.BeginType(TSM_VALUE, "Test.SingleType");
			builder.SetTypeHandlers({}, {});
			builder.Link(true, false);
			builder.EndType();
		}

		static void CheckEmptyType(RuntimeLoader* loader)
		{
			auto t = LoadType(loader, "Test", "Test.SingleType", false);
			CheckValueTypeBasic(loader, t);
			CheckValueTypeSize(t, 1, 1);
			CheckFieldOffset(t, {});
		}

		static void SetupNativeType(Builder& builder, TypeReference* t1, TypeReference* t4)
		{
			*t1 = builder.BeginType(TSM_VALUE, "Test.Native1");
			builder.SetTypeHandlers({}, {});
			builder.Link(true, true);
			builder.EndType();

			*t4 = builder.BeginType(TSM_VALUE, "Test.Native4");
			builder.SetTypeHandlers({}, {});
			builder.Link(true, true);
			builder.EndType();
		}

		static void CheckNativeType(RuntimeLoader* loader)
		{
			auto t1 = LoadNativeType(loader, "Test", "Test.Native1", 1);
			CheckValueTypeBasic(loader, t1);
			CheckValueTypeSize(t1, 1, 1);
			CheckFieldOffset(t1, {});

			auto t4 = LoadNativeType(loader, "Test", "Test.Native4", 4);
			CheckValueTypeBasic(loader, t4);
			CheckValueTypeSize(t4, 4, 4);
			CheckFieldOffset(t4, {});
		}

		static void SetupValueType(Builder& builder, TypeReference t1, TypeReference t4)
		{
			auto a = builder.BeginType(TSM_VALUE, "Test.ValueTypeA");
			builder.SetTypeHandlers({}, {});
			builder.Link(false, false);
			builder.AddField(t1);
			builder.AddField(t1);
			builder.AddField(t4);
			builder.AddField(t4);
			builder.AddField(t1);
			builder.EndType();

			builder.BeginType(TSM_VALUE, "Test.ValueTypeB");
			builder.SetTypeHandlers({}, {});
			builder.Link(true, false);
			builder.AddField(a);
			builder.AddField(t1);
			builder.AddField(t4);
			builder.EndType();
		}

		static void CheckValueType(RuntimeLoader* loader)
		{
			auto b = LoadType(loader, "Test", "Test.ValueTypeB", false);
			auto a = b->Fields[0].Type;

			CheckValueTypeBasic(loader, a);
			CheckValueTypeSize(a, 13, 4);
			CheckFieldOffset(a, { 0, 1, 4, 8, 12 });

			CheckValueTypeBasic(loader, b);
			CheckValueTypeSize(b, 20, 4);
			CheckFieldOffset(b, { 0, 13, 16 });
		}

		static void SetupReferenceType(Builder& builder, TypeReference t1, TypeReference t4)
		{
			auto a = builder.BeginType(TSM_REF, "Test.RefTypeA");
			builder.SetTypeHandlers({}, {});
			builder.Link(false, false);
			builder.AddField(t1);
			builder.AddField(t4);
			builder.EndType();

			builder.BeginType(TSM_REF, "Test.RefTypeB");
			builder.SetTypeHandlers({}, {});
			builder.Link(true, false);
			builder.AddField(t4);
			builder.AddField(a);
			builder.AddField(t4);
			builder.EndType();
		}

		static void CheckReferenceType(RuntimeLoader* loader)
		{
			auto b = LoadType(loader, "Test", "Test.RefTypeB", false);
			auto a = b->Fields[1].Type;

			CheckReferenceTypeBasic(loader, a);
			CheckValueTypeSize(a, 8, 4);
			CheckFieldOffset(a, { 0, 4 });

			CheckReferenceTypeBasic(loader, b);
			CheckValueTypeSize(b, sizeof(void*) * 2 + 4, sizeof(void*));
			CheckFieldOffset(b, { 0, sizeof(void*), sizeof(void*) * 2 });
		}

		static void SetupGlobalType(Builder& builder, TypeReference t1, TypeReference t4)
		{
			auto a = builder.BeginType(TSM_VALUE, "Test.ValueTypeG1");
			builder.SetTypeHandlers({}, {});
			builder.Link(false, false);
			builder.AddField(t4);
			builder.AddField(t4);
			builder.EndType();

			builder.BeginType(TSM_GLOBAL, "Test.GlobalType");
			builder.SetTypeHandlers({}, {});
			builder.Link(true, false);
			builder.AddField(a);
			builder.AddField(t4);
			builder.EndType();
		}

		static void CheckGlobalType(RuntimeLoader* loader)
		{
			auto b = LoadType(loader, "Test", "Test.GlobalType", false);

			CheckGlobalTypeBasic(loader, b);
			CheckValueTypeSize(b, 12, 4);
			CheckFieldOffset(b, { 0, 8 });
		}

		static void CheckValueTypeFailExportName(RuntimeLoader* loader)
		{
			LoadType(loader, "Test", "Test.ValueTypeA", true);
			LoadType(loader, "Test", "Test.ValueTypeC", true);
			CheckValueType(loader);
		}

		static void SetupValueTypeFailTypeReference(Builder& builder)
		{
			TypeReference r1, r2;
			r1.Type = Builder::TR_TEMP;
			r1.Id = 0;
			r2.Type = Builder::TR_TEMP;
			r2.Id = 100;
			builder.BeginType(TSM_VALUE, "Test.ValueTypeC");
			builder.SetTypeHandlers({}, {});
			builder.Link(true, false);
			builder.AddField(r1);
			builder.AddField(r2);
			builder.EndType();
		}

		static void CheckValueTypeFailTypeReference(RuntimeLoader* loader)
		{
			LoadType(loader, "Test", "Test.ValueTypeC", true);
			CheckValueType(loader);
		}

		static void SetupTemplateType(Builder& builder, TypeReference t1, TypeReference t4)
		{
			auto tt = builder.BeginType(TSM_VALUE, "Test.TemplateType");
			auto g1 = builder.AddGenericParameter();
			auto g2 = builder.AddGenericParameter();
			builder.SetTypeHandlers({}, {});
			builder.Link(false, false);
			builder.AddField(g1);
			builder.AddField(g2);
			builder.EndType();

			auto tt11 = builder.MakeType(tt, { t1, t1 });
			auto tt12 = builder.MakeType(tt, { t1, t4 });

			builder.BeginType(TSM_VALUE, "Test.TemplateTestType1");
			builder.SetTypeHandlers({}, {});
			builder.Link(true, false);
			builder.AddField(tt11);
			builder.AddField(tt12);
			builder.EndType();

			builder.BeginType(TSM_VALUE, "Test.TemplateTestType2");
			auto g3 = builder.AddGenericParameter();
			auto tt2 = builder.MakeType(tt, { t4, g3 });
			builder.SetTypeHandlers({}, {});
			builder.Link(true, false);
			builder.AddField(tt2);
			builder.EndType();
		}

		static void CheckTemplateType(RuntimeLoader* loader)
		{
			auto t1 = LoadType(loader, "Test", "Test.TemplateTestType1", false);
			auto t11 = t1->Fields[0].Type;
			auto t12 = t1->Fields[1].Type;

			CheckValueTypeBasic(loader, t11);
			CheckValueTypeSize(t11, 2, 1);
			CheckFieldOffset(t11, { 0, 1 });

			CheckValueTypeBasic(loader, t12);
			CheckValueTypeSize(t12, 8, 4);
			CheckFieldOffset(t12, { 0, 4 });

			CheckValueTypeBasic(loader, t1);
			CheckValueTypeSize(t1, 12, 4);
			CheckFieldOffset(t1, { 0, 4 });

			auto t2 = LoadType(loader, "Test", "Test.TemplateTestType2",
				{ t11->Fields[0].Type }, false);
			auto t21 = t2->Fields[0].Type;

			CheckValueTypeBasic(loader, t21);
			CheckValueTypeSize(t21, 5, 4);
			CheckFieldOffset(t21, { 0, 4 });

			CheckValueTypeBasic(loader, t2);
			CheckValueTypeSize(t2, 5, 4);
			CheckFieldOffset(t2, { 0 });
		}

		static void SetupCyclicType(Builder& builder)
		{
			auto t1b = builder.ForwardDeclareType();
			auto t1a = builder.BeginType(TSM_VALUE, "Test.CycType1A");
			builder.SetTypeHandlers({}, {});
			builder.Link(true, false);
			builder.AddField(t1b);
			builder.EndType();

			builder.BeginType(TSM_VALUE, "Test.CycType1B", t1b);
			builder.SetTypeHandlers({}, {});
			builder.Link(false, false);
			builder.AddField(t1a);
			builder.EndType();

			auto t2b = builder.ForwardDeclareType();
			auto t2a = builder.BeginType(TSM_VALUE, "Test.CycType2A");
			builder.SetTypeHandlers({}, {});
			builder.Link(true, false);
			builder.AddField(t2b);
			builder.EndType();

			builder.BeginType(TSM_REF, "Test.CycType2B", t2b);
			builder.SetTypeHandlers({}, {});
			builder.Link(false, false);
			builder.AddField(t2a);
			builder.EndType();

			auto t3 = builder.BeginType(TSM_REF, "Test.CycType3A");
			builder.SetTypeHandlers({}, {});
			builder.Link(true, false);
			builder.AddField(t3);
			builder.EndType();
		}

		static void CheckCyclicType(RuntimeLoader* loader)
		{
			LoadType(loader, "Test", "Test.CycType1A", true);

			auto t2a = LoadType(loader, "Test", "Test.CycType2A", false);
			auto t2b = t2a->Fields[0].Type;
			CheckValueTypeBasic(loader, t2a);
			CheckValueTypeSize(t2a, sizeof(void*), sizeof(void*));
			CheckReferenceTypeBasic(loader, t2b);
			CheckValueTypeSize(t2b, t2a->Size, t2a->Alignment);

			auto t3 = LoadType(loader, "Test", "Test.CycType3A", false);
			CheckReferenceTypeBasic(loader, t3);
			CheckValueTypeSize(t3, sizeof(void*), sizeof(void*));
		}

	public:
		TEST_METHOD(LoadEmptyType)
		{
			Builder builder;

			builder.BeginAssembly("Test");
			SetupEmptyType(builder);
			builder.EndAssembly();
			
			RuntimeLoader loader(builder.Build());
			CheckEmptyType(&loader);
			CheckEmptyType(&loader);
		}

		TEST_METHOD(LoadValueType)
		{
			Builder builder;
			TypeReference t1, t4;

			builder.BeginAssembly("Test");
			SetupNativeType(builder, &t1, &t4);
			SetupValueType(builder, t1, t4);
			builder.EndAssembly();

			RuntimeLoader loader(builder.Build());
			CheckNativeType(&loader);
			CheckValueType(&loader);
			CheckNativeType(&loader);
			CheckValueType(&loader);
		}

		TEST_METHOD(LoadReferenceType)
		{
			Builder builder;
			TypeReference t1, t4;

			builder.BeginAssembly("Test");
			SetupNativeType(builder, &t1, &t4);
			SetupReferenceType(builder, t1, t4);
			builder.EndAssembly();

			RuntimeLoader loader(builder.Build());
			CheckNativeType(&loader);
			CheckReferenceType(&loader);
			CheckNativeType(&loader);
			CheckReferenceType(&loader);
		}

		TEST_METHOD(LoadGlobalType)
		{
			Builder builder;
			TypeReference t1, t4;

			builder.BeginAssembly("Test");
			SetupNativeType(builder, &t1, &t4);
			SetupGlobalType(builder, t1, t4);
			builder.EndAssembly();

			RuntimeLoader loader(builder.Build());
			CheckNativeType(&loader);
			CheckGlobalType(&loader);
			CheckNativeType(&loader);
			CheckGlobalType(&loader);
		}

		TEST_METHOD(LoadFailExportName)
		{
			Builder builder;
			TypeReference t1, t4;

			builder.BeginAssembly("Test");
			SetupNativeType(builder, &t1, &t4);
			SetupValueType(builder, t1, t4);
			builder.EndAssembly();

			RuntimeLoader loader(builder.Build());
			CheckNativeType(&loader);
			CheckValueTypeFailExportName(&loader);
		}

		TEST_METHOD(LoadFailTypeReference)
		{
			Builder builder;
			TypeReference t1, t4;

			builder.BeginAssembly("Test");
			SetupNativeType(builder, &t1, &t4);
			SetupValueType(builder, t1, t4);
			SetupValueTypeFailTypeReference(builder);
			builder.EndAssembly();

			RuntimeLoader loader(builder.Build());
			CheckNativeType(&loader);
			CheckValueTypeFailTypeReference(&loader);
		}

		TEST_METHOD(LoadTemplateType)
		{
			Builder builder;
			TypeReference t1, t4;

			builder.BeginAssembly("Test");
			SetupNativeType(builder, &t1, &t4);
			SetupTemplateType(builder, t1, t4);
			builder.EndAssembly();

			RuntimeLoader loader(builder.Build());
			CheckNativeType(&loader);
			CheckTemplateType(&loader);
		}

		TEST_METHOD(LoadCyclicReferencedType)
		{
			Builder builder;
			TypeReference t1, t4;

			builder.BeginAssembly("Test");
			SetupCyclicType(builder);
			builder.EndAssembly();

			RuntimeLoader loader(builder.Build());
			CheckCyclicType(&loader);
		}
	};
}
