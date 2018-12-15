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
			LoadType(l, "Core", "Core." + name, false);
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
				LoadType(&l, "Core", "Core.ReferenceTyp1", true);
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
				LoadType(&l, "Core", "Core.ValueType2", true);
				LoadEmptyType(&l, "B");
			}
		}

		TEST_METHOD(CircularTypeConstrainCheck)
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
				b.AddConstrain(b.MakeType(tt, { g }), {}, CONSTRAIN_EXIST, 0);
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
				LoadType(&l, "Core", "Core.TestType", true);
				LoadEmptyType(&l, "B");
			}
		}
	};
}
