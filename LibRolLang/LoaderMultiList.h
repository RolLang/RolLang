#pragma once
#include <vector>
#include <functional>

namespace RolLang {

template <typename T>
class MultiList final
{
public:
	MultiList() = default;
	MultiList(const std::vector<T>& list)
		: _list(list), _size({ list.size() })
	{
	}
	MultiList(const std::vector<std::vector<T>>& list)
	{
		for (auto&& a : list)
		{
			_list.insert(_list.end(), a.begin(), a.end());
			_size.push_back(a.size());
		}
	}
	MultiList(const MultiList<T>& list)
		: _list(list._list), _size(list._size)
	{
	}
	MultiList(MultiList<T>&& list)
		: _list(std::move(list._list)), _size(std::move(list._size))
	{
	}

	T Get(std::size_t seg, std::size_t index) const
	{
		return _list[GetSegmentStart(seg) + index];
	}

	T& GetRef(std::size_t seg, std::size_t index)
	{
		return _list[GetSegmentStart(seg) + index];
	}

	const T& GetRef(std::size_t seg, std::size_t index) const
	{
		return _list[GetSegmentStart(seg) + index];
	}

	bool TryGetRef(std::size_t seg, std::size_t index, const T**r) const
	{
		if (seg >= _size.size()) return false;
		auto i = GetSegmentStart(seg) + index;
		if (i >= _list.size()) return false;
		*r = &_list[i];
		return true;
	}

	const std::vector<std::size_t>& GetSizeList() const
	{
		return _size;
	}

	std::size_t GetTotalSize() const
	{
		return GetSegmentStart(_size.size());
	}

	std::vector<T> GetList(std::size_t seg) const
	{
		std::vector<T> ret;
		auto begin = GetSegmentStart(seg);
		auto end = GetSegmentStart(seg + 1);
		ret.insert(ret.end(), _list.begin() + begin, _list.begin() + end);
		return ret;
	}

	bool IsEmpty() const
	{
		return _list.size() == 0 && _size.size() == 0;
	}

	bool IsSingle() const
	{
		return _size.size() == 1 && _list.size() == 1;
	}

	MultiList& operator=(const MultiList& list)
	{
		_list = std::vector<T>(list._list);
		_size = std::vector<std::size_t>(list._size);
		return *this;
	}

	MultiList& operator=(MultiList&& list)
	{
		_list = std::vector<T>(std::move(list._list));
		_size = std::vector<std::size_t>(std::move(list._size));
		return *this;
	}

	bool operator== (const MultiList& rhs) const
	{
		return _size == rhs._size && _list == rhs._list;
	}

	bool operator!= (const MultiList& rhs) const
	{
		return !(*this == rhs);
	}

	void NewList()
	{
		_size.push_back(0);
	}

	void AppendLast(T&& val)
	{
		assert(!IsEmpty());
		_list.emplace_back(std::move(val));
		_size[_size.size() - 1] += 1;
	}

	void AppendLast(const T& val)
	{
		assert(!IsEmpty());
		_list.push_back(val);
		_size[_size.size() - 1] += 1;
	}

	//TODO check the use of GetSizeList for iteration and use this functio if possible
	template <typename T1, typename F>
	void CopyList(MultiList<T1>& target, F f)
	{
		for (std::size_t i = 0; i < _size.size(); ++i)
		{
			target.NewList();
			for (std::size_t j = 0; j < _size[i]; ++j)
			{
				target.AppendLast(f(Get(i, j)));
			}
		}
	}
	
public:
	struct AllItemIterator
	{
		AllItemIterator(MultiList<T>* parent, std::size_t seg)
			: _parent(parent), _seg(seg), _i(0)
		{
			while (_seg < parent->_size.size() && parent->_size[_seg] == 0)
			{
				_seg += 1;
			}
		}

		bool operator== (const AllItemIterator& rhs) const
		{
			return _parent == rhs._parent && _seg == rhs._seg && _i == rhs._i;
		}

		bool operator!= (const AllItemIterator& rhs) const
		{
			return !(*this == rhs);
		}

		AllItemIterator& operator++()
		{
			if (++_i >= _parent->_size[_seg])
			{
				_i = 0;
				_seg += 1;
			}
			return *this;
		}

		T& operator*() const
		{
			return _parent->GetRef(_seg, _i);
		}

	private:
		MultiList<T>* _parent;
		std::size_t _seg, _i;
	};
	struct AllItem
	{
		AllItem(MultiList<T>* parent) : _parent(parent) {}

		AllItemIterator begin()
		{
			return { _parent, 0 };
		}

		AllItemIterator end()
		{
			return { _parent, _parent->_size.size() };
		}

	private:
		MultiList<T>* _parent;
	};

	AllItem GetAll()
	{
		return { this };
	}

private:
	std::size_t GetSegmentStart(std::size_t seg) const
	{
		std::size_t totalSize = 0;
		for (std::size_t i = 0; i < seg; ++i)
		{
			totalSize += _size[i];
		}
		return totalSize;
	}

private:
	std::vector<T> _list;
	std::vector<std::size_t> _size;
};

}
