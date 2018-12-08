#include "stdafx.h"

namespace LibRolLangTest
{
	using namespace RuntimeLoaderHelper;
	using Builder = TestAssemblyListBuilder;
	
	TEST_CLASS(TraitFunctionTest)
	{
		TEST_METHOD(SingleReturnDetermined)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto vt1 = b.BeginType(TSM_VALUE, "Core.ValueType1");
				b.EndType();
				auto vt2 = b.BeginType(TSM_VALUE, "Core.ValueType2");
				b.EndType();
				auto f = b.BeginFunction("Core.Function");
				b.Signature(vt1, {});
				b.EndFunction();
				auto tt = b.BeginType(TSM_REFERENCE, "Core.TargetType");
				b.AddMemberFunction("F", f);
				b.EndType();

				auto tr1 = b.BeginTrait("Core.Trait1");
				b.AddTraitFunction(vt1, {}, "F", "F");
				b.EndTrait();
				auto tr2 = b.BeginTrait("Core.Trait2");
				b.AddTraitFunction(vt2, {}, "F", "F");
				b.EndTrait();

				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.Link(true, false);
				b.AddConstrain(tt, {}, CONSTRAIN_TRAIT_ASSEMBLY, tr1.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.Link(true, false);
				b.AddConstrain(tt, {}, CONSTRAIN_TRAIT_ASSEMBLY, tr2.Id);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType1", false);
				LoadType(&l, "Core", "Core.TestType2", true);
			}
		}

		TEST_METHOD(MatchParameterEqual)
		{
			//void F(vt1, any1, vt3<any2>) && any1 == any2
			//void F(vt1, vt1, vt3<vt1>) //success
			//void F(vt1, vt1, vt3<vt2>) //fail
			Builder b;
			{
				b.BeginAssembly("Core");
				auto vt1 = b.BeginType(TSM_VALUE, "Core.ValueType1");
				b.EndType();
				auto vt2 = b.BeginType(TSM_VALUE, "Core.ValueType2");
				b.EndType();
				auto vt3 = b.BeginType(TSM_VALUE, "Core.ValueType3");
				b.AddGenericParameter();
				b.EndType();

				auto tr = b.BeginTrait("Core.Trait");
				auto tg1 = b.AddGenericParameter();
				auto tg2 = b.AddGenericParameter();
				b.AddConstrain(tg1, { tg2 }, CONSTRAIN_SAME, 0);
				b.AddTraitFunction({}, { vt1, tg1, b.MakeType(vt3, { tg2 }) }, "F", "F");
				b.EndTrait();

				auto f1 = b.BeginFunction("Core.Function1");
				b.Signature({}, { vt1, vt1, b.MakeType(vt3, { vt1 }) });
				b.EndFunction();
				auto f2 = b.BeginFunction("Core.Function2");
				b.Signature({}, { vt1, vt1, b.MakeType(vt3, { vt2 }) });
				b.EndFunction();
				auto f3 = b.BeginFunction("Core.Function3");
				b.Signature({}, { vt1, vt1 });
				b.EndFunction();

				auto tt1 = b.BeginType(TSM_VALUE, "Core.TargetType1");
				b.AddMemberFunction("Func", f3);
				b.AddMemberFunction("F", f1);
				b.EndType();
				auto tt2 = b.BeginType(TSM_VALUE, "Core.TargetType2");
				b.AddMemberFunction("Func", f3);
				b.AddMemberFunction("F", f2);
				b.EndType();

				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.Link(true, false);
				b.AddConstrain(tt1, { b.AnyType(), b.AnyType() }, CONSTRAIN_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.Link(true, false);
				b.AddConstrain(tt2, { b.AnyType(), b.AnyType() }, CONSTRAIN_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType1", false);
				LoadType(&l, "Core", "Core.TestType2", true);
			}
		}
	};
}
