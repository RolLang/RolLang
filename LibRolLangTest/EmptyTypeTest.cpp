#include "stdafx.h"

namespace LibRolLangTest
{
	using namespace RuntimeLoaderHelper;
	using Builder = TestAssemblyListBuilder;

	TEST_CLASS(EmptyTypeTest)
	{
		TEST_METHOD(GenericTypeArgument)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				b.BeginType(TSM_VALUE, "Core.ValueType");
				b.Link(true, false);
				b.EndType();
				auto t1 = b.BeginType(TSM_VALUE, "Core.TestType1");
				b.Link(true, false);
				b.AddGenericParameter();
				auto t1g = b.AddGenericParameter();
				b.AddField(t1g);
				b.EndType();
				auto t2 = b.BeginType(TSM_VALUE, "Core.TargetType1");
				b.AddGenericParameter();
				b.EndType();
				auto t3 = b.BeginType(TSM_VALUE, "Core.TestType2");
				b.Link(true, false);
				b.AddField(b.MakeType(t2, { {} }));
				b.EndType();
				auto t4 = b.BeginType(TSM_VALUE, "Core.TestType3");
				b.Link(true, false);
				auto t4g = b.AddGenericParameter();
				b.AddField(t4g);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				auto vt = LoadType(&l, "Core", "Core.ValueType", ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType1", { vt, vt }, ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType1", { nullptr, vt }, ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType1", { vt, nullptr }, ERR_L_PROGRAM);
				LoadType(&l, "Core", "Core.TestType2", ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType3", { vt }, ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType3", { nullptr }, ERR_L_PROGRAM);
			}
		}

