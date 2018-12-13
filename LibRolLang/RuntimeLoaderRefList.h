#pragma once
#include "RuntimeLoaderCore.h"

namespace RolLang {

struct RuntimeLoaderRefList : RuntimeLoaderCore
{
public:
	struct RefListIteratorData
	{
		const DeclarationReference& Entry;
		std::size_t Index;
		RefListIteratorData(const DeclarationReference& entry, std::size_t i)
			: Entry(entry), Index(i)
		{
		}
	};
	struct RefListArgsList;
	struct RefListArgsIterator
	{
		const RefListArgsList* _parent;
		std::size_t _i;

		RefListArgsIterator(const RefListArgsList* parent, std::size_t i)
			: _parent(parent), _i(i)
		{
			Check();
		}

		bool operator!=(const RefListArgsIterator& rhs) const
		{
			assert(_parent == rhs._parent);
			return _i != rhs._i;
		}

		RefListArgsIterator& operator++()
		{
			if (_i == SIZE_MAX) return *this;
			++_i;
			Check();
			return *this;
		}

		RefListIteratorData operator*() const
		{
			assert(_i != SIZE_MAX);
			return { (*_parent->list)[_parent->head + _i], _parent->head + _i };
		}

	private:
		void Check()
		{
			if (_i == SIZE_MAX) return;
			if (_parent->head + _i >= _parent->list->size())
			{
				throw RuntimeLoaderException("Invalid RefList entry");
			}
			if ((*_parent->list)[_parent->head + _i].Type == REF_LISTEND)
			{
				_i = SIZE_MAX;
			}
		}
	};
	struct RefListArgsList
	{
		const std::vector<DeclarationReference>* list;
		std::size_t head;

		RefListArgsIterator begin() const
		{
			return { this, 1 };
		}

		RefListArgsIterator end() const
		{
			return { this, SIZE_MAX };
		}
	};
	static RefListArgsList GetRefArgList(const std::vector<DeclarationReference>& list, std::size_t head)
	{
		return { &list, head };
	}

public:
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
			//TODO detect circular REF_CLONE
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
			for (auto&& e : GetRefArgList(lg.Declaration.Types, typeId))
			{
				la.Arguments.push_back(LoadRefType(lg, e.Index));
			}
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
			for (auto&& e : GetRefArgList(lg.Declaration.Types, typeId))
			{
				la.Arguments.push_back(LoadRefType(lg, e.Index));
			}
			if (!i.GenericParameters.CanMatch({ la.Arguments.size() }))
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
			auto name = lg.Declaration.NamesList[type.Index];
			auto parent = LoadRefType(lg, typeId + 1);
			if (parent == nullptr)
			{
				throw RuntimeLoaderException("Invalid type reference");
			}
			for (auto&& e : GetRefArgList(lg.Declaration.Types, typeId + 1))
			{
				la.Arguments.push_back(LoadRefType(lg, e.Index));
			}
			if (!FindSubType({ parent, name, std::move(la.Arguments) }, la)) //Moved. No problem.
			{
				return false;
			}
			return true;
		}
		case REF_CONSTRAIN:
		{
			ConstrainExportList* list;
			if (lg.SelfType != nullptr)
			{
				list = &lg.SelfType->ConstrainExportList;
			}
			else
			{
				assert(lg.SelfFunction);
				list = &lg.SelfFunction->ConstrainExportList;
			}
			for (auto& e : *list)
			{
				if (e.EntryType == CONSTRAIN_EXPORT_TYPE && e.Index == typeId)
				{
					la = e.Type->Args;
					return true;
				}
			}
			return false;
		}
		case REF_CLONETYPE:
		case REF_LISTEND:
		default:
			throw RuntimeLoaderException("Invalid type reference");
		}
	}

	bool FindRefFunctionImpl(const LoadingRefArguments& lg, std::size_t funcId, LoadingArguments& la)
	{
		if (funcId >= lg.Declaration.Functions.size())
		{
			throw RuntimeLoaderException("Invalid function reference");
		}
		auto func = lg.Declaration.Functions[funcId];
	loadClone:
		switch (func.Type & REF_REFTYPES)
		{
		case REF_EMPTY:
			return false;
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
			for (auto&& e : GetRefArgList(lg.Declaration.Functions, funcId))
			{
				if (e.Entry.Type != REF_CLONETYPE)
				{
					throw RuntimeLoaderException("Invalid generic function argument");
				}
				la.Arguments.push_back(LoadRefType(lg, e.Entry.Index));
			}
			return true;
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
			for (auto&& e : GetRefArgList(lg.Declaration.Functions, funcId))
			{
				if (e.Entry.Type != REF_CLONETYPE)
				{
					throw RuntimeLoaderException("Invalid generic function argument");
				}
				la.Arguments.push_back(LoadRefType(lg, e.Entry.Index));
			}
			if (!i.GenericParameters.CanMatch({ la.Arguments.size() }))
			{
				throw RuntimeLoaderException("Invalid function reference");
			}
			return true;
		}
		case REF_CONSTRAIN:
		{
			ConstrainExportList* list;
			if (lg.SelfType != nullptr)
			{
				list = &lg.SelfType->ConstrainExportList;
			}
			else
			{
				assert(lg.SelfFunction);
				list = &lg.SelfFunction->ConstrainExportList;
			}
			for (auto& e : *list)
			{
				if (e.EntryType == CONSTRAIN_EXPORT_FUNCTION && e.Index == funcId)
				{
					la = e.Function->Args;
					return true;
				}
			}
			return false;
		}
		case REF_CLONETYPE:
		case REF_ARGUMENT:
		case REF_SELF:
		case REF_SUBTYPE:
		case REF_LISTEND:
		default:
			throw RuntimeLoaderException("Invalid function reference");
		}
	}

public:
	bool FindSubTypeImpl(const SubMemberLoadingArguments& args, LoadingArguments& la)
	{
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

		for (auto& t : _loading->_loadingSubtypes)
		{
			if (t == args)
			{
				throw RuntimeLoaderException("Circular reference in subtype");
			}
		}
		_loading->_loadingSubtypes.push_back(args);
		_loading->CheckLoadingSizeLimit(_loadingLimit);

		//Possible circular reference here.
		auto ret = FindRefType({ args.Parent, tt->Generic, args.Arguments }, id, la);

		assert(_loading->_loadingSubtypes.back() == args);
		_loading->_loadingSubtypes.pop_back();

		return ret;
	}
};

bool RuntimeLoaderCore::FindRefType(const LoadingRefArguments& lg, std::size_t typeId, LoadingArguments& la)
{
	auto l = static_cast<RuntimeLoaderRefList*>(this);
	return l->FindRefTypeImpl(lg, typeId, la);
}

bool RuntimeLoaderCore::FindRefFunction(const LoadingRefArguments& lg, std::size_t funcId, LoadingArguments& la)
{
	auto l = static_cast<RuntimeLoaderRefList*>(this);
	return l->FindRefFunctionImpl(lg, funcId, la);
}

bool RuntimeLoaderCore::FindSubType(const SubMemberLoadingArguments& args, LoadingArguments& la)
{
	auto l = static_cast<RuntimeLoaderRefList*>(this);
	return l->FindSubTypeImpl(args, la);
}

}
