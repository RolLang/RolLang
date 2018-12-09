#pragma once
#include "RuntimeLoaderData.h"
#include <algorithm>

namespace RolLang {

struct RuntimeLoaderCore : RuntimeLoaderData
{
public: //Forward declaration

	inline bool CheckConstrains(const std::string& srcAssebly, GenericDeclaration* g,
		const std::vector<RuntimeType*>& args, ConstrainExportList* exportList);

	inline bool FindSubType(const SubMemberLoadingArguments& args, LoadingArguments& la);
	//No SubFunction. All member function reference is exported from trait.
	//(Because choosing the correct overload candidate is more difficult for functions,
	//we do it only in trait.)

	inline bool FindRefType(const LoadingRefArguments& lg, std::size_t typeId, LoadingArguments& la);
	inline bool FindRefFunction(const LoadingRefArguments& lg, std::size_t funcId, LoadingArguments& la);

	RuntimeType* LoadRefType(const LoadingRefArguments& lg, std::size_t typeId)
	{
		LoadingArguments la;
		if (!FindRefType(lg, typeId, la))
		{
			return nullptr;
		}
		return LoadTypeInternal(la, false);
	}

	RuntimeFunction* LoadRefFunction(const LoadingRefArguments& lg, std::size_t funcId)
	{
		LoadingArguments la;
		if (!FindRefFunction(lg, funcId, la))
		{
			return nullptr;
		}
		return LoadFunctionInternal(la);
	}

	RuntimeType* LoadSubType(const SubMemberLoadingArguments& args)
	{
		LoadingArguments la;
		if (!FindSubType(args, la))
		{
			return nullptr;
		}
		return LoadTypeInternal(la, false);
	}

	virtual void OnTypeLoaded(RuntimeType* type) = 0;
	virtual void OnFunctionLoaded(RuntimeFunction* func) = 0;

public: //External API (for RuntimeLoader external API)

	RuntimeType* LoadTypeEntry(const LoadingArguments& args, std::string& err)
	{
		_loading->ClearLoadingLists();
		RuntimeType* ret = nullptr;
		try
		{
			auto ret2 = LoadTypeInternal(args, false);
			ProcessLoadingLists();
			MoveFinishedObjects();
			ret = ret2;
		}
		catch (std::exception& ex)
		{
			err = ex.what();
		}
		catch (...)
		{
			err = "Unknown exception in loading type";
		}
		_loading->ClearLoadingLists();
		return ret;
	}

	RuntimeFunction* LoadFunctionEntry(const LoadingArguments& args, std::string& err)
	{
		_loading->ClearLoadingLists();
		RuntimeFunction* ret = nullptr;
		try
		{
			auto ret2 = LoadFunctionInternal(args);
			ProcessLoadingLists();
			MoveFinishedObjects();
			ret = ret2;
		}
		catch (std::exception& ex)
		{
			err = ex.what();
		}
		catch (...)
		{
			err = "Unknown exception in loading function";
		}
		_loading->ClearLoadingLists();
		return ret;
	}

	RuntimeType* AddNativeTypeInternal(const std::string& assemblyName, std::size_t id,
		std::size_t size, std::size_t alignment)
	{
		auto a = FindAssemblyThrow(assemblyName);
		auto& type = a->Types[id];
		if (type.Generic.ParameterCount)
		{
			throw RuntimeLoaderException("Native type cannot be generic");
		}
		if (type.GCMode != TSM_VALUE)
		{
			throw RuntimeLoaderException("Internal type can only be value type");
		}
		if (type.Finalizer >= type.Generic.Functions.size())
		{
			throw RuntimeLoaderException("Invalid function reference");
		}
		if ((type.Generic.Functions[type.Finalizer].Type & REF_REFTYPES) != REF_EMPTY)
		{
			throw RuntimeLoaderException("Internal type cannot have finalizer");
		}
		if (type.Initializer >= type.Generic.Functions.size())
		{
			throw RuntimeLoaderException("Invalid function reference");
		}
		if ((type.Generic.Functions[type.Initializer].Type & REF_REFTYPES) != REF_EMPTY)
		{
			throw RuntimeLoaderException("Internal type cannot have initializer");
		}
		auto rt = std::make_unique<RuntimeType>();
		rt->Parent = _loader;
		rt->TypeId = _nextTypeId++;
		rt->Args.Assembly = assemblyName;
		rt->Args.Id = id;
		rt->Storage = TSM_VALUE;
		rt->Size = size;
		rt->Alignment = alignment;
		rt->Initializer = nullptr;
		rt->Finalizer = nullptr;
		rt->BaseType.Type = nullptr;
#if _DEBUG
		rt->Fullname = rt->GetFullname();
#endif

		auto ret = rt.get();
		FinalCheckType(ret);
		OnTypeLoaded(ret);
		AddLoadedType(std::move(rt));
		return ret;
	}

public: //Internal API (for other modules)

