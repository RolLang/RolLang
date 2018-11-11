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
	};
}
