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
	template <typename T> struct RefListArgsList;
	template <typename T> struct RefListArgsIterator
	{
		const RefListArgsList<T>* _parent;
		std::size_t _i;

		RefListArgsIterator(const RefListArgsList<T>* parent, std::size_t i)
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
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid RefList entry");
			}
			if ((*_parent->list)[_parent->head + _i].Type == REF_LISTEND)
			{
				_i = SIZE_MAX;
				return;
			}
			if ((*_parent->list)[_parent->head + _i].Type == REF_SEGMENT)
			{
				if (_parent->target != nullptr)
				{
					_parent->target->NewList();
				}
				++*this;
			}
			if (_parent->target != nullptr)
			{
				if (_parent->target->IsEmpty())
				{
					throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid RefList entry");
				}
			}
		}
	};
	template <typename T>
	struct RefListArgsList
	{
		const std::vector<DeclarationReference>* list;
		std::size_t head;
		MultiList<T>* target;

		RefListArgsIterator<T> begin() const
		{
			return { this, 1 };
		}

		RefListArgsIterator<T> end() const
		{
			return { this, SIZE_MAX };
		}
	};
	template <typename T>
	static RefListArgsList<T> GetRefArgList(const std::vector<DeclarationReference>& list, std::size_t head,
		MultiList<T>& target)
	{
		return { &list, head, &target };
	}
	template <typename T>
	static const T& GetRefArgument(const std::vector<DeclarationReference>& list, std::size_t head,
		const MultiList<T>& target)
	{
		assert((list[head].Type & REF_REFTYPES) == REF_ARGUMENT);
		if (head + 1 >= list.size() || list[head + 1].Type != REF_ARGUMENTSEG)
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid RefList entry");
		}
		const T* ret;
		if (!target.TryGetRef(list[head + 1].Index, list[head].Index, &ret))
		{
			throw RuntimeLoaderException(ERR_L_GENERIC, "Invalid RefList entry");
		}
		return *ret;
	}
	template <typename T>
	static const T& GetRefArgument(const std::vector<DeclarationReference>& list, std::size_t head,
		const MultiList<T>& target, const MultiList<T>* targetAdditional)
	{
		assert((list[head].Type & REF_REFTYPES) == REF_ARGUMENT);
		if (head + 1 >= list.size() || list[head + 1].Type != REF_ARGUMENTSEG)
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid RefList entry");
		}
		const T* ret;
		if (!target.TryGetRef(list[head + 1].Index, list[head].Index, &ret))
		{
			auto s = target.GetSizeList().size();
			if (targetAdditional == nullptr ||
				!targetAdditional->TryGetRef(list[head + 1].Index - s, list[head].Index, &ret))
			{
				throw RuntimeLoaderException(ERR_L_GENERIC, "Invalid RefList entry");
			}
		}
		return *ret;
	}

