#include "stdafx.h"

namespace LibRolLangTest
{
	using namespace RuntimeLoaderHelper;
	using Builder = TestAssemblyListBuilder;

	TEST_CLASS(TraitGenericFunctionTest)
	{
		TEST_METHOD(SimpleNonGenericFunction)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto vt1 = b.BeginType(TSM_VALUE, "Core.ValueType1");
				b.EndType();
				auto vt2 = b.BeginType(TSM_VALUE, "Core.ValueType2");
				b.EndType();

				auto f1 = b.BeginFunction("Core.Func1");
				b.Signature({}, { vt1 });
				b.EndFunction();
				auto f2 = b.BeginFunction("Core.Func2");
				b.Signature({}, { vt2 });
				b.EndFunction();

				auto tt1 = b.BeginType(TSM_VALUE, "Core.TargetType1");
				b.AddMemberFunction("F", f1);
				b.EndType();
				auto tt2 = b.BeginType(TSM_VALUE, "Core.TargetType2");
				b.AddMemberFunction("F", f2);
				b.EndType();

				auto tr = b.BeginTrait("Core.TestTrait");
				b.AddTraitGenericFunction(f1, "F", "F");
				b.EndTrait();

				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.AddConstraint(tt1, {}, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.Link(true, false);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.AddConstraint(tt2, {}, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.Link(true, false);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType1", ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType2", ERR_L_GENERIC);
			}
		}

		TEST_METHOD(NonGenericFunctionOverload)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto vt1 = b.BeginType(TSM_VALUE, "Core.ValueType1");
				b.EndType();
				auto vt2 = b.BeginType(TSM_VALUE, "Core.ValueType2");
				b.EndType();

				auto f1 = b.BeginFunction("Core.Func1");
				b.Signature({}, { vt1 });
				b.EndFunction();
				auto f2 = b.BeginFunction("Core.Func2");
				b.Signature({}, { vt2 });
				b.EndFunction();
				auto f3 = b.BeginFunction("Core.Func3");
				b.Signature({}, { vt1, vt1 });
				b.EndFunction();
				auto f4 = b.BeginFunction("Core.Func4");
				b.Signature(vt1, { vt2 });
				b.EndFunction();

				auto tt1 = b.BeginType(TSM_VALUE, "Core.TargetType1");
				b.AddMemberFunction("F", f1);
				b.AddMemberFunction("F", f4);
				b.AddMemberFunction("F", f2);
				b.EndType();
				auto tt2 = b.BeginType(TSM_VALUE, "Core.TargetType2");
				b.AddMemberFunction("F", f1);
				b.AddMemberFunction("F", f2);
				b.AddMemberFunction("F", f3);
				b.EndType();

