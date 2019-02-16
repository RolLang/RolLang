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
			//class A<T1> { T1.Sub F<T>(); }
			//trait Tr<T1> { T1.Sub F<T>(); }
		}

		TEST_METHOD(TraitTypeDeduction)
		{
			//class A { X Func<T>(); Y Field; }
			//trait Tr<T1> { T1 Func<T>(); T1.Sub Field; }
			//A : Tr<any>
		}

		TEST_METHOD(GenericFunctionWithConstraint)
		{
			//test assembly trait and imported trait
		}
	};
}
