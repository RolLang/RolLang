#pragma once
#include "RuntimeLoaderData.h"
#include <algorithm>

namespace RolLang {

struct RuntimeLoaderCore : RuntimeLoaderData
{
public: //Forward declaration

	inline bool CheckConstraints(const std::string& srcAssebly, GenericDeclaration* g,
		const MultiList<RuntimeType*>& args, ConstraintExportList* exportList);

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
		return LoadFunctionInternal(la, false);
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

	RuntimeType* LoadTypeEntry(const LoadingArguments& args, LoaderErrorInformation& err)
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
		catch (RuntimeLoaderException& ex)
		{
			err = ex.info;
		}
		catch (std::exception& ex)
		{
			err = { ERR_L_UNKNOWN, ex.what() };
		}
		catch (...)
		{
			err = { ERR_L_UNKNOWN, "Unknown exception in loading type" };
		}
		_loading->ClearLoadingLists();
		return ret;
	}

	RuntimeFunction* LoadFunctionEntry(const LoadingArguments& args, LoaderErrorInformation& err)
	{
		_loading->ClearLoadingLists();
		RuntimeFunction* ret = nullptr;
		try
		{
			auto ret2 = LoadFunctionInternal(args, false);
			ProcessLoadingLists();
			MoveFinishedObjects();
			ret = ret2;
		}
		catch (RuntimeLoaderException& ex)
		{
			err = ex.info;
		}
		catch (std::exception& ex)
		{
			err = { ERR_L_UNKNOWN, ex.what() };
		}
		catch (...)
		{
			err = { ERR_L_UNKNOWN, "Unknown exception in loading function" };
		}
		_loading->ClearLoadingLists();
		return ret;
	}

