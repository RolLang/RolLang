#pragma once
#include "RuntimeLoaderConstrain.h"

//TODO Trait component export
//TODO Import virtual function from constrain (trait)
//TODO Consider put constrain on imported types (automatically checked and exported by member name)?
//TODO Parameter pack
//TODO Variable sized object (array, string)
//TODO Embed ref types
//TODO Field index reference (no need to import index as constant) in GenericDeclaration
//TODO Multiple type template (partial specialization)
//TODO Attribute
//TODO Public API for subtype and constrain
//TODO Test for pointer size
//TODO Test for import trait

namespace RolLang {

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

	RuntimeType* GetTypeById(std::size_t id)
	{
		std::lock_guard<Spinlock> lock(_loaderLock);
		if (id >= _loadedTypes.size())
		{
			return nullptr;
		}
		return _loadedTypes[id].get();
	}

	RuntimeFunction* GetFunctionById(std::size_t id)
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

private:
	std::string GetTypeArgNameInternal(const RuntimeObjectSymbol& sym, std::size_t& x)
	{
		if (x >= sym.Hierarchy.size()) return "[error]";

		auto& t = sym.Hierarchy[x++];
		if (t.Assembly.length() == 0 && t.Id == SIZE_MAX && t.ArgumentCount == 0)
		{
			return "";
		}
		auto a = FindAssemblyNoThrow(t.Assembly);
		if (a == nullptr) return "[error]";
		if (t.Id >= a->Types.size()) return "[error]";
		std::string name = "[" + t.Assembly + ":" + std::to_string(t.Id) + "]";
		for (auto& e : a->ExportTypes)
		{
			if (e.InternalId == t.Id)
			{
				name = e.ExportName;
				break;
			}
		}
		if (t.ArgumentCount != 0)
		{
			name = name + "<";
			for (std::size_t i = 0; i < t.ArgumentCount; ++i)
			{
				auto&& tt = GetTypeArgNameInternal(sym, x);
				if (i == 0)
				{
					assert(tt.length() == 0);
				}
				else if (tt.length() == 0)
				{
					name += "; ";
				}
				else
				{
					if (i != 1) name += ", ";
					name += tt;
				}
			}
			name += ">";
		}
		return name;
	}

public:
	std::string GetTypeArgName(const RuntimeObjectSymbol& sym)
	{
		if (sym.Loader != this) return "[unknown]";
		std::size_t i = 0;
		return GetTypeArgNameInternal(sym, i);
	}

	std::string GetFunctionArgName(const RuntimeObjectSymbol& sym)
	{
		if (sym.Loader != this) return "[unknown]";
		if (sym.Hierarchy.size() == 0) return "[error]";
		auto& f = sym.Hierarchy[0];
		auto a = FindAssemblyNoThrow(f.Assembly);
		if (a == nullptr) return "[error]";
		if (f.Id >= a->Functions.size()) return "[error]";
		std::string name = "[" + f.Assembly + ":" + std::to_string(f.Id) + "]";
		for (auto& e : a->ExportFunctions)
		{
			if (e.InternalId == f.Id)
			{
				name = e.ExportName;
				break;
			}
		}
		if (f.ArgumentCount != 0)
		{
			name += "<";
			for (std::size_t i = 0, j = 1; i < f.ArgumentCount; ++i)
			{
				auto&& t = GetTypeArgNameInternal(sym, j);
				if (i == 0)
				{
					assert(t.length() == 0);
				}
				else if (t.length() == 0)
				{
					name += "; ";
				}
				else
				{
					if (i != 1) name += ", ";
					name += t;
				}
			}
			name += ">";
		}
		return name;
	}

public:
	RuntimeType* LoadPointerType(RuntimeType* t, std::string& err)
	{
		assert(t->PointerType == nullptr);
		LoadingArguments args;
		args.Assembly = "Core";
		args.Id = _pointerTypeId;
		args.Arguments = { { t } };
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

std::string RuntimeType::GetFullname()
{
	return Parent->GetTypeArgName(Args.ConvertToSymbol(Parent));
}

std::string RuntimeFunction::GetFullname()
{
	auto fullname = Parent->GetFunctionArgName(Args.ConvertToSymbol(Parent));
	fullname += "(";
	for (std::size_t i = 0; i < Parameters.size(); ++i)
	{
		if (i != 0) fullname += ", ";
		fullname += Parameters[i]->GetFullname();
	}
	fullname += ")";
	return fullname;
}

}
