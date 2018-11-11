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

		TEST_METHOD(SimpleTypeSame)
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
	};
}
