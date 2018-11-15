#pragma once
#include "RuntimeLoaderCore.h"

struct RuntimeLoaderRefList : RuntimeLoaderCore
{
public:
	RuntimeType* LoadRefTypeImpl(const LoadingRefArguments& lg, std::size_t typeId)
	{
		if (typeId >= lg.Declaration.Types.size())
		{
			throw RuntimeLoaderException("Invalid type reference");
		}
		auto type = lg.Declaration.Types[typeId];
		LoadingArguments la;
	loadClone:
		switch (type.Type & REF_REFTYPES)
		{
		case REF_EMPTY:
			return nullptr;
		case REF_CLONE:
			if (type.Index >= lg.Declaration.Types.size())
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			typeId = type.Index;
			type = lg.Declaration.Types[type.Index];
			goto loadClone;
		case REF_ASSEMBLY:
			la.Assembly = lg.Arguments.Assembly;
			la.Id = type.Index;
			LoadRefTypeArgList(lg, typeId, la);
			return LoadTypeInternal(la, false);
		case REF_IMPORT:
		{
			auto a = FindAssemblyThrow(lg.Arguments.Assembly);
			if (type.Index >= a->ImportTypes.size())
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			auto i = a->ImportTypes[type.Index];
			if (!FindExportType(i, la))
			{
				throw RuntimeLoaderException("Import type not found");
			}
			LoadRefTypeArgList(lg, typeId, la);
			if (la.Arguments.size() != i.GenericParameters)
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			return LoadTypeInternal(la, false);
		}
		case REF_ARGUMENT:
			if (type.Index >= lg.Arguments.Arguments.size())
			{
				auto aaid = type.Index - lg.Arguments.Arguments.size();
				if (lg.AdditionalArguments == nullptr || aaid >= lg.AdditionalArguments->size())
				{
					throw RuntimeLoaderException("Invalid type reference");
				}
				return (*lg.AdditionalArguments)[aaid];
			}
			return lg.Arguments.Arguments[type.Index];
		case REF_SELF:
			if (lg.SelfType == nullptr)
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			return lg.SelfType;
		case REF_SUBTYPE:
		{
			auto name = lg.Declaration.SubtypeNames[type.Index];
			auto parent = LoadRefType(lg, typeId + 1);
			LoadRefTypeArgList(lg, typeId + 1, la);
			return LoadSubType({ parent, name, la.Arguments });
		}
		case REF_CLONETYPE:
		default:
			throw RuntimeLoaderException("Invalid type reference");
		}
	}

	RuntimeFunction* LoadRefFunctionImpl(const LoadingRefArguments& lg, std::size_t funcId)
	{
		if (funcId >= lg.Declaration.Functions.size())
		{
			throw RuntimeLoaderException("Invalid function reference");
		}
		auto func = lg.Declaration.Functions[funcId];
		LoadingArguments la;
	loadClone:
		switch (func.Type & REF_REFTYPES)
		{
		case REF_EMPTY:
			return nullptr;
		case REF_CLONE:
			if (func.Index >= lg.Declaration.Functions.size())
			{
				throw RuntimeLoaderException("Invalid function reference");
			}
			funcId = func.Index;
			func = lg.Declaration.Functions[func.Index];
			goto loadClone;
		case REF_ASSEMBLY:
			la.Assembly = lg.Arguments.Assembly;
			la.Id = func.Index;
			LoadRefFuncArgList(lg, funcId, la);
			return LoadFunctionInternal(la);
		case REF_IMPORT:
		{
			auto a = FindAssemblyThrow(lg.Arguments.Assembly);
			if (func.Index >= a->ImportFunctions.size())
			{
				throw RuntimeLoaderException("Invalid function reference");
			}
			auto i = a->ImportFunctions[func.Index];
			if (!FindExportFunction(i, la))
			{
				throw RuntimeLoaderException("Import function not found");
			}
			LoadRefFuncArgList(lg, funcId, la);
			if (la.Arguments.size() != i.GenericParameters)
			{
				throw RuntimeLoaderException("Invalid function reference");
			}
			return LoadFunctionInternal(la);
		}
		case REF_CLONETYPE:
			return nullptr;
		case REF_ARGUMENT:
		case REF_SELF:
		case REF_SUBTYPE:
		default:
			throw RuntimeLoaderException("Invalid function reference");
		}
	}

public:
	RuntimeType* LoadSubTypeImpl(const SubtypeLoadingArguments& args)
	{
		for (auto& t : _loading->_loadingSubtypes)
		{
			if (t == args)
			{
				throw RuntimeLoaderException("Cyclic reference in subtype");
			}
		}
		_loading->_loadingSubtypes.push_back(args);
		//TODO move to LoadingData
		if (_loading->_loadingTypes.size() + _loading->_loadingFunctions.size() +
			_loading->_loadingSubtypes.size() > _loadingLimit)
		{
			throw RuntimeLoaderException("Loading object limit exceeded.");
		}

		auto tt = FindTypeTemplate(args.Parent->Args);
		std::size_t id = SIZE_MAX;
		for (auto& n : tt->PublicSubTypes)
		{
			if (n.Name == args.Name)
			{
				id = n.Id;
				break;
			}
		}
		if (id == SIZE_MAX)
		{
			throw RuntimeLoaderException("Subtype name not found");
		}

		LoadingRefArguments la(args.Parent, tt->Generic, args.Arguments);
		auto ret = LoadRefType(la, id);

		assert(_loading->_loadingSubtypes.back() == args);
		_loading->_loadingSubtypes.pop_back();

		return ret;
	}

private:
	void LoadRefTypeArgList(const LoadingRefArguments& lg, std::size_t index, LoadingArguments& la)
	{
		for (std::size_t i = index + 1; i < lg.Declaration.Types.size(); ++i)
		{
			if (lg.Declaration.Types[i].Type == REF_EMPTY) break; //Use REF_Empty as the end of arg list
			la.Arguments.push_back(LoadRefType(lg, i));
		}
	}

	void LoadRefFuncArgList(const LoadingRefArguments& lg, std::size_t index, LoadingArguments& la)
	{
		for (std::size_t i = index + 1; i < lg.Declaration.Functions.size(); ++i)
		{
			if (lg.Declaration.Functions[i].Type == REF_EMPTY) break;
			if (lg.Declaration.Functions[i].Type != REF_CLONETYPE)
			{
				throw RuntimeLoaderException("Invalid generic function argument");
			}
			la.Arguments.push_back(LoadRefType(lg, lg.Declaration.Functions[i].Index));
		}
	}
};

RuntimeType* RuntimeLoaderCore::LoadRefType(const LoadingRefArguments& lg, std::size_t typeId)
{
	auto l = static_cast<RuntimeLoaderRefList*>(this);
	return l->LoadRefTypeImpl(lg, typeId);
}

RuntimeFunction* RuntimeLoaderCore::LoadRefFunction(const LoadingRefArguments& lg, std::size_t typeId)
{
	auto l = static_cast<RuntimeLoaderRefList*>(this);
	return l->LoadRefFunctionImpl(lg, typeId);
}

RuntimeType* RuntimeLoaderCore::LoadSubType(const SubtypeLoadingArguments& args)
{
	auto l = static_cast<RuntimeLoaderRefList*>(this);
	return l->LoadSubTypeImpl(args);
}
