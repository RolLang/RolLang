#pragma once
#include "RuntimeLoaderCore.h"

struct RuntimeLoaderRefList : RuntimeLoaderCore
{
public:
	//RuntimeType* LoadRefTypeImpl(const LoadingRefArguments& lg, std::size_t typeId)
	bool FindRefTypeImpl(const LoadingRefArguments& lg, std::size_t typeId, LoadingArguments& la)
	{
		if (typeId >= lg.Declaration.Types.size())
		{
			throw RuntimeLoaderException("Invalid type reference");
		}
		auto type = lg.Declaration.Types[typeId];
	loadClone:
		switch (type.Type & REF_REFTYPES)
		{
		case REF_EMPTY:
			return false;
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
			return true;
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
			return true;
		}
		case REF_ARGUMENT:
			if (type.Index >= lg.Arguments.Arguments.size())
			{
				auto aaid = type.Index - lg.Arguments.Arguments.size();
				if (lg.AdditionalArguments == nullptr || aaid >= lg.AdditionalArguments->size())
				{
					throw RuntimeLoaderException("Invalid type reference");
				}
				la = (*lg.AdditionalArguments)[aaid]->Args;
				return true;
			}
			//TODO Improve? Now we have to load again (although it's in the short path)
			//Same for REF_SELF
			la = lg.Arguments.Arguments[type.Index]->Args;
			return true;
		case REF_SELF:
			if (lg.SelfType == nullptr)
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			la = lg.SelfType->Args;
			return true;
		case REF_SUBTYPE:
		{
			auto name = lg.Declaration.SubtypeNames[type.Index];
			auto parent = LoadRefType(lg, typeId + 1);
			LoadRefTypeArgList(lg, typeId + 1, la); //Temporarily use la.Arguments.
			if (!FindSubType({ parent, name, std::move(la.Arguments) }, la)) //Moved. No problem.
			{
				return false;
			}
			return true;
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
	bool FindSubTypeImpl(const SubtypeLoadingArguments& args, LoadingArguments& la)
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

		assert(_loading->_loadingSubtypes.back() == args);
		_loading->_loadingSubtypes.pop_back();

		return FindRefType({ args.Parent, tt->Generic, args.Arguments }, id, la);
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

bool RuntimeLoaderCore::FindRefType(const LoadingRefArguments& lg, std::size_t typeId, LoadingArguments& la)
{
	auto l = static_cast<RuntimeLoaderRefList*>(this);
	return l->FindRefTypeImpl(lg, typeId, la);
}

RuntimeFunction* RuntimeLoaderCore::LoadRefFunction(const LoadingRefArguments& lg, std::size_t typeId)
{
	auto l = static_cast<RuntimeLoaderRefList*>(this);
	return l->LoadRefFunctionImpl(lg, typeId);
}

bool RuntimeLoaderCore::FindSubType(const SubtypeLoadingArguments& args, LoadingArguments& la)
{
	auto l = static_cast<RuntimeLoaderRefList*>(this);
	return l->FindSubTypeImpl(args, la);
}