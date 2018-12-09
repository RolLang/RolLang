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
				b.SetBaseType(v1, {}, {});
				b.Link(true, false);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.ValueType3");
				b.Link(true, false);
				b.EndType();

				auto i1 = b.BeginType(TSM_INTERFACE, "Core.Interface1");
				b.SetBaseType({}, {}, {});
				b.EndType();
				b.BeginType(TSM_INTERFACE, "Core.Interface2");
				b.SetBaseType({}, {}, {});
				b.AddInterface(i1, {}, {});
				b.Link(true, false);
				b.EndType();
				b.BeginType(TSM_INTERFACE, "Core.Interface3");
				b.SetBaseType({}, {}, {});
				b.Link(true, false);
				b.EndType();

				auto r1 = b.BeginType(TSM_REFERENCE, "Core.RefType1");
				b.Link(true, false);
				b.EndType();
				b.BeginType(TSM_REFERENCE, "Core.RefType2");
				b.Link(true, false);
				b.SetBaseType(r1, {}, {});
				b.AddInterface(i1, {}, {});
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
				b.BeginType(TSM_VALUE, "Core.TestType3");
				auto t3g = b.AddGenericParameter();
				b.AddConstrain(t3g, { r1 }, CONSTRAIN_BASE, 0);
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
				auto r1 = LoadType(&l, "Core", "Core.RefType1", false);
				auto r2 = LoadType(&l, "Core", "Core.RefType2", false);

				LoadType(&l, "Core", "Core.TestType1", { v2 }, false);
				LoadType(&l, "Core", "Core.TestType1", { v3 }, true);
				LoadType(&l, "Core", "Core.TestType2", { i2 }, false);
				LoadType(&l, "Core", "Core.TestType2", { i3 }, true);
				LoadType(&l, "Core", "Core.TestType2", { r1 }, true);
				LoadType(&l, "Core", "Core.TestType2", { r2 }, false);
				LoadType(&l, "Core", "Core.TestType3", { r2 }, false);
			}
		}

		TEST_METHOD(NestedLoadedInheritance)
		{
			//The difference of this test is that the referenced type is not loaded
			//until used in the loading process (by a field). In the SimpleInheritance,
			//the type has been fully loaded so base/interfaces are already there.
			Builder b;
			{
				b.BeginAssembly("Core");
				auto r1 = b.BeginType(TSM_REFERENCE, "Core.RefType1");
				b.EndType();
				auto r2 = b.BeginType(TSM_REFERENCE, "Core.RefType2");
				b.SetBaseType(r1, {}, {});
				b.EndType();
				auto ct = b.BeginType(TSM_VALUE, "Core.ConstrainedType");
				auto g = b.AddGenericParameter();
				b.AddConstrain(g, { r1 }, CONSTRAIN_BASE, 0);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType");
				b.Link(true, false);
				b.AddField(b.MakeType(ct, { r2 }));
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadType(&l, "Core", "Core.TestType", false);
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

		TEST_METHOD(TryTypeExist)
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
				auto ct = b.BeginType(TSM_VALUE, "Core.ConstrainType");
				auto cg = b.AddGenericParameter();
				b.AddConstrain(cg, { tv1 }, CONSTRAIN_SAME, 0);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType");
				b.Link(true, false);
				auto tg = b.AddGenericParameter();
				b.AddConstrain(b.TryType(b.MakeType(ct, { tg })), {}, CONSTRAIN_EXIST, 0);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				auto tv1 = LoadType(&l, "Core", "Core.ValueType1", false);
				auto tv2 = LoadType(&l, "Core", "Core.ValueType2", false);
				LoadType(&l, "Core", "Core.TestType", { tv1 }, false);
				LoadType(&l, "Core", "Core.TestType", { tv2 }, true);
				//Needs manually step-in to check where it throws.
				//Should be in loading Core.TestType instead of Core.ConstrainType.
				//TODO use exception tracing, loader constrain API or type overload when any is available.
			}
		}

		TEST_METHOD(TrySubTypeExist)
		{
			//class ParentType { alias S1<T> = GenericType<T>; } //no S2
			//class CheckType1<T> requires ParentType::S1<T>;
			//class CheckType2<T> requires ParentType::S2<T>;
			//success: CheckType1<BasicType1>
			//fail: CheckType1<BasicType2>
			//fail: CheckType2<BasicType1>
			Builder b;
			{
				b.BeginAssembly("Core");
				auto t1 = b.BeginType(TSM_VALUE, "Core.BasicType1");
				b.Link(true, false);
				b.EndType();
				auto t2 = b.BeginType(TSM_VALUE, "Core.BasicType2");
				b.Link(true, false);
				b.EndType();
				auto tg = b.BeginType(TSM_VALUE, "Core.GenericType");
				auto g1 = b.AddGenericParameter();
				b.AddConstrain(g1, { t1 }, CONSTRAIN_SAME, 0);
				b.EndType();
				auto tp = b.BeginType(TSM_VALUE, "Core.ParentType");
				b.AddSubType("S1", b.MakeType(tg, { b.AddAdditionalGenericParameter(0) }));
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.CheckType1");
				b.Link(true, false);
				auto g2 = b.AddGenericParameter();
				b.AddConstrain(b.TryType(b.MakeSubtype(tp, "S1", { g2 })), {}, CONSTRAIN_EXIST, 0);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.CheckType2");
				b.Link(true, false);
				auto g3 = b.AddGenericParameter();
				b.AddConstrain(b.TryType(b.MakeSubtype(tp, "S2", { g3 })), {}, CONSTRAIN_EXIST, 0);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				auto t1 = LoadType(&l, "Core", "Core.BasicType1", false);
				auto t2 = LoadType(&l, "Core", "Core.BasicType2", false);
				LoadType(&l, "Core", "Core.CheckType1", { t1 }, false);
				LoadType(&l, "Core", "Core.CheckType1", { t2 }, true);
				LoadType(&l, "Core", "Core.CheckType2", { t1 }, true);
			}
		}

		TEST_METHOD(CircularConstrain_TypeExist)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				b.BeginType(TSM_VALUE, "Core.BasicType");
				b.Link(true, false);
				b.EndType();
				auto t1 = b.ForwardDeclareType();
				auto trait = b.BeginTrait("TestTrait");
				b.AddConstrain(b.MakeType(t1, { b.SelfType() }), {}, CONSTRAIN_EXIST, 0);
				b.EndTrait();
				b.BeginType(TSM_VALUE, "Core.TestType", t1);
				b.Link(true, false);
				auto g = b.AddGenericParameter();
				b.AddConstrain(g, {}, CONSTRAIN_TRAIT_ASSEMBLY, trait.Id);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				auto t = LoadType(&l, "Core", "Core.BasicType", false);
				LoadType(&l, "Core", "Core.TestType", { t }, true);
			}
		}

		TEST_METHOD(Field)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto ft1 = b.BeginType(TSM_VALUE, "Core.FieldType1");
				b.Link(true, false);
				b.EndType();
				auto ft2 = b.BeginType(TSM_VALUE, "Core.FieldType2");
				b.Link(true, false);
				b.EndType();
				auto ft3 = b.BeginType(TSM_VALUE, "Core.FieldType3");
				b.Link(true, false);
				b.AddGenericParameter();
				b.EndType();

				auto target = b.BeginType(TSM_VALUE, "Core.CompoundType");
				b.Link(true, false);
				b.AddField(ft1, "FieldA");
				b.AddField(b.MakeType(ft3, { ft2 }), "FieldB");
				b.EndType();

				auto t1 = b.BeginTrait("Core.Trait1");
				b.AddTraitField(ft1, "FieldA", "FieldA");
				b.EndTrait();
				auto t2 = b.BeginTrait("Core.Trait2");
				b.AddTraitField(ft2, "FieldA", "FieldA");
				b.EndTrait();
				auto t3 = b.BeginTrait("Core.Trait3");
				b.AddTraitField(ft1, "FieldC", "FieldC");
				b.EndTrait();
				auto t4 = b.BeginTrait("Core.Trait4");
				auto g = b.AddGenericParameter();
				b.AddTraitField(b.MakeType(ft3, { g }), "FieldB", "FieldB");
				b.EndTrait();

				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.Link(true, false);
				b.AddConstrain(target, {}, CONSTRAIN_TRAIT_ASSEMBLY, t1.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.Link(true, false);
				b.AddConstrain(target, {}, CONSTRAIN_TRAIT_ASSEMBLY, t2.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType3");
				b.Link(true, false);
				b.AddConstrain(target, {}, CONSTRAIN_TRAIT_ASSEMBLY, t3.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType4");
				b.Link(true, false);
				b.AddConstrain(target, { b.AnyType() }, CONSTRAIN_TRAIT_ASSEMBLY, t4.Id);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType5");
				b.Link(true, false);
				auto gx = b.AddGenericParameter();
				b.AddConstrain(target, { gx }, CONSTRAIN_TRAIT_ASSEMBLY, t4.Id);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				auto t1 = LoadType(&l, "Core", "Core.FieldType1", false);
				auto t2 = LoadType(&l, "Core", "Core.FieldType2", false);
				auto t3 = LoadType(&l, "Core", "Core.FieldType3", { t2 }, false);
				LoadType(&l, "Core", "Core.TestType1", false);
				LoadType(&l, "Core", "Core.TestType2", true);
				LoadType(&l, "Core", "Core.TestType3", true);
				LoadType(&l, "Core", "Core.TestType4", false);
				LoadType(&l, "Core", "Core.TestType5", { t2 }, false);
			}
		}

		TEST_METHOD(ReferenceTypeField)
		{
			//A quite complicated test case testing interface, any, and ref field.
			Builder b;
			{
				b.BeginAssembly("Core");
				auto vt = b.BeginType(TSM_VALUE, "Core.ValueType");
				b.EndType();
				auto i = b.BeginType(TSM_INTERFACE, "Core.Interface");
				auto g1 = b.AddGenericParameter();
				b.EndType();
				auto rt = b.BeginType(TSM_REFERENCE, "Core.RefType");
				auto g2 = b.AddGenericParameter();
				b.AddInterface(b.MakeType(i, { g2 }), {}, {});
				b.AddField(b.MakeType(rt, { g2 }), "FieldA");
				b.EndType();
				auto tr = b.BeginTrait("Core.Trait");
				auto g3 = b.AddGenericParameter();
				b.AddConstrain(g3, { b.MakeType(i, { vt }) }, CONSTRAIN_INTERFACE, 0);
				b.AddTraitField(g3, "FieldA", "FieldA");
				b.EndTrait();
				b.BeginType(TSM_VALUE, "Core.TestType");
				b.Link(true, false);
				b.AddConstrain(b.MakeType(rt, { vt }), { b.AnyType() }, CONSTRAIN_TRAIT_ASSEMBLY, tr.Id);
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