	RuntimeType* AddNativeTypeInternal(const std::string& assemblyName, std::size_t id,
		std::size_t size, std::size_t alignment)
	{
		auto a = FindAssemblyThrow(assemblyName);
		auto& type = a->Types[id];
		if (!type.Generic.ParameterCount.IsEmpty())
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Native type cannot be generic");
		}
		if (type.GCMode != TSM_VALUE)
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Internal type can only be value type");
		}
		if (type.Finalizer >= type.Generic.Functions.size())
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
		}
		if ((type.Generic.Functions[type.Finalizer].Type & REF_REFTYPES) != REF_EMPTY)
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Internal type cannot have finalizer");
		}
		if (type.Initializer >= type.Generic.Functions.size())
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
		}
		if ((type.Generic.Functions[type.Initializer].Type & REF_REFTYPES) != REF_EMPTY)
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Internal type cannot have initializer");
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
	bool CheckTypeGenericArguments(GenericDeclaration& g, const LoadingArguments& args,
		ConstraintExportList* exportList)
	{
		auto check = LoadingStackScopeGuard<LoadingArguments>(_loading->_constraintCheckingTypes, args);
		return CheckGenericArguments(g, args, exportList);
	}

	bool CheckFunctionGenericArguments(GenericDeclaration& g, const LoadingArguments& args,
		ConstraintExportList* exportList)
	{
		auto check = LoadingStackScopeGuard<LoadingArguments>(_loading->_constraintCheckingFunctions, args);
		return CheckGenericArguments(g, args, exportList);
	}

	bool CheckGenericArguments(GenericDeclaration& g, const LoadingArguments& args,
		ConstraintExportList* exportList)
	{
		//TODO match multi-dimension
		if (!g.ParameterCount.CanMatch(args.Arguments.GetSizeList()))
		{
			return false;
		}
		return CheckConstraints(args.Assembly, &g, args.Arguments, exportList);
	}

	RuntimeType* LoadTypeInternal(const LoadingArguments& args, bool skipArgumentCheck)
	{
		if (args.IsEmpty())
		{
			return nullptr;
		}
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

		ConstraintExportList exportList;
		auto typeTemplate = FindTypeTemplate(args);
		if (!skipArgumentCheck && !CheckTypeGenericArguments(typeTemplate->Generic, args, &exportList))
		{
			throw RuntimeLoaderException(ERR_L_GENERIC, "Invalid generic arguments");
		}

		//Check constraints of internal types currently not supported by the constraint system.
		if (args.Assembly == "Core" && args.Id == _boxTypeId)
		{
			assert(args.Arguments.IsSingle());
			if (args.Arguments.Get(0, 0)->Storage != TSM_VALUE)
			{
				throw RuntimeLoaderException(ERR_L_GENERIC, "Box type can only take value type as argument");
			}
		}
		else if (args.Assembly == "Core" && args.Id == _embedTypeId)
		{
			assert(args.Arguments.IsSingle());
			if (args.Arguments.Get(0, 0)->Storage != TSM_REFERENCE)
			{
				throw RuntimeLoaderException(ERR_L_GENERIC, "Embed type can only take reference type as argument");
			}
		}

		if (typeTemplate->Generic.Fields.size() != 0)
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Type template cannot contain field reference");
		}

		auto t = std::make_unique<RuntimeType>();
		t->Parent = _loader;
		t->Args = args;
		t->TypeId = _nextTypeId++;
		t->Storage = typeTemplate->GCMode;
		t->ConstraintExportList = std::move(exportList);
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

	RuntimeFunction* LoadFunctionInternal(const LoadingArguments& args, bool skipArgumentCheck)
	{
		if (args.IsEmpty())
		{
			return nullptr;
		}
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

		ConstraintExportList exportList;
		auto funcTemplate = FindFunctionTemplate(args.Assembly, args.Id);
		if (!skipArgumentCheck && !CheckFunctionGenericArguments(funcTemplate->Generic, args, &exportList))
		{
			throw RuntimeLoaderException(ERR_L_GENERIC, "Invalid generic arguments");
		}

		auto f = std::make_unique<RuntimeFunction>();
		f->Args = args;
		f->Parent = _loader;
		f->FunctionId = _nextFunctionId++;
		f->Code = GetCode(args.Assembly, args.Id);
		f->ConstraintExportList = std::move(exportList);

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
				if (t == nullptr)
				{
					continue;
				}
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
				_loading->_loadingFunctions.pop_front();
				if (t == nullptr)
				{
					//See EnsureFunctionLoaded.
					continue;
				}
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
		auto check = LoadingStackScopeGuard<RuntimeType*>(_loading->_loadingTypes, type.get());
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
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Interface cannot have fields");
			}
		}

		//Base type
		auto baseType = LoadRefType({ type.get(), tt->Generic }, tt->Base.InheritedType);
		if (baseType != nullptr)
		{
			if (baseType->Alignment == 0)
			{
				//For reference base type, we have to force loading it here.
				EnsureRefTypeLoaded(baseType);
			}
			if (type->Storage == TSM_GLOBAL)
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Global type cannot have base type");
			}
			else if (type->Storage == TSM_INTERFACE)
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Interface cannot have base type");
			}
			else
			{
				if (type->Storage != baseType->Storage)
				{
					throw RuntimeLoaderException(ERR_L_PROGRAM, "Base type storage must be same as the derived type");
				}
			}
		}
		type->BaseType = LoadVirtualTable(type.get(), type.get(), tt->Generic, tt->Base.VirtualFunctions, baseType);
		if (type->Storage == TSM_VALUE && type->BaseType.VirtualFunctions.size() > 0)
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Value type cannot have virtual functions");
		}

		if (type->Storage == TSM_INTERFACE)
		{
			//Within the range of _loadingTypes, we can avoid circular interface inheritance.
			LoadInterfaces(type.get(), type.get(), tt);
		}

		std::size_t offset = 0, totalAlignment = 1;

		//Fields. Handle special case for Core.Embed<T>.
		std::vector<RuntimeType*> fields;
		RuntimeType* fieldSourceType = type.get();
		Type* fieldSourceTypeTemplate = tt;
		if (type->Args.Assembly == "Core" && type->Args.Id == _embedTypeId)
		{
			assert(type->Args.Arguments.IsSingle());
			fieldSourceType = type->Args.Arguments.Get(0, 0);
			fieldSourceTypeTemplate = FindTypeTemplate(fieldSourceType->Args);
		}
		for (auto typeId : fieldSourceTypeTemplate->Fields)
		{
			auto fieldType = LoadRefType({ fieldSourceType, fieldSourceTypeTemplate->Generic }, typeId);
			if (fieldType == nullptr)
			{
				//Only goes here if REF_EMPTY is specified.
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid field type");
			}
			if (fieldType->Storage == TSM_VALUE && fieldType->Alignment == 0)
			{
				assert(std::any_of(_loading->_loadingTypes.begin(), _loading->_loadingTypes.end(),
					[fieldType](RuntimeType* t) { return t == fieldType; }));
				throw RuntimeLoaderException(ERR_L_CIRCULAR, "Circular type dependence");
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
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid field type");
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
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Global type cannot have interfaces");
			}
		}

		if (type->Args.Assembly == "Core" && type->Args.Id == _boxTypeId)
		{
			assert(type->Args.Arguments.IsSingle());
			if (type->Args.Arguments.Get(0, 0)->Storage == TSM_VALUE)
			{
				LoadInterfaces(type.get(), type->Args.Arguments.Get(0, 0), nullptr);
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
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Only global type can have initializer");
			}
		}
		if (type->Storage != TSM_REFERENCE)
		{
			if (type->Finalizer != nullptr)
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Only reference type can have finalizer");
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
			auto paramType = func->References.Types[funcTemplate->Parameters[i].TypeId];
			if (paramType == nullptr)
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Function parameter type cannot be void");
			}
			func->Parameters.push_back(paramType);
		}
