#include "stdafx.h"

namespace LibRolLangTest
{
	using namespace RuntimeLoaderHelper;
	using Builder = TestAssemblyListBuilder;

	TEST_CLASS(GenericConstrainTest)
	{
		TEST_METHOD(SimpleTypeExist)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto te = b.BeginType(TSM_VALUE, "Core.ExistingType");
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.SuccessType");
				b.Link(true, false);
				b.AddConstrain(te, {}, CONSTRAIN_EXIST, 0);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.FailType");
				b.Link(true, false);
				b.AddConstrain(b.AnyType(), {}, CONSTRAIN_EXIST, 0);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.SuccessType", false);
				LoadType(&l, "Core", "Core.FailType", true);
			}
		}

		TEST_METHOD(DeductionTypeSame)
		{
			//class A<T> requires B<T,C<T>> == B<D,any>
			//success: A<D>, fail: A<E>
			Builder b;
			{
				b.BeginAssembly("Core");
				auto tb = b.BeginType(TSM_VALUE, "Core.B");
				b.AddGenericParameter();
				b.AddGenericParameter();
				b.EndType();
				auto tc = b.BeginType(TSM_VALUE, "Core.C");
				b.AddGenericParameter();
				b.EndType();
				auto td = b.BeginType(TSM_VALUE, "Core.D");
				b.Link(true, false);
				b.EndType();
				auto te = b.BeginType(TSM_VALUE, "Core.E");
				b.Link(true, false);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.A");
				b.Link(true, false);
				auto tg = b.AddGenericParameter();
				auto lhs = b.MakeType(tb, { tg, b.MakeType(tc, { tg }) });
				auto rhs = b.MakeType(tb, { td, b.AnyType() });
				b.AddConstrain(lhs, { rhs }, CONSTRAIN_SAME, 0); //TODO lhs, lhs
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				auto td = LoadType(&l, "Core", "Core.D", false);
				auto te = LoadType(&l, "Core", "Core.E", false);
				LoadType(&l, "Core", "Core.A", { td }, false);
				LoadType(&l, "Core", "Core.A", { te }, true);
			}
		}

		TEST_METHOD(SimpleInheritance)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto v1 = b.BeginType(TSM_VALUE, "Core.ValueType1");
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.ValueType2");
				b.SetBaseType(v1, {});
				b.Link(true, false);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.ValueType3");
				b.Link(true, false);
				b.EndType();

				auto vtab = b.BeginType(TSM_GLOBAL, "Core.VTabType");
				b.EndType();
				auto i1 = b.BeginType(TSM_INTERFACE, "Core.Interface1");
				b.SetBaseType({}, vtab);
				b.EndType();
				b.BeginType(TSM_INTERFACE, "Core.Interface2");
				b.SetBaseType({}, vtab);
				b.AddInterface(i1, {});
				b.Link(true, false);
				b.EndType();
				b.BeginType(TSM_INTERFACE, "Core.Interface3");
				b.SetBaseType({}, vtab);
				b.Link(true, false);
				b.EndType();

				b.BeginType(TSM_VALUE, "Core.TestType1");
				auto t1g = b.AddGenericParameter();
				b.AddConstrain(t1g, { v1 }, CONSTRAIN_BASE, 0);
				b.Link(true, false);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				auto t2g = b.AddGenericParameter();
				b.AddConstrain(t2g, { i1 }, CONSTRAIN_INTERFACE, 0);
				b.Link(true, false);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				auto v2 = LoadType(&l, "Core", "Core.ValueType2", false);
				auto v3 = LoadType(&l, "Core", "Core.ValueType3", false);
				auto i2 = LoadType(&l, "Core", "Core.Interface2", false);
				auto i3 = LoadType(&l, "Core", "Core.Interface3", false);

				LoadType(&l, "Core", "Core.TestType1", { v2 }, false);
				LoadType(&l, "Core", "Core.TestType1", { v3 }, true);
				LoadType(&l, "Core", "Core.TestType2", { i2 }, false);
				LoadType(&l, "Core", "Core.TestType2", { i3 }, true);
			}
		}

		TEST_METHOD(SimpleTrait)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto tv1 = b.BeginType(TSM_VALUE, "Core.ValueType1");
				b.Link(true, false);
				b.EndType();
				auto tv2 = b.BeginType(TSM_VALUE, "Core.ValueType2");
				b.Link(true, false);
				b.EndType();
				auto tt = b.BeginTrait("Core.Trait1");
				auto cg1 = b.SelfType();
				auto cg2 = b.AddGenericParameter();
				b.AddConstrain(cg1, { tv1 }, CONSTRAIN_SAME, 0);
				b.AddConstrain(cg2, { tv1 }, CONSTRAIN_SAME, 0);
				b.EndTrait();
				b.BeginType(TSM_VALUE, "Core.TestType");
				b.Link(true, false);
				auto tg1 = b.AddGenericParameter();
				auto tg2 = b.AddGenericParameter();
				b.AddConstrain(tg1, { tg2 }, CONSTRAIN_TRAIT_ASSEMBLY, tt.Id);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				auto tv1 = LoadType(&l, "Core", "Core.ValueType1", false);
				auto tv2 = LoadType(&l, "Core", "Core.ValueType2", false);
				LoadType(&l, "Core", "Core.TestType", { tv1, tv1 }, false);
				LoadType(&l, "Core", "Core.TestType", { tv2, tv1 }, true);
				LoadType(&l, "Core", "Core.TestType", { tv1, tv2 }, true);
				LoadType(&l, "Core", "Core.TestType", { tv2, tv2 }, true);
			}
		}
	};
}
