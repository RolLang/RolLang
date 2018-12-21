#include "stdafx.h"

namespace LibRolLangTest
{
	using namespace RuntimeLoaderHelper;
	using Builder = TestAssemblyListBuilder;

	TEST_CLASS(CircularDetectionTest)
	{
	private:
		void AddType(Builder& b, std::string name)
		{
			b.BeginType(TSM_VALUE, "Core." + name);
			b.Link(true, false);
			b.EndType();
		}

		void LoadEmptyType(RuntimeLoader* l, std::string name)
		{
			LoadType(l, "Core", "Core." + name, ERR_L_SUCCESS);
		}

	public:
		TEST_METHOD(CircularLoadType1)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				AddType(b, "A");
				AddType(b, "B");
				auto vt2 = b.ForwardDeclareType();
				auto vt1 = b.BeginType(TSM_VALUE, "Core.ValueType1");
				b.AddField(vt2);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.ValueType2", vt2);
				b.AddField(vt1);
				b.EndType();
				auto rf1 = b.BeginType(TSM_REFERENCE, "Core.ReferenceTyp1");
				b.Link(true, false);
				b.AddField(vt1);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build(), sizeof(void*), sizeof(void*), SIZE_MAX);
			{
				LoadEmptyType(&l, "A");
				LoadType(&l, "Core", "Core.ReferenceTyp1", ERR_L_CIRCULAR);
				LoadEmptyType(&l, "B");
			}
		}

		TEST_METHOD(CircularLoadType2)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto embed = b.BeginType(TSM_VALUE, "Core.Embed");
				b.Link(true, false);
				b.AddGenericParameter();
				b.EndType();

				AddType(b, "A");
				AddType(b, "B");
				auto vt1 = b.ForwardDeclareType();
				auto rf1 = b.BeginType(TSM_REFERENCE, "Core.ReferenceTyp1");
				b.AddField(vt1);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.ValueType1", vt1);
				b.AddField(b.MakeType(embed, { rf1 }));
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.ValueType2");
				b.Link(true, false);
				b.AddField(rf1);
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build(), sizeof(void*), sizeof(void*), SIZE_MAX);
			{
				LoadEmptyType(&l, "A");
				LoadType(&l, "Core", "Core.ValueType2", ERR_L_CIRCULAR);
				LoadEmptyType(&l, "B");
			}
		}

		TEST_METHOD(CircularTypeConstraintCheck)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				AddType(b, "A");
				AddType(b, "B");
				auto vt = b.BeginType(TSM_REFERENCE, "Core.ElementType");
				b.EndType();
				auto tt = b.BeginType(TSM_REFERENCE, "Core.TargetType");
				auto g = b.AddGenericParameter();
				b.AddConstraint(b.MakeType(tt, { g }), {}, CONSTRAINT_EXIST, 0);
				b.EndType();
				b.BeginType(TSM_REFERENCE, "Core.TestType");
				b.Link(true, false);
				b.AddField(b.MakeType(tt, { vt }));
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build(), sizeof(void*), sizeof(void*), SIZE_MAX);
			{
				LoadEmptyType(&l, "A");
				LoadType(&l, "Core", "Core.TestType", ERR_L_CIRCULAR);
				LoadEmptyType(&l, "B");
			}
		}

		TEST_METHOD(CircularInterface)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				AddType(b, "A");
				AddType(b, "B");
				AddType(b, "C");

				auto i2 = b.ForwardDeclareType();
				auto i1 = b.BeginType(TSM_INTERFACE, "Core.Interface1");
				b.AddInterface(i2, {}, {});
				b.EndType();
				b.BeginType(TSM_INTERFACE, "Core.Interface2", i2);
				b.AddInterface(i1, {}, {});
				b.EndType();

				auto i3 = b.BeginType(TSM_INTERFACE, "Core.Interface3");
				b.EndType();
				auto i4 = b.BeginType(TSM_INTERFACE, "Core.Interface4");
				b.AddInterface(i3, {}, {});
				b.EndType();

				auto tt1 = b.BeginType(TSM_REFERENCE, "Core.TestType1");
				b.Link(true, false);
				b.AddInterface(i2, {}, {});
				b.EndType();
				auto tt2 = b.BeginType(TSM_REFERENCE, "Core.TestType2");
				b.Link(true, false);
				b.AddInterface(i4, {}, {});
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build(), sizeof(void*), sizeof(void*), SIZE_MAX);
			{
				LoadEmptyType(&l, "A");
				LoadType(&l, "Core", "Core.TestType1", ERR_L_CIRCULAR);
				LoadEmptyType(&l, "B");
				LoadType(&l, "Core", "Core.TestType2", ERR_L_SUCCESS);
				LoadEmptyType(&l, "C");
			}
		}

		TEST_METHOD(CircularSubtype)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				AddType(b, "A");
				AddType(b, "B");
				AddType(b, "C");
				AddType(b, "D");
				auto t2 = b.ForwardDeclareType();
				auto t1 = b.BeginType(TSM_VALUE, "Core.A");
				auto t1g = b.AddGenericParameter();
				auto st2 = b.MakeSubtype(b.MakeType(t2, { t1g, b.AddAdditionalGenericParameter(0) }), "S2", {});
				b.AddSubType("S1", st2);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.B", t2);
				auto t2g1 = b.AddGenericParameter();
				auto t2g2 = b.AddGenericParameter();
				auto st1 = b.MakeSubtype(b.MakeType(t1, { t2g2 }), "S1", { t2g1 });
				b.AddSubType("S2", st1);
				b.EndType();
				auto vt1 = b.BeginType(TSM_VALUE, "Core.ValueType1");
				b.EndType();
				auto vt2 = b.BeginType(TSM_VALUE, "Core.ValueType2");
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType1");
				b.Link(true, true);
				b.AddField(b.MakeSubtype(b.MakeType(t2, { vt1, vt1 }), "S2", {}));
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType2");
				b.Link(true, true);
				b.AddField(b.MakeSubtype(b.MakeType(t2, { vt1, vt2 }), "S2", {}));
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType3");
				b.Link(true, true);
				b.AddField(b.MakeType(t2, { vt1, vt2 }));
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadEmptyType(&l, "A");
				LoadType(&l, "Core", "Core.TestType1", ERR_L_CIRCULAR);
				LoadEmptyType(&l, "B");
				LoadType(&l, "Core", "Core.TestType2", ERR_L_CIRCULAR);
				LoadEmptyType(&l, "C");
				LoadType(&l, "Core", "Core.TestType3", ERR_L_SUCCESS);
				LoadEmptyType(&l, "D");
			}
		}

		//Should move to elsewhere
		TEST_METHOD(InfiniteGeneric)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				b.BeginType(TSM_VALUE, "Core.ValueType");
				b.Link(true, false);
				b.EndType();
				auto t = b.BeginType(TSM_REFERENCE, "Core.CycType");
				auto g = b.AddGenericParameter();
				b.Link(true, false);
				b.AddField(b.MakeType(t, { b.SelfType() }));
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build(), sizeof(void*), sizeof(void*), 20);
			{
				auto t = LoadType(&l, "Core", "Core.ValueType", ERR_L_SUCCESS);
				LoadType(&l, "Core", "Core.CycType", { t }, ERR_L_LIMIT);
			}
		}
	};
}
