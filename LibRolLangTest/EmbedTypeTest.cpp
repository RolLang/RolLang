#include "stdafx.h"

namespace LibRolLangTest
{
	using namespace RuntimeLoaderHelper;
	using Builder = TestAssemblyListBuilder;

	TEST_CLASS(EmbedTypeTest)
	{
		TEST_METHOD(SimpleTypeTest)
		{
			Builder b;
			{
				b.BeginAssembly("Core");
				auto embed = b.BeginType(TSM_VALUE, "Core.Embed");
				b.Link(true, false);
				b.AddGenericParameter();
				b.EndType();

				auto vt1 = b.BeginType(TSM_VALUE, "Core.ValueType1");
				b.Link(false, true);
				b.EndType();
				auto vt2 = b.BeginType(TSM_VALUE, "Core.ValueType2");
				b.Link(false, true);
				b.EndType();
				auto rt1 = b.BeginType(TSM_REFERENCE, "Core.ReferenceType1");
				b.EndType();

				auto ct = b.BeginType(TSM_REFERENCE, "Core.ContainerType");
				b.AddField(vt1);
				b.AddField(rt1);
				b.AddField(vt2);
				b.EndType();
				b.BeginType(TSM_VALUE, "Core.TestType");
				b.Link(true, false);
				b.AddField(b.MakeType(embed, { ct }));
				b.EndType();
				b.EndAssembly();
			}
			RuntimeLoader l(b.Build());
			{
				LoadNativeType(&l, "Core", "Core.ValueType1", 8);
				LoadNativeType(&l, "Core", "Core.ValueType2", 4);
				auto t = LoadType(&l, "Core", "Core.TestType", false);
				CheckValueTypeBasic(&l, t);
				CheckValueTypeSize(t, 16, 8);
				CheckFieldOffset(t, { 0 });
				auto c = t->Fields[0].Type;
				CheckValueTypeBasic(&l, c);
				CheckValueTypeSize(c, 16, 8);
				CheckFieldOffset(c, { 0, 8, 12 });
			}
		}
	};
}
