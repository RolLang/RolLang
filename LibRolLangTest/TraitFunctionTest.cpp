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
	};
}