	bool CheckGenericArguments(GenericDeclaration& g, const LoadingArguments& args,
		ConstrainExportList* exportList)
	{
		//TODO We should use different list for type and functions. (low priority)
		for (auto& i : _loading->_constrainCheckingTypes)
		{
			if (i == args)
			{
				throw RuntimeLoaderException("Circular constrain reference");
			}
		}
		_loading->_constrainCheckingTypes.push_back(args);

		if (g.ParameterCount != args.Arguments.size())
		{
			return false;
		}
		if (std::any_of(args.Arguments.begin(), args.Arguments.end(),
			[](RuntimeType* t) { return t == nullptr; }))
		{
			return false;
		}
		auto ret = CheckConstrains(args.Assembly, &g, args.Arguments, exportList);

		assert(_loading->_constrainCheckingTypes.back() == args);
		_loading->_constrainCheckingTypes.pop_back();
		return ret;
	}

	RuntimeType* LoadTypeInternal(const LoadingArguments& args, bool skipArgumentCheck)
	{
		for (auto& t : _loadedTypes)
		{
			if (t && t->Args == args)
			{
				return t.get();
			}
		}
		for (auto& t : _loading->_finishedLoadingTypes)
		{
			if (t->Args == args)
			{
				return t.get();
			}
		}
		for (auto& t : _loading->_postLoadingTypes)
		{
			if (t->Args == args)
			{
				return t.get();
			}
		}
		for (auto& t : _loading->_loadingRefTypes)
		{
			if (t->Args == args)
			{
				return t.get();
			}
		}
		for (auto t : _loading->_loadingTypes)
		{
			if (t->Args == args)
			{
				return t;
			}
		}

		ConstrainExportList exportList;
		auto typeTemplate = FindTypeTemplate(args);
		if (!skipArgumentCheck && !CheckGenericArguments(typeTemplate->Generic, args, &exportList))
		{
			throw RuntimeLoaderException("Invalid generic arguments");
		}

		if (typeTemplate->Generic.Fields.size() != 0)
		{
			throw RuntimeLoaderException("Type template cannot contain field reference");
		}

		if (args.Assembly == "Core" && args.Id == _boxTypeId)
		{
			if (args.Arguments.size() != 1 ||
				args.Arguments[0]->Storage != TSM_VALUE)
			{
				throw RuntimeLoaderException("Box type can only take value type as argument");
			}
		}

		auto t = std::make_unique<RuntimeType>();
		t->Parent = _loader;
		t->Args = args;
		t->TypeId = _nextTypeId++;
		t->Storage = typeTemplate->GCMode;
		t->PointerType = nullptr;
		t->ConstrainExportList = std::move(exportList);
#if _DEBUG
		t->Fullname = t->GetFullname();
#endif

		if (typeTemplate->GCMode == TSM_REFERENCE)
		{
			RuntimeType* ret = t.get();
			_loading->_loadingRefTypes.push_back(std::move(t));
			return ret;
		}
		else
		{
			//Note that interfaces also go here. We need to use LoadFields to check circular inheritance
			//of interfaces.
			return LoadFields(std::move(t), typeTemplate);
		}
	}