#if _DEBUG
		func->Fullname = func->GetFullname();
#endif

		auto ptr = func.get();
		_loading->_finishedLoadingFunctions.emplace_back(std::move(func));
	}

	void FinalCheckType(RuntimeType* type)
	{
		//Special types
		if (type->Args.Assembly == "Core")
		{
			if (type->Args.Id == _pointerTypeId)
			{
				assert(type->Storage == TSM_VALUE);
				assert(type->Args.Arguments.IsSingle());
				auto elementType = type->Args.Arguments.Get(0, 0);
				assert(elementType->PointerType == nullptr);
				elementType->PointerType = type;
			}
			else if (type->Args.Id == _boxTypeId)
			{
				assert(type->Storage == TSM_REFERENCE);
				assert(type->Args.Arguments.IsSingle());
				auto elementType = type->Args.Arguments.Get(0, 0);
				assert(elementType->BoxType == nullptr);
				elementType->BoxType = type;
			}
			else if (type->Args.Id == _referenceTypeId)
			{
				assert(type->Storage == TSM_VALUE);
				assert(type->Args.Arguments.IsSingle());
				auto elementType = type->Args.Arguments.Get(0, 0);
				assert(elementType->ReferenceType == nullptr);
				elementType->ReferenceType = type;
			}
			else if (type->Args.Id == _embedTypeId)
			{
				assert(type->Storage == TSM_VALUE);
				assert(type->Args.Arguments.IsSingle());
				auto elementType = type->Args.Arguments.Get(0, 0);
				assert(elementType->EmbedType == nullptr);
				elementType->EmbedType = type;
			}
		}
		//Type handlers
		if (type->Initializer != nullptr)
		{
			if (type->Initializer->ReturnValue != nullptr ||
				type->Initializer->Parameters.size() != 0)
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid initializer");
			}
		}
		if (type->Finalizer != nullptr)
		{
			if (type->Finalizer->ReturnValue != nullptr ||
				type->Finalizer->Parameters.size() != 1)
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid finalizer");
			}
			if (type->Finalizer->Parameters[0] != type)
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid finalizer");
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
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Interface type not specified");
			}
			if (baseType->Storage != TSM_INTERFACE)
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Interface must be interface storage");
			}

			dest->Interfaces.emplace_back(LoadVirtualTable(src, dest, srcTemplate->Generic,
				i.VirtualFunctions, baseType));
		}
	}

	void EnsureRefTypeLoaded(RuntimeType* f)
	{
		for (auto& type : _loading->_loadingRefTypes)
		{
			if (type.get() == f)
			{
				LoadFields(std::move(type), nullptr);
				break;
			}
		}
	}

	void EnsureFunctionLoaded(RuntimeFunction* f)
	{
		for (auto& func : _loading->_loadingFunctions)
		{
			if (func.get() == f)
			{
				PostLoadFunction(std::move(func));
				break;
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
		if (baseType != nullptr && baseType->Alignment == 0)
		{
			throw RuntimeLoaderException(ERR_L_CIRCULAR, "Circular base type detected");
		}

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
					throw RuntimeLoaderException(ERR_L_PROGRAM, "Virtual table not matching");
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
					throw RuntimeLoaderException(ERR_L_PROGRAM, "Virtual table not matching");
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
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid field reference");
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
		case REF_CONSTRAINT:
		{
			ConstraintExportList* list = &f->ConstraintExportList;
			for (auto& e : *list)
			{
				if (e.EntryType == CONSTRAINT_EXPORT_FIELD && e.Index == index)
				{
					return e.Field;
				}
			}
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid field reference");
		}
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid field reference");
		}
	}
};

}