public:
	bool FindRefTypeImpl(const LoadingRefArguments& lg, std::size_t typeId, LoadingArguments& la)
	{
		if (typeId >= lg.Declaration.Types.size())
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
		}
		auto type = lg.Declaration.Types[typeId];
	loadClone:
		switch (type.Type & REF_REFTYPES)
		{
		case REF_EMPTY:
			la = LoadingArguments::Empty();
			return true;
		case REF_CLONE:
			//TODO detect circular REF_CLONE
			if (type.Index >= lg.Declaration.Types.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
			}
			typeId = type.Index;
			type = lg.Declaration.Types[type.Index];
			goto loadClone;
		case REF_ASSEMBLY:
			la.Assembly = lg.Arguments.Assembly;
			la.Id = type.Index;
			for (auto&& e : GetRefArgList(lg.Declaration.Types, typeId, la.Arguments))
			{
				la.Arguments.AppendLast(LoadRefType(lg, e.Index));
			}
			return true;
		case REF_IMPORT:
		{
			auto a = FindAssemblyThrow(lg.Arguments.Assembly);
			if (type.Index >= a->ImportTypes.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
			}
			auto i = a->ImportTypes[type.Index];
			if (!FindExportType(i, la))
			{
				throw RuntimeLoaderException(ERR_L_LINK, "Import type not found");
			}
			for (auto&& e : GetRefArgList(lg.Declaration.Types, typeId, la.Arguments))
			{
				la.Arguments.AppendLast(LoadRefType(lg, e.Index));
			}
			return true;
		}
		case REF_ARGUMENT:
		{
			auto argType = GetRefArgument(lg.Declaration.Types, typeId,
				lg.Arguments.Arguments, lg.AdditionalArguments);
			if (argType == nullptr)
			{
				la = LoadingArguments::Empty();
				return true;
			}
			la = argType->Args;
			return true;
		}
		case REF_SELF:
			if (lg.SelfType == nullptr)
			{
				//Note that void cannot be self type. This always indicates an error.
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
			}
			la = lg.SelfType->Args;
			return true;
		case REF_SUBTYPE:
		{
			if (type.Index >= lg.Declaration.NamesList.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
			}
			auto name = lg.Declaration.NamesList[type.Index];
			auto parent = LoadRefType(lg, typeId + 1);
			if (parent == nullptr)
			{
				return false;
			}
			for (auto&& e : GetRefArgList(lg.Declaration.Types, typeId + 1, la.Arguments))
			{
				la.Arguments.AppendLast(LoadRefType(lg, e.Index));
			}
			//la.Arguments is moved to temp obj before entering FindSubType.
			if (!FindSubType({ parent, name, std::move(la.Arguments) }, la))
			{
				return false;
			}
			return true;
		}
		case REF_CONSTRAINT:
		{
			ConstraintExportList* list;
			if (lg.SelfType != nullptr)
			{
				list = &lg.SelfType->ConstraintExportList;
			}
			else
			{
				assert(lg.SelfFunction);
				list = &lg.SelfFunction->ConstraintExportList;
			}
			for (auto& e : *list)
			{
				if (e.EntryType == CONSTRAINT_EXPORT_TYPE && e.Index == typeId)
				{
					la = e.Type->Args;
					return true;
				}
			}
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid REF_CONSTRAINT reference");
		}
		case REF_CLONETYPE:
		case REF_LISTEND:
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid type reference");
		}
	}

	bool FindRefFunctionImpl(const LoadingRefArguments& lg, std::size_t funcId, LoadingArguments& la)
	{
		if (funcId >= lg.Declaration.Functions.size())
		{
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
		}
		auto func = lg.Declaration.Functions[funcId];
	loadClone:
		switch (func.Type & REF_REFTYPES)
		{
		case REF_EMPTY:
			la = LoadingArguments::Empty();
			return true;
		case REF_CLONE:
			if (func.Index >= lg.Declaration.Functions.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
			}
			funcId = func.Index;
			func = lg.Declaration.Functions[func.Index];
			goto loadClone;
		case REF_ASSEMBLY:
			la.Assembly = lg.Arguments.Assembly;
			la.Id = func.Index;
			for (auto&& e : GetRefArgList(lg.Declaration.Functions, funcId, la.Arguments))
			{
				if (e.Entry.Type != REF_CLONETYPE)
				{
					throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid generic function argument");
				}
				la.Arguments.AppendLast(LoadRefType(lg, e.Entry.Index));
			}
			return true;
		case REF_IMPORT:
		{
			auto a = FindAssemblyThrow(lg.Arguments.Assembly);
			if (func.Index >= a->ImportFunctions.size())
			{
				throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
			}
			auto i = a->ImportFunctions[func.Index];
			if (!FindExportFunction(i, la))
			{
				throw RuntimeLoaderException(ERR_L_LINK, "Import function not found");
			}
			for (auto&& e : GetRefArgList(lg.Declaration.Functions, funcId, la.Arguments))
			{
				if (e.Entry.Type != REF_CLONETYPE)
				{
					throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid generic function argument");
				}
				la.Arguments.AppendLast(LoadRefType(lg, e.Entry.Index));
			}
			return true;
		}
		case REF_CONSTRAINT:
		{
			ConstraintExportList* list;
			if (lg.SelfType != nullptr)
			{
				list = &lg.SelfType->ConstraintExportList;
			}
			else
			{
				assert(lg.SelfFunction);
				list = &lg.SelfFunction->ConstraintExportList;
			}
			for (auto& e : *list)
			{
				if (e.EntryType == CONSTRAINT_EXPORT_FUNCTION && e.Index == funcId)
				{
					la = e.Function->Args;
					return true;
				}
			}
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid REF_CONSTRAINT reference");
		}
		case REF_CLONETYPE:
		case REF_ARGUMENT:
		case REF_SELF:
		case REF_SUBTYPE:
		case REF_LISTEND:
		default:
			throw RuntimeLoaderException(ERR_L_PROGRAM, "Invalid function reference");
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
			return false;
		}

		for (auto& t : _loading->_loadingSubtypes)
		{
			if (t == args)
			{
				throw RuntimeLoaderException(ERR_L_CIRCULAR, "Circular reference in subtype");
			}
		}

		auto check = LoadingStackScopeGuard<SubMemberLoadingArguments>(_loading->_loadingSubtypes, args);
		_loading->CheckLoadingSizeLimit();

		return FindRefType({ args.Parent, tt->Generic, args.Arguments }, id, la);
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
