#pragma once
#include "RuntimeLoaderRefList.h"

struct RuntimeLoaderConstrain : RuntimeLoaderRefList
{
public:
	bool CheckConstrainsImpl(const std::string& srcAssebly, GenericDeclaration* g,
		const std::vector<RuntimeType*>& args)
	{
		return true;
	}
};

inline bool RuntimeLoaderCore::CheckConstrains(const std::string& srcAssebly, GenericDeclaration* g,
	const std::vector<RuntimeType*>& args)
{
	auto l = static_cast<RuntimeLoaderConstrain*>(this);
	return l->CheckConstrainsImpl(srcAssebly, g, args);
}