	RuntimeFunction* LoadFunctionInternal(const LoadingArguments& args)
	{
		for (auto& ff : _loadedFunctions)
		{
			if (ff && ff->Args == args)
			{
				return ff.get();
			}
		}
		for (auto& ff : _loading->_finishedLoadingFunctions)
		{
			if (ff->Args == args)
			{
				return ff.get();
			}
		}
		for (auto& ff : _loading->_loadingFunctions)
		{
			if (ff->Args == args)
			{
				return ff.get();
			}
		}

		ConstrainExportList exportList;
		auto funcTemplate = FindFunctionTemplate(args.Assembly, args.Id);
		if (!CheckGenericArguments(funcTemplate->Generic, args, &exportList))
		{
			throw RuntimeLoaderException("Invalid generic arguments");
		}

		auto f = std::make_unique<RuntimeFunction>();
		f->Args = args;
		f->Parent = _loader;
		f->FunctionId = _nextFunctionId++;
		f->Code = GetCode(args.Assembly, args.Id);
		f->ConstrainExportList = std::move(exportList);

		auto ret = f.get();
		_loading->_loadingFunctions.push_back(std::move(f));
		_loading->CheckLoadingSizeLimit(_loadingLimit);

		return ret;
	}

private:
	void MoveFinishedObjects()
	{
		for (auto& t : _loading->_finishedLoadingTypes)
		{
			FinalCheckType(t.get());
			OnTypeLoaded(t.get());
		}
		for (auto& f : _loading->_finishedLoadingFunctions)
		{
			FinalCheckFunction(f.get());
			OnFunctionLoaded(f.get());
		}
		while (_loading->_finishedLoadingTypes.size() > 0)
		{
			auto t = std::move(_loading->_finishedLoadingTypes.front());
			AddLoadedType(std::move(t));
			_loading->_finishedLoadingTypes.pop_front();
		}
		while (_loading->_finishedLoadingFunctions.size() > 0)
		{
			auto f = std::move(_loading->_finishedLoadingFunctions.front());
			AddLoadedFunction(std::move(f));
			_loading->_finishedLoadingFunctions.pop_front();
		}
	}

