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
				b.AddConstraint(tt, {}, CONSTRAINT_TRAIT_ASSEMBLY, tr1.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.Link(true, false);
				b.AddConstraint(tt, {}, CONSTRAINT_TRAIT_ASSEMBLY, tr2.Id);
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
				b.AddConstraint(tg1, { tg2 }, CONSTRAINT_SAME, 0);
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
				b.AddConstraint(tt1, { b.AnyType(), b.AnyType() }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.Link(true, false);
				b.AddConstraint(tt2, { b.AnyType(), b.AnyType() }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType1", false);
				LoadType(&l, "Core", "Core.TestType2", true);
			}
		}

		TEST_METHOD(GenericMemberFunction)
		{
			//class A<T> { void F<T1>(T, t3<T1>); }
			//A<t1>.F(t1, t3<t2>) //success
			//A<t1>.F(t2, t3<t2>) //fail
			//A<t1>.F(t1, t2) //fail
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
				
				auto f = b.BeginFunction("Core.Function");
				auto fg1 = b.AddGenericParameter();
				auto fg2 = b.AddGenericParameter();
				b.Signature({}, { fg1, b.MakeType(vt3, { fg2 }) });
				b.EndFunction();

				auto tr = b.BeginTrait("Core.Trait");
				auto trg1 = b.AddGenericParameter();
				auto trg2 = b.AddGenericParameter();
				b.AddTraitFunction({}, { trg1, trg2 }, "F", "F");
				b.EndTrait();

				auto tt = b.BeginType(TSM_VALUE, "Core.TargetType");
				auto tg1 = b.AddGenericParameter();
				b.AddMemberFunction("F", b.MakeFunction(f, { tg1, b.AddAdditionalGenericParameter(0) }));
				b.EndType();

				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.Link(true, false);
				b.AddConstraint(b.MakeType(tt, { vt1 }), 
					{ vt1, b.MakeType(vt3, { vt2 }) }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.Link(true, false);
				b.AddConstraint(b.MakeType(tt, { vt1 }),
					{ vt2, b.MakeType(vt3, { vt2 }) }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType3");
				b.Link(true, false);
				b.AddConstraint(b.MakeType(tt, { vt1 }),
					{ vt1, vt2 }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType1", false);
				LoadType(&l, "Core", "Core.TestType2", true);
				LoadType(&l, "Core", "Core.TestType3", true);
			}
		}

		TEST_METHOD(MultipleFunctions)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto tint = b.BeginType(TSM_VALUE, "Core.Int");
				b.EndType();
				auto tele = b.BeginType(TSM_VALUE, "Core.Element");
				b.EndType();
				auto tlist = b.ForwardDeclareType();
				Builder::FunctionReference f_add, f_remove, f_get, f_set;
				{
					f_add = b.BeginFunction("Core.List<1>.Add");
					auto g = b.AddGenericParameter();
					b.Signature({}, { b.MakeType(tlist, { g }), g });
					b.EndFunction();
				}
				{
					f_remove = b.BeginFunction("Core.List<1>.Remove");
					auto g = b.AddGenericParameter();
					b.Signature({}, { b.MakeType(tlist, { g }), tint });
					b.EndFunction();
				}
				{
					f_get = b.BeginFunction("Core.List<1>.Get");
					auto g = b.AddGenericParameter();
					b.Signature(g, { b.MakeType(tlist, { g }), tint });
					b.EndFunction();
				}
				{
					f_set = b.BeginFunction("Core.List<1>.Set");
					auto g = b.AddGenericParameter();
					b.Signature({}, { b.MakeType(tlist, { g }), tint, g });
					b.EndFunction();
				}
				b.BeginType(TSM_REFERENCE, "Core.List", tlist);
				auto tg = b.AddGenericParameter();
				b.AddMemberFunction("Add", b.MakeFunction(f_add, { tg }));
				b.AddMemberFunction("Remove", b.MakeFunction(f_remove, { tg }));
				b.AddMemberFunction("Get", b.MakeFunction(f_get, { tg }));
				b.AddMemberFunction("Set", b.MakeFunction(f_set, { tg }));
				b.EndType();
				auto tr = b.BeginTrait("Core.Trait");
				auto trg = b.AddGenericParameter();
				b.AddTraitFunction({}, { b.SelfType(), trg }, "Add", "Add");
				b.AddTraitFunction({}, { b.SelfType(), tint }, "Remove", "Remove");
				b.AddTraitFunction(trg, { b.SelfType(), tint }, "Get", "Get");
				b.AddTraitFunction({}, { b.SelfType(), tint, trg }, "Set", "Set");
				b.EndTrait();

				b.BeginType(TSM_VALUE, "Core.TestType");
				b.Link(true, false);
				b.AddConstraint(b.MakeType(tlist, { tele }), { b.AnyType() }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType", false);
			}
		}
	};
}
