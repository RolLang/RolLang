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
				auto vt = LoadType(&l, "Core", "Core.ValueType", false);
				LoadType(&l, "Core", "Core.TestType1", { vt, vt }, false);
				LoadType(&l, "Core", "Core.TestType1", { nullptr, vt }, false);
				LoadType(&l, "Core", "Core.TestType1", { vt, nullptr }, true);
				LoadType(&l, "Core", "Core.TestType2", false);
				LoadType(&l, "Core", "Core.TestType3", { vt }, false);
				LoadType(&l, "Core", "Core.TestType3", { nullptr }, true);
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
				auto vt = LoadType(&l, "Core", "Core.ValueType", false);
				LoadFunction(&l, "Core", "Core.Function1", { vt }, false);
				LoadFunction(&l, "Core", "Core.Function1", { nullptr }, false);
				LoadFunction(&l, "Core", "Core.Function2", { vt }, false);
				LoadFunction(&l, "Core", "Core.Function2", { nullptr }, true);
			}
		}
		//TODO subtype (loader API, to function ret, to type)
		//TODO trait argument
		//TODO trait target (equal, exist)
		//TODO argument passing
		//TODO constraint matching
		//TODO constraint exporting
	};
}
