#pragma once

namespace LibRolLangTest
{
	namespace RuntimeLoaderHelper
	{
		using namespace RolLang;

		static RuntimeType* LoadType(RuntimeLoader* loader,
			const std::string& a, const std::string& n,
			std::vector<RuntimeType*> args, bool shouldFail)
		{
			LoadingArguments la;
			loader->FindExportType({ a, n, GenericDefArgumentListSize::Create(args.size()) }, la);
			la.Arguments = args;
			std::string err;
			auto ret = loader->GetType(la, err);
			if (shouldFail)
			{
				Assert::IsNull(ret, ToString(err.c_str()).c_str());
			}
			else
			{
				Assert::IsNotNull(ret, ToString(err.c_str()).c_str());
			}
			return ret;
		}

		static RuntimeType* LoadType(RuntimeLoader* loader,
			const std::string& a, const std::string& n, bool shouldFail)
		{
			return LoadType(loader, a, n, {}, shouldFail);
		}

		static RuntimeType* LoadNativeType(RuntimeLoader* loader,
			const std::string& a, const std::string& n, std::size_t size)
		{
			std::string err;
			auto ret = loader->AddNativeType(a, n, size, size, err);
			Assert::IsNotNull(ret, ToString(err.c_str()).c_str());
			return ret;
		}

		static void CheckValueTypeBasic(RuntimeLoader* loader, RuntimeType* t)
		{
			Assert::AreEqual((std::size_t)loader, (std::size_t)t->Parent);
			Assert::AreEqual((int)TSM_VALUE, (int)t->Storage);
			Assert::IsNull(t->Initializer);
			Assert::IsNull(t->Finalizer);
		}

		static void CheckValueTypeSize(RuntimeType* t, std::size_t s, std::size_t a)
		{
			Assert::AreEqual(s, t->Size);
			Assert::AreEqual(a, t->Alignment);
		}

		static void CheckFieldOffset(RuntimeType* t, std::vector<std::size_t> v)
		{
			Assert::AreEqual(v.size(), t->Fields.size());
			for (std::size_t i = 0; i < v.size(); ++i)
			{
				Assert::AreEqual(v[i], t->Fields[i].Offset);
			}
		}

		static void CheckReferenceTypeBasic(RuntimeLoader* loader, RuntimeType* t)
		{
			Assert::AreEqual((std::size_t)loader, (std::size_t)t->Parent);
			Assert::AreEqual((int)TSM_REFERENCE, (int)t->Storage);
			Assert::AreEqual(sizeof(void*), t->GetStorageAlignment());
			Assert::AreEqual(sizeof(void*), t->GetStorageSize());
			Assert::IsNull(t->Initializer);
			Assert::IsNull(t->Finalizer);
		}

		static void CheckGlobalTypeBasic(RuntimeLoader* loader, RuntimeType* t)
		{
			Assert::AreEqual((std::size_t)loader, (std::size_t)t->Parent);
			Assert::AreEqual((int)TSM_GLOBAL, (int)t->Storage);
			Assert::IsNull(t->Initializer);
			Assert::IsNull(t->Finalizer);
		}

		static RuntimeFunction* LoadFunction(RuntimeLoader* loader,
			const std::string& a, const std::string& n,
			std::vector<RuntimeType*> args, bool shouldFail)
		{
			LoadingArguments la;
			loader->FindExportFunction({ a, n, GenericDefArgumentListSize::Create(args.size()) }, la);
			la.Arguments = args;
			std::string err;
			auto ret = loader->GetFunction(la, err);
			if (shouldFail)
			{
				Assert::IsNull(ret, ToString(err.c_str()).c_str());
			}
			else
			{
				Assert::IsNotNull(ret, ToString(err.c_str()).c_str());
			}
			return ret;
		}

		static RuntimeFunction* LoadFunction(RuntimeLoader* loader,
			const std::string& a, const std::string& n, bool shouldFail)
		{
			return LoadFunction(loader, a, n, {}, shouldFail);
		}

		static void CheckFunctionBasic(RuntimeLoader* loader, RuntimeFunction* f)
		{
			Assert::AreEqual((std::size_t)loader, (std::size_t)f->Parent);
			Assert::IsFalse((bool)f->Code);
		}

		static void CheckFunctionTypes(RuntimeFunction* f, std::size_t retSize,
			std::vector<std::size_t> paramSizes)
		{
			if (retSize == 0)
			{
				Assert::IsNull(f->ReturnValue);
			}
			else
			{
				Assert::IsNotNull(f->ReturnValue);
				Assert::AreEqual(retSize, f->ReturnValue->GetStorageSize());
			}
			Assert::AreEqual(paramSizes.size(), f->Parameters.size());
			for (std::size_t i = 0; i < paramSizes.size(); ++i)
			{
				Assert::AreEqual(paramSizes[i], f->Parameters[i]->GetStorageSize());
			}
		}

	}
}
