#pragma once
#include "RuntimeLoaderConstrain.h"

class RuntimeLoader : RuntimeLoaderConstrain
{
public:
	RuntimeLoader(AssemblyList assemblies, std::size_t ptrSize = sizeof(void*),
		std::size_t itabPtrSize = sizeof(void*), std::size_t loadingLimit = 256)
	{
		_loader = this;

		_assemblies = std::move(assemblies);
		_ptrSize = ptrSize;
		_itabPtrSize = itabPtrSize;
		_loadingLimit = loadingLimit;

		FindInternalTypeId();
	}

	virtual ~RuntimeLoader() {}

public: //Public API

	std::size_t GetPointerSize()
	{
		return _ptrSize;
	}

	using RuntimeLoaderData::FindExportType;
	using RuntimeLoaderData::FindExportFunction;

	RuntimeType* GetType(const LoadingArguments& args, std::string& err)
	{
		std::lock_guard<Spinlock> lock(_loaderLock);
		for (auto& t : _loadedTypes)
		{
			if (t && t->Args == args)
			{
				return t.get();
			}
		}
		RuntimeLoaderLoadingData loading = {};
		_loading = &loading;
		return LoadTypeEntry(args, err);
	}

	RuntimeFunction* GetFunction(const LoadingArguments& args, std::string& err)
	{
		std::lock_guard<Spinlock> lock(_loaderLock);
		for (auto& f : _loadedFunctions)
		{
			if (f->Args == args)
			{
				return f.get();
			}
		}
		RuntimeLoaderLoadingData loading = {};
		_loading = &loading;
		return LoadFunctionEntry(args, err);
	}

	RuntimeType* GetTypeById(std::uint32_t id)
	{
		std::lock_guard<Spinlock> lock(_loaderLock);
		if (id >= _loadedTypes.size())
		{
			return nullptr;
		}
		return _loadedTypes[id].get();
	}

	RuntimeFunction* GetFunctionById(std::uint32_t id)
	{
		std::lock_guard<Spinlock> lock(_loaderLock);
		if (id > _loadedFunctions.size())
		{
			return nullptr;
		}
		return _loadedFunctions[id].get();
	}

	RuntimeType* AddNativeType(const std::string& assemblyName, const std::string& name,
		std::size_t size, std::size_t alignment, std::string& err)
	{
		std::lock_guard<Spinlock> lock(_loaderLock);
		try
		{
			auto id = FindNativeIdThrow(FindAssemblyThrow(assemblyName)->NativeTypes, name);
			return AddNativeTypeInternal(assemblyName, id, size, alignment);
		}
		catch (std::exception& ex)
		{
			err = ex.what();
		}
		catch (...)
		{
			err = "Unknown exception in adding native type";
		}
		return nullptr;
	}

public:
	RuntimeType* LoadPointerType(RuntimeType* t, std::string& err)
	{
		assert(t->PointerType == nullptr);
		LoadingArguments args;
		args.Assembly = "Core";
		args.Id = _pointerTypeId;
		args.Arguments.push_back(t);
		return GetType(args, err);
	}

	bool IsPointerType(RuntimeType* t)
	{
		return t->Args.Assembly == "Core" && t->Args.Id == _pointerTypeId;
	}

protected:
	Assembly* FindAssembly(const std::string& name)
	{
		return FindAssemblyNoThrow(name);
	}

	std::size_t FindNativeId(const std::vector<AssemblyExport>& list,
		const std::string name)
	{
		return FindNativeIdNoThrow(list, name);
	}

	virtual void OnTypeLoaded(RuntimeType* type) override
	{
	}

	virtual void OnFunctionLoaded(RuntimeFunction* func) override
	{
	}

protected:
	//We don't expect loader to run very often. A simple spinlock
	//should be enough.
	Spinlock _loaderLock;
};

inline std::size_t RuntimeType::GetStorageSize()
{
	return Storage == TSM_REFERENCE || Storage == TSM_INTERFACE ?
		Parent->GetPointerSize() : Size;
}

inline std::size_t RuntimeType::GetStorageAlignment()
{
	return Storage == TSM_REFERENCE || Storage == TSM_INTERFACE ?
		Parent->GetPointerSize() : Alignment;
}
