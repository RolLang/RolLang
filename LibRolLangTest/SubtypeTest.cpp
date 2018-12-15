#include "stdafx.h"

namespace LibRolLangTest
{
	using namespace RuntimeLoaderHelper;
	using Builder = TestAssemblyListBuilder;

	TEST_CLASS(SubtypeTest)
	{
		TEST_METHOD(LoadSimpleSubtype)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto n = b.BeginType(TSM_VALUE, "Core.NativeType");
				b.Link(false, true);
				b.EndType();
				auto p = b.BeginType(TSM_VALUE, "Core.ParentType");
				b.AddSubType("MyNativeType", n);
				b.EndType();
				auto t = b.BeginType(TSM_VALUE, "Core.TestType");
				auto subtype = b.MakeSubtype(p, "MyNativeType", {});
				b.Link(true, false);
				b.AddField(subtype);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				auto n = LoadNativeType(&l, "Core", "Core.NativeType", 4);
				auto t = LoadType(&l, "Core", "Core.TestType", ERR_L_SUCCESS);
				CheckValueTypeBasic(&l, t);
				CheckValueTypeSize(t, 4, 4);
				CheckFieldOffset(t, { 0 });
				Assert::AreEqual((std::uintptr_t)n, (std::uintptr_t)t->Fields[0].Type);
			}
		}

		TEST_METHOD(LoadGenericSubtype)
		{
			/*
			 * class A<T> { alias S<T1> = B<T, T1, native4>; }
			 * class B<T1, T2, T3> { T1 f1; T2 f2; T3 f3; }
			 * class C { A<native1>.S<native2> f1; }
			 */
			Builder b;
			{
				b.BeginAssembly("Core");
				auto tn1 = b.BeginType(TSM_VALUE, "Core.Native1");
				b.Link(true, true);
				b.EndType();
				auto tn2 = b.BeginType(TSM_VALUE, "Core.Native2");
				b.Link(true, true);
				b.EndType();
				auto tn4 = b.BeginType(TSM_VALUE, "Core.Native4");
				b.Link(true, true);
				b.EndType();
				auto tb = b.BeginType(TSM_VALUE, "Core.B");
				{
					auto g1 = b.AddGenericParameter();
					auto g2 = b.AddGenericParameter();
					auto g3 = b.AddGenericParameter();
					b.AddField(g1);
					b.AddField(g2);
					b.AddField(g3);
				}
				b.EndType();
				auto ta = b.BeginType(TSM_VALUE, "Core.A");
				{
					auto g1 = b.AddGenericParameter();
					auto ga1 = b.AddAdditionalGenericParameter(0);
					auto s = b.MakeType(tb, { g1, ga1, tn4 });
					b.AddSubType("S", s);
				}
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.C");
				b.Link(true, false);
				auto f = b.MakeSubtype(b.MakeType(ta, { tn1 }), "S", { tn2 });
				b.AddField(f);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadNativeType(&l, "Core", "Core.Native1", 1);
				LoadNativeType(&l, "Core", "Core.Native2", 2);
				LoadNativeType(&l, "Core", "Core.Native4", 4);
				auto tc = LoadType(&l, "Core", "Core.C", ERR_L_SUCCESS);
				auto t = tc->Fields[0].Type;
				CheckValueTypeBasic(&l, t);
				CheckValueTypeSize(t, 8, 4);
				CheckFieldOffset(t, { 0, 2, 4 });
			}
		}
	};
}