				auto tr = b.BeginTrait("Core.TestTrait");
				b.AddTraitGenericFunction(f4, "F", "F");
				b.EndTrait();

				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.AddConstraint(tt1, {}, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.Link(true, false);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.AddConstraint(tt2, {}, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.Link(true, false);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType1", ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType2", ERR_L_GENERIC);
			}
		}

		TEST_METHOD(SimpleGenericFunction)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto vt1 = b.BeginType(TSM_VALUE, "Core.ValueType1");
				b.EndType();
				auto vt2 = b.BeginType(TSM_VALUE, "Core.ValueType2");
				b.EndType();

				auto f1 = b.BeginFunction("Core.Func1");
				b.AddGenericParameter();
				b.Signature({}, { vt1 });
				b.EndFunction();

				auto tt1 = b.BeginType(TSM_VALUE, "Core.TargetType1");
				b.AddMemberFunction("F", b.MakeFunction(f1, { b.AddAdditionalGenericParameter(0) }));
				b.EndType();
				auto tt2 = b.BeginType(TSM_VALUE, "Core.TargetType2");
				b.AddMemberFunction("F", b.MakeFunction(f1, { b.AddGenericParameter() }));
				b.EndType();
				auto tt3 = b.BeginType(TSM_VALUE, "Core.TargetType3");
				b.AddMemberFunction("F", b.MakeFunction(f1, { b.AddAdditionalGenericParameter(1) }));
				b.EndType();

				auto tr = b.BeginTrait("Core.TestTrait");
				b.AddTraitGenericFunction(b.MakeFunction(f1, { b.AddAdditionalGenericParameter(0) }), "F", "F");
				b.EndTrait();

				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.AddConstraint(tt1, {}, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.Link(true, false);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.AddConstraint(tt2, {}, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.Link(true, false);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType3");
				b.AddConstraint(tt3, {}, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.Link(true, false);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType1", ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType2", ERR_L_GENERIC);
				LoadType(&l, "Core", "Core.TestType3", ERR_L_GENERIC);
			}
		}

		TEST_METHOD(GenericTypeArgument)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto vt1 = b.BeginType(TSM_VALUE, "Core.ValueType1");
				b.EndType();
				auto vt2 = b.BeginType(TSM_VALUE, "Core.ValueType2");
				b.EndType();
				auto pt1 = b.BeginType(TSM_VALUE, "Core.ParentType1");
				b.AddSubType("S", vt1);
				b.EndType();
				auto pt2 = b.BeginType(TSM_VALUE, "Core.ParentType2");
				b.AddSubType("S", vt1);
				b.EndType();

				auto f1 = b.BeginFunction("Core.Func1");
				auto f1g1 = b.AddGenericParameter();
				auto f1g2 = b.AddGenericParameter();
				b.Signature(b.MakeSubtype(f1g1, "S", {}), { f1g2 });
				b.EndFunction();
				auto tt1 = b.BeginType(TSM_VALUE, "Core.TargetType1");
				auto tt1g1 = b.AddGenericParameter();
				auto tt1fg1 = b.AddAdditionalGenericParameter(0);
				b.AddMemberFunction("F", b.MakeFunction(f1, { tt1g1, tt1fg1 }));
				b.EndType();

				auto f2 = b.BeginFunction("Core.Func2");
				auto f2g1 = b.AddGenericParameter();
				auto f2g2 = b.AddGenericParameter();
				b.Signature(f2g1, { f1g2 });
				b.EndFunction();
				auto tt2 = b.BeginType(TSM_VALUE, "Core.TargetType2");
				auto tt2g1 = b.AddGenericParameter();
				auto tt2fg1 = b.AddAdditionalGenericParameter(0);
				b.AddMemberFunction("F", b.MakeFunction(f2, { b.MakeSubtype(tt2g1, "S", {}), tt2fg1 }));
				b.EndType();

				auto f3 = b.BeginFunction("Core.Func3");
				auto f3g1 = b.AddGenericParameter();
				auto f3g2 = b.AddGenericParameter();
				auto f3g3 = b.AddGenericParameter();
				b.Signature(b.MakeSubtype(f3g2, "S", {}), { f3g3 });
				b.EndFunction();
				auto tr = b.BeginTrait("Core.TestTrait");
				auto trg1 = b.AddGenericParameter();
				auto trg2 = b.AddGenericParameter();
				auto trfg1 = b.AddAdditionalGenericParameter(0);
				b.AddTraitGenericFunction(b.MakeFunction(f3, { trg1, trg2, trfg1 }), "F", "F");
				b.EndTrait();

				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.AddConstraint(b.MakeType(tt1, { pt1 }), { vt2, pt2 }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.Link(true, false);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.AddConstraint(b.MakeType(tt2, { pt1 }), { vt2, pt2 }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.Link(true, false);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType1", ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType2", ERR_L_SUCCESS);
			}
			//class A<T1> { T1.S F<T>(T); }
			//trait Tr<T1, T2> { T2.S F<T>(T); }
		}

		TEST_METHOD(TraitTypeDeduction)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto vt = b.BeginType(TSM_VALUE, "Core.ValueType");
				b.Link(true, false);
				b.EndType();
				auto f = b.BeginFunction("Core.TestFunction");
				auto fg = b.AddGenericParameter();
				b.Signature(vt, {});
				b.EndFunction();

				auto tt = b.BeginType(TSM_VALUE, "Core.TargetType");
				b.AddMemberFunction("Func", b.MakeFunction(f, { b.AddAdditionalGenericParameter(0) }));
				b.EndType();

				auto trf = b.BeginFunction("Core.TraitFunction");
				auto trf1 = b.AddGenericParameter();
				auto trf2 = b.AddGenericParameter();
				b.Signature(trf1, {});
				b.EndFunction();

				auto tr = b.BeginTrait("Core.TestTrait");
				auto trg = b.AddGenericParameter();
				auto trfg = b.AddAdditionalGenericParameter(0);
				b.AddTraitGenericFunction(b.MakeFunction(trf, { trg, trfg }), "Func", "Func");
				b.AddTraitType(trg, "T1");
				b.EndTrait();

				b.BeginType(TSM_VALUE, "Core.TestType");
				b.Link(true, false);
				b.AddConstraint(tt, { b.AnyType() }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id, "Tr");
				auto ft = b.ConstraintImportType("Tr/T1");
				b.AddField(ft, "F");
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				auto t = LoadType(&l, "Core", "Core.TestType", ERR_L_SUCCESS);
				auto vt = LoadType(&l, "Core", "Core.ValueType", ERR_L_SUCCESS);
				Assert::IsTrue(t->Fields[0].Type == vt);
			}
			//class A { X Func<T>(); }
			//trait Tr<T1> { T1 Func<T>(); export T1 as T1; }
			//class B requires A : Tr<any> as Tr { Tr.T1 F; }
		}

		TEST_METHOD(GenericFunctionWithConstraint)
		{
			Builder b;
			{
				b.BeginAssembly("Test");
				b.BeginTrait("Test.TestTrait");
				b.Link(true, false);
				b.AddTraitField(b.AddGenericParameter(), "F", "F");
				b.EndTrait();
				b.EndAssembly();

				b.BeginAssembly("Core");
				auto tr1 = b.ImportTrait("Test", "Test.TestTrait");

				auto tr2 = b.BeginTrait("Core.TestTrait");
				b.AddTraitField(b.AddGenericParameter(), "F", "F");
				b.EndTrait();

				auto vt = b.BeginType(TSM_VALUE, "Core.ValueType");
				b.EndType();

				auto f1a = b.BeginFunction("Core.F1A");
				b.AddConstraint(b.AddGenericParameter(), { b.AnyType() }, CONSTRAINT_TRAIT_ASSEMBLY, tr1.Id);
				b.Signature({}, {});
				b.EndFunction();
				auto f1b = b.BeginFunction("Core.F1B");
				b.AddConstraint(b.AddGenericParameter(), { b.AnyType() }, CONSTRAINT_TRAIT_ASSEMBLY, tr2.Id);
				b.Signature({}, {});
				b.EndFunction();
				auto ttr1 = b.BeginTrait("Core.TargetTrait1");
				b.AddTraitGenericFunction(b.MakeFunction(f1a, { b.AddAdditionalGenericParameter(0) }), "Func", "Func");
				b.EndTrait();
				auto tt1 = b.BeginType(TSM_VALUE, "Core.TargetType1");
				b.AddMemberFunction("Func", b.MakeFunction(f1b, { b.AddAdditionalGenericParameter(0) }));
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.Link(true, false);
				b.AddConstraint(tt1, {}, CONSTRAINT_TRAIT_ASSEMBLY, ttr1.Id);
				b.EndType();

				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType1", ERR_L_SUCCESS);
			}
		}
	};
}