	void ProcessLoadingLists()
	{
		assert(_loading->_loadingTypes.size() == 0);

		while (true)
		{
			if (_loading->_loadingRefTypes.size())
			{
				auto t = std::move(_loading->_loadingRefTypes.front());
				_loading->_loadingRefTypes.pop_front();
				LoadFields(std::move(t), nullptr);
				assert(_loading->_loadingTypes.size() == 0);
				continue;
			}
			if (_loading->_postLoadingTypes.size())
			{
				auto t = std::move(_loading->_postLoadingTypes.front());
				_loading->_postLoadingTypes.pop_front();
				PostLoadType(std::move(t));
				assert(_loading->_loadingTypes.size() == 0);
				continue;
			}
			if (_loading->_loadingFunctions.size())
			{
				auto t = std::move(_loading->_loadingFunctions.front());
				if (t == nullptr)
				{
					//We sometimes need to force function loading, in which case
					//the unique_ptr is moved out. See EnsureFunctionLoaded.
					continue;
				}
				_loading->_loadingFunctions.pop_front();
				PostLoadFunction(std::move(t));
				assert(_loading->_loadingTypes.size() == 0);
				continue;
			}
			break;
		}
	}

private:
	RuntimeType* LoadFields(std::unique_ptr<RuntimeType> type, Type* typeTemplate)
	{
		for (auto t : _loading->_loadingTypes)
		{
			assert(!(t->Args == type->Args));
		}
		_loading->_loadingTypes.push_back(type.get());
		_loading->CheckLoadingSizeLimit(_loadingLimit);

		Type* tt = typeTemplate;
		if (tt == nullptr)
		{
			tt = FindTypeTemplate(type->Args);
		}

		if (type->Storage == TSM_INTERFACE)
		{
			if (tt->Fields.size() != 0)
			{
				throw RuntimeLoaderException("Interface cannot have fields");
			}
		}

		//Base type
		auto baseType = LoadRefType({ type.get(), tt->Generic }, tt->Base.InheritedType);
		if (baseType != nullptr)
		{
			if (type->Storage == TSM_GLOBAL)
			{
				throw RuntimeLoaderException("Global type cannot have base type");
			}
			else if (type->Storage == TSM_INTERFACE)
			{
				throw RuntimeLoaderException("Interface cannot have base type");
			}
			else
			{
				if (type->Storage != baseType->Storage)
				{
					throw RuntimeLoaderException("Base type storage must be same as the derived type");
				}
			}
		}
		type->BaseType = LoadVirtualTable(type.get(), type.get(), tt->Generic, tt->Base.VirtualFunctions, baseType);
		if (type->Storage == TSM_VALUE && type->BaseType.VirtualFunctions.size() > 0)
		{
			throw RuntimeLoaderException("Value type cannot have virtual functions");
		}

		if (type->Storage == TSM_INTERFACE)
		{
			//Within the range of _loadingTypes, we can avoid circular interface inheritance.
			LoadInterfaces(type.get(), type.get(), tt);
		}

		std::size_t offset = 0, totalAlignment = 1;

		//Fields
		std::vector<RuntimeType*> fields;
		for (auto typeId : tt->Fields)
		{
			auto fieldType = LoadRefType({ type.get(), tt->Generic }, typeId);
			if (fieldType == nullptr)
			{
				//Only goes here if REF_EMPTY is specified.
				throw RuntimeLoaderException("Invalid field type");
			}
			if (fieldType->Storage == TSM_VALUE && fieldType->Alignment == 0)
			{
				assert(std::any_of(_loading->_loadingTypes.begin(), _loading->_loadingTypes.end(),
					[fieldType](RuntimeType* t) { return t == fieldType; }));
				throw RuntimeLoaderException("Circular type dependence");
			}
			fields.push_back(fieldType);
		}

		for (std::size_t i = 0; i < fields.size(); ++i)
		{
			auto ftype = fields[i];
			std::size_t len, alignment;
			switch (ftype->Storage)
			{
			case TSM_REFERENCE:
			case TSM_INTERFACE:
				len = alignment = _ptrSize;
				break;
			case TSM_VALUE:
				len = ftype->Size;
				alignment = ftype->Alignment;
				break;
			default:
				throw RuntimeLoaderException("Invalid field type");
			}
			offset = (offset + alignment - 1) / alignment * alignment;
			totalAlignment = alignment > totalAlignment ? alignment : totalAlignment;
			type->Fields.push_back({ ftype, offset, len });
			offset += len;
		}
		type->Size = offset;
		type->Alignment = totalAlignment;

		auto ret = type.get();
		_loading->_postLoadingTypes.emplace_back(std::move(type));

		assert(_loading->_loadingTypes.back() == ret);
		_loading->_loadingTypes.pop_back();

		return ret;
	}

	void PostLoadType(std::unique_ptr<RuntimeType> type)
	{
		auto typeTemplate = FindTypeTemplate(type->Args);

		for (std::size_t i = 0; i < typeTemplate->Generic.Types.size(); ++i)
		{
			if (typeTemplate->Generic.Types[i].Type & REF_FORCELOAD)
			{
				SetValueInList(type->References.Types, i, LoadRefType({ type.get(), typeTemplate->Generic }, i));
			}
		}
		for (std::size_t i = 0; i < typeTemplate->Generic.Functions.size(); ++i)
		{
			if (typeTemplate->Generic.Functions[i].Type & REF_FORCELOAD)
			{
				SetValueInList(type->References.Functions, i, LoadRefFunction({ type.get(), typeTemplate->Generic }, i));
			}
		}

		if (type->Storage == TSM_GLOBAL)
		{
			if (typeTemplate->Interfaces.size() != 0)
			{
				throw RuntimeLoaderException("Global and value type cannot have interfaces");
			}
		}

		if (type->Args.Assembly == "Core" && type->Args.Id == _boxTypeId)
		{
			if (type->Args.Arguments[0]->Storage == TSM_VALUE)
			{
				LoadInterfaces(type.get(), type->Args.Arguments[0], nullptr);
			}
		}
		else if (type->Storage == TSM_REFERENCE)
		{
			LoadInterfaces(type.get(), type.get(), typeTemplate);
		}
		//Interfaces of TSM_INTERFACE has already been loaded in LoadFields.

		type->Initializer = LoadRefFunction({ type.get(), typeTemplate->Generic }, typeTemplate->Initializer);
		type->Finalizer = LoadRefFunction({ type.get(), typeTemplate->Generic }, typeTemplate->Finalizer);
		if (type->Storage != TSM_GLOBAL)
		{
			if (type->Initializer != nullptr)
			{
				throw RuntimeLoaderException("Only global type can have initializer");
			}
		}
		if (type->Storage != TSM_REFERENCE)
		{
			if (type->Finalizer != nullptr)
			{
				throw RuntimeLoaderException("Only reference type can have finalizer");
			}
		}
		_loading->_finishedLoadingTypes.emplace_back(std::move(type));
	}