		TEST_METHOD(GenericFunctionArgument)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				b.BeginType(TSM_VALUE, "Core.ValueType");
				b.Link(true, false);
				b.EndType();
				b.BeginFunction("Core.Function1");
				b.Link(true, false);
				auto f1g = b.AddGenericParameter();
				b.Signature(f1g, {});
				b.EndFunction();
				b.BeginFunction("Core.Function2");
				b.Link(true, false);
				auto f2g = b.AddGenericParameter();
				b.Signature({}, { f2g });
				b.EndFunction();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				auto vt = LoadType(&l, "Core", "Core.ValueType", ERR_L_SUCCESS);
				LoadFunction(&l, "Core", "Core.Function1", { vt }, ERR_L_SUCCESS);
				LoadFunction(&l, "Core", "Core.Function1", { nullptr }, ERR_L_SUCCESS);
				LoadFunction(&l, "Core", "Core.Function2", { vt }, ERR_L_SUCCESS);
				LoadFunction(&l, "Core", "Core.Function2", { nullptr }, ERR_L_PROGRAM);
			}
		}

		TEST_METHOD(DeductFunctionReturn)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto tr = b.BeginTrait("Core.TestTrait");
				auto trg = b.AddGenericParameter();
				b.AddTraitFunction(trg, {}, "Func", "Func");
				b.AddTraitType(trg, "ret");
				b.EndTrait();
				
				auto vt = b.BeginType(TSM_VALUE, "Core.ValueType");
				b.Link(true, false);
				b.EndType();
				auto tf = b.BeginFunction("Core.TargetFunction1");
				auto tfg = b.AddGenericParameter();
				b.Signature(tfg, {});
				b.EndFunction();
				auto tt1 = b.BeginType(TSM_VALUE, "Core.TargetType1");
				b.AddMemberFunction("Func", b.MakeFunction(tf, { {} }));
				b.EndType();
				auto tt2 = b.BeginType(TSM_VALUE, "Core.TargetType2");
				b.AddMemberFunction("Func", b.MakeFunction(tf, { vt }));
				b.EndType();

				auto gt = b.BeginType(TSM_VALUE, "Core.GenericType");
				b.Link(true, false);
				b.AddGenericParameter();
				b.EndType();

				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.Link(true, false);
				b.AddConstraint(tt1, { b.AnyType() }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id, "constraint");
				b.AddField(b.MakeType(gt, { b.ConstraintImportType("constraint/ret") }));
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.Link(true, false);
				b.AddConstraint(tt2, { b.AnyType() }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id, "constraint");
				b.AddField(b.MakeType(gt, { b.ConstraintImportType("constraint/ret") }));
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				auto t1 = LoadType(&l, "Core", "Core.TestType1", ERR_L_SUCCESS);
				auto t2 = LoadType(&l, "Core", "Core.TestType2", ERR_L_SUCCESS);
				CheckFieldOffset(t1, { 0 });
				CheckFieldOffset(t2, { 0 });
				auto ft1 = t1->Fields[0].Type;
				auto ft2 = t2->Fields[0].Type;
				Assert::IsTrue(ft2 != ft1);
				auto gt1 = LoadType(&l, "Core", "Core.GenericType", { nullptr }, ERR_L_SUCCESS);
				auto vt = LoadType(&l, "Core", "Core.ValueType", ERR_L_SUCCESS);
				auto gt2 = LoadType(&l, "Core", "Core.GenericType", { vt }, ERR_L_SUCCESS);
				Assert::IsTrue(gt1 == ft1);
				Assert::IsTrue(gt2 == ft2);
			}
		}

		TEST_METHOD(DeductGenericArg)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto gt = b.BeginType(TSM_VALUE, "Core.GenericType");
				b.AddGenericParameter();
				b.EndType();
				auto vt = b.BeginType(TSM_VALUE, "Core.ValueType");
				b.EndType();

				auto tr = b.BeginTrait("Core.Trait");
				auto trg = b.AddGenericParameter();
				b.AddTraitFunction(trg, { b.MakeType(gt, { trg }) }, "Func", "Func");
				b.EndTrait();

				auto f1 = b.BeginFunction("Core.TargetFunction1");
				b.Signature({}, { b.MakeType(gt, { {} }) });
				b.EndFunction();
				auto f2 = b.BeginFunction("Core.TargetFunction2");
				b.Signature(vt, { b.MakeType(gt, { vt }) });
				b.EndFunction();
				auto f3 = b.BeginFunction("Core.TargetFunction3");
				b.Signature({}, { b.MakeType(gt, { vt }) });
				b.EndFunction();
				auto f4 = b.BeginFunction("Core.TargetFunction4");
				b.Signature(vt, { b.MakeType(gt, { {} }) });
				b.EndFunction();

				auto tt1 = b.BeginType(TSM_VALUE, "Core.TargetType1");
				b.AddMemberFunction("Func", f1);
				b.EndType();
				auto tt2 = b.BeginType(TSM_VALUE, "Core.TargetType2");
				b.AddMemberFunction("Func", f2);
				b.EndType();
				auto tt3 = b.BeginType(TSM_VALUE, "Core.TargetType3");
				b.AddMemberFunction("Func", f3);
				b.EndType();
				auto tt4 = b.BeginType(TSM_VALUE, "Core.TargetType4");
				b.AddMemberFunction("Func", f4);
				b.EndType();

				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.Link(true, false);
				b.AddConstraint(tt1, { b.AnyType() }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.Link(true, false);
				b.AddConstraint(tt2, { b.AnyType() }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType3");
				b.Link(true, false);
				b.AddConstraint(tt3, { b.AnyType() }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType4");
				b.Link(true, false);
				b.AddConstraint(tt4, { b.AnyType() }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType1", ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType2", ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType3", ERR_L_GENERIC);
				LoadType(&l, "Core", "Core.TestType4", ERR_L_GENERIC);
			}
		}

		TEST_METHOD(TraitTarget)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto tr = b.BeginTrait("Core.Trait");
				b.EndTrait();
				b.BeginType(TSM_VALUE, "Core.TestType");
				b.Link(true, false);
				b.AddConstraint({}, {}, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType", ERR_L_GENERIC);
			}
		}

		TEST_METHOD(ConstraintArgument)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto gt = b.BeginType(TSM_VALUE, "Core.GenericType");
				b.AddGenericParameter();
				b.EndType();
				auto vt = b.BeginType(TSM_VALUE, "Core.ValueType");
				b.EndType();
				
				auto tr = b.BeginTrait("Core.Trait");
				auto trg = b.AddGenericParameter();
				b.AddConstraint(b.MakeType(gt, { trg }), { b.MakeType(gt, { {} }) }, CONSTRAINT_SAME, 0);
				b.EndTrait();

				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.Link(true, false);
				b.AddConstraint(vt, { {} }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.Link(true, false);
				b.AddConstraint(vt, { vt }, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType1", ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType2", ERR_L_GENERIC);
			}
		}

		TEST_METHOD(EmptySubtype)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto vt = b.BeginType(TSM_VALUE, "Core.ValueType");
				b.EndType();

				auto tf1 = b.BeginFunction("Core.TargetFunction");
				b.Signature({}, {});
				b.EndFunction();
				auto tf2 = b.BeginFunction("Core.TargetFunction");
				b.Signature(vt, {});
				b.EndFunction();

				auto tt1 = b.BeginType(TSM_VALUE, "Core.TargetType1");
				b.AddSubType("RetType", {});
				b.AddMemberFunction("Func", tf1);
				b.EndType();
				auto tt2 = b.BeginType(TSM_VALUE, "Core.TargetType2");
				b.AddSubType("RetType", vt);
				b.AddMemberFunction("Func", tf2);
				b.EndType();
				auto tt3 = b.BeginType(TSM_VALUE, "Core.TargetType3");
				b.AddSubType("RetType", vt);
				b.AddMemberFunction("Func", tf1);
				b.EndType();
				
				auto tr = b.BeginTrait("Core.Trait");
				auto trret = b.MakeSubtype(b.SelfType(), "RetType", {});
				b.AddTraitFunction(trret, {}, "Func", "Func");
				b.EndTrait();

				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.Link(true, false);
				b.AddConstraint(tt1, {}, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.Link(true, false);
				b.AddConstraint(tt2, {}, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType3");
				b.Link(true, false);
				b.AddConstraint(tt3, {}, CONSTRAINT_TRAIT_ASSEMBLY, tr.Id);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType1", ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType2", ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType3", ERR_L_GENERIC);
			}
		}

		TEST_METHOD(SubtypeParent)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				b.BeginType(TSM_VALUE, "Core.TestType");
				b.Link(true, false);
				auto type = b.MakeSubtype({}, "Type", {});
				b.AddConstraint(b.TryType(type), {}, CONSTRAINT_EXIST, 0);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType", ERR_L_GENERIC);
			}
		}

		TEST_METHOD(BuiltinConstraint)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto vt = b.BeginType(TSM_VALUE, "Core.ValueType");
				b.EndType();

				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.Link(true, false);
				b.AddConstraint({}, { {} }, CONSTRAINT_SAME, 0);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.Link(true, false);
				b.AddConstraint({}, {}, CONSTRAINT_EXIST, 0);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType3");
				b.Link(true, false);
				b.AddConstraint({}, { vt }, CONSTRAINT_BASE, 0);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType4");
				b.Link(true, false);
				b.AddConstraint(vt, { {} }, CONSTRAINT_BASE, 0);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType5");
				b.Link(true, false);
				b.AddConstraint({}, { vt }, CONSTRAINT_INTERFACE, 0);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType6");
				b.Link(true, false);
				b.AddConstraint(vt, { {} }, CONSTRAINT_INTERFACE, 0);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType1", ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType2", ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.TestType3", ERR_L_GENERIC);
				LoadType(&l, "Core", "Core.TestType4", ERR_L_GENERIC);
				LoadType(&l, "Core", "Core.TestType5", ERR_L_GENERIC);
				LoadType(&l, "Core", "Core.TestType6", ERR_L_GENERIC);
			}
		}
	};
}