	void PostLoadFunction(std::unique_ptr<RuntimeFunction> func)
	{
		//TODO Optimize loading. Directly find the cloned func/type.
		auto funcTemplate = FindFunctionTemplate(func->Args.Assembly, func->Args.Id);
		for (std::size_t i = 0; i < funcTemplate->Generic.Types.size(); ++i)
		{
			if (funcTemplate->Generic.Types[i].Type & REF_FORCELOAD)
			{
				SetValueInList(func->References.Types, i,
					LoadRefType({ func.get(), funcTemplate->Generic }, i));
			}
		}
		for (std::size_t i = 0; i < funcTemplate->Generic.Functions.size(); ++i)
		{
			if (funcTemplate->Generic.Functions[i].Type & REF_FORCELOAD)
			{
				SetValueInList(func->References.Functions, i,
					LoadRefFunction({ func.get(), funcTemplate->Generic }, i));
			}
		}
		auto assembly = FindAssemblyThrow(func->Args.Assembly);
		for (std::size_t i = 0; i < funcTemplate->Generic.Fields.size(); ++i)
		{
			func->References.Fields.push_back(LoadFieldIndex(assembly, func.get(), funcTemplate->Generic, i));
		}
		func->ReturnValue = func->References.Types[funcTemplate->ReturnValue.TypeId];
		for (std::size_t i = 0; i < funcTemplate->Parameters.size(); ++i)
		{
			func->Parameters.push_back(func->References.Types[funcTemplate->Parameters[i].TypeId]);
		}
#if _DEBUG
		func->Fullname = func->GetFullname();
#endif

		auto ptr = func.get();
		_loading->_finishedLoadingFunctions.emplace_back(std::move(func));
	}

	void FinalCheckType(RuntimeType* type)
	{
		if (type->Args.Assembly == "Core" && type->Args.Id == _pointerTypeId)
		{
			assert(type->Storage == TSM_VALUE);
			assert(type->Args.Arguments.size() == 1);
			auto elementType = type->Args.Arguments[0];
			assert(elementType->PointerType == nullptr);
			elementType->PointerType = type;
		}
		//TODO check box type?
		if (type->Initializer != nullptr)
		{
			if (type->Initializer->ReturnValue != nullptr ||
				type->Initializer->Parameters.size() != 0)
			{
				throw RuntimeLoaderException("Invalid initializer");
			}
		}
		if (type->Finalizer != nullptr)
		{
			if (type->Finalizer->ReturnValue != nullptr ||
				type->Finalizer->Parameters.size() != 1)
			{
				throw RuntimeLoaderException("Invalid finalizer");
			}
			if (type->Finalizer->Parameters[0] != type)
			{
				throw RuntimeLoaderException("Invalid finalizer");
			}
		}
	}

	void FinalCheckFunction(RuntimeFunction* func)
	{
	}

	void LoadInterfaces(RuntimeType* dest, RuntimeType* src, Type* srcTemplate)
	{
		if (srcTemplate == nullptr)
		{
			srcTemplate = FindTypeTemplate(src->Args);
		}
		for (auto& i : srcTemplate->Interfaces)
		{
			auto baseType = LoadRefType({ src, srcTemplate->Generic }, i.InheritedType);
			if (baseType == nullptr)
			{
				throw RuntimeLoaderException("Interface type not specified");
			}
			if (baseType->Storage != TSM_INTERFACE)
			{
				throw RuntimeLoaderException("Interface must be interface storage");
			}

			dest->Interfaces.emplace_back(LoadVirtualTable(src, dest, srcTemplate->Generic,
				i.VirtualFunctions, baseType));
		}
	}

	void EnsureFunctionLoaded(RuntimeFunction* f)
	{
		for (auto& func : _loading->_loadingFunctions)
		{
			if (func.get() == f)
			{
				PostLoadFunction(std::move(func));
			}
		}
	}

	//TODO not tested
	bool CheckVirtualFunctionEqual(RuntimeFunction* a, RuntimeFunction* b)
	{
		EnsureFunctionLoaded(a);
		EnsureFunctionLoaded(b);
		if (a->ReturnValue != b->ReturnValue) return false;
		if (a->Parameters.size() != b->Parameters.size()) return false;
		for (std::size_t i = 0; i < a->Parameters.size(); ++i)
		{
			if (a->Parameters[i] != b->Parameters[i]) return false;
		}
		return true;
	}

	RuntimeType::InheritanceInfo LoadVirtualTable(RuntimeType* type, RuntimeType* target, GenericDeclaration& g,
		const std::vector<InheritanceVirtualFunctionInfo>& types, RuntimeType* baseType)
	{
		RuntimeType::InheritanceInfo ret = {};
		ret.Type = baseType;
		Type* tt = FindTypeTemplate(type->Args);

		for (std::size_t i = 0; i < types.size(); ++i)
		{
			auto& fid = types[i];
			auto virt = LoadRefFunction({ type, g }, fid.VirtualFunction);
			auto& name = tt->Base.VirtualFunctions[i].Name;
			if (baseType && baseType->BaseType.VirtualFunctions.size() > i)
			{
				if (baseType->BaseType.VirtualFunctions[i].Name != name ||
					baseType->BaseType.VirtualFunctions[i].V != virt)
				{
					throw RuntimeLoaderException("Virtual table not matching");
				}
				assert(virt->Virtual->Slot == i);
			}
			else
			{
				virt->Virtual = std::make_unique<RuntimeVirtualFunction>(RuntimeVirtualFunction{ target, i });
			}

			auto impl = LoadRefFunction({ type, g }, fid.Implementation);
			if (impl != nullptr)
			{
				if (virt == nullptr || !CheckVirtualFunctionEqual(virt, impl))
				{
					throw RuntimeLoaderException("Virtual table not matching");
				}
			}
			ret.VirtualFunctions.push_back({ name, virt, impl });
		}
		return ret;
	}

	std::size_t LoadFieldIndex(Assembly* assembly, RuntimeFunction* f, GenericDeclaration& g, std::size_t index)
	{
		if (index >= g.Fields.size())
		{
			throw RuntimeLoaderException("Invalid field reference");
		}
		while (g.Fields[index].Type == REF_CLONE)
		{
			index = g.Fields[index].Index;
		}
		switch (g.Fields[index].Type)
		{
		case REF_FIELDID:
			return g.Fields[index].Index;
		case REF_IMPORT:
			return LoadImportConstant(assembly, g.Fields[index].Index);
		case REF_CONSTRAIN:
		{
			ConstrainExportList* list = &f->ConstrainExportList;
			for (auto& e : *list)
			{
				if (e.EntryType == CONSTRAIN_EXPORT_FIELD && e.Index == index)
				{
					return e.Field;
				}
			}
			throw RuntimeLoaderException("Invalid field reference");
		}
		default:
			throw RuntimeLoaderException("Invalid field reference");
		}
	}
};

}
