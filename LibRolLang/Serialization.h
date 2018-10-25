#pragma once
#include <type_traits>
#include <iostream>
#include <vector>
#include <string>
#include <cassert>

template <typename T>
struct SimpleSerializer
{
	static_assert(std::is_trivially_copy_constructible<T>::value, "Invalid SimpleSerializer type.");

	static void Read(std::istream& s, T& val)
	{
		s.read((char*)&val, sizeof(T));
	}

	static void Write(std::ostream& s, const T& val)
	{
		s.write((const char*)&val, sizeof(T));
	}
};

template <typename T, typename ENABLE = void>
struct Serializer
{
};

template <typename T>
struct Serializer<T, typename std::enable_if<std::is_arithmetic<T>::value>::type> : SimpleSerializer<T>
{
};
static_assert(sizeof(std::size_t) >= 4, "Invalid size of std::size_t");

template <>
struct Serializer<std::size_t>
{
	static void Read(std::istream& s, std::size_t& val)
	{
		std::uint32_t v;
		SimpleSerializer<std::uint32_t>::Read(s, v);
		val = v;
		if (v == 0xFFFFFFFF) val = SIZE_MAX;
	}

	static void Write(std::ostream& s, const std::size_t& val)
	{
		assert(val <= 0xFFFFFFFF);
		std::uint32_t v = (std::uint32_t)val;
		SimpleSerializer<std::uint32_t>::Write(s, v);
	}
};

template <typename T>
struct Serializer<T, typename std::enable_if<std::is_enum<T>::value>::type> : SimpleSerializer<T>
{
};

template <typename T>
struct Serializer<std::vector<T>, void>
{
	static void Read(std::istream& s, std::vector<T>& val)
	{
		std::size_t n;
		Serializer<std::size_t>::Read(s, n);
		val.clear();
		for (std::size_t i = 0; i < n; ++i)
		{
			val.push_back(T());
			Serializer<T>::Read(s, val[i]);
		}
	}

	static void Write(std::ostream& s, const std::vector<T>& val)
	{
		Serializer<std::size_t>::Write(s, val.size());
		for (auto& i : val)
		{
			Serializer<T>::Write(s, i);
		}
	}
};

template <typename T>
struct Serializer<std::basic_string<T>, void>
{
	static void Read(std::istream& s, std::basic_string<T>& val)
	{
		std::size_t n;
		Serializer<std::size_t>::Read(s, n);
		val.clear();
		for (std::size_t i = 0; i < n; ++i)
		{
			T ch;
			Serializer<T>::Read(s, ch);
			val.push_back(ch);
		}
	}

	static void Write(std::ostream& s, const std::basic_string<T>& val)
	{
		Serializer<std::size_t>::Write(s, val.length());
		for (auto& i : val)
		{
			Serializer<T>::Write(s, i);
		}
	}
};

struct FieldSerializer
{
	struct Field
	{
		void (*ReadFunction)(std::istream& s, void* val);
		void (*WriteFunction)(std::ostream& s, const void* val);
		std::size_t FieldOffset;
	};

	template <typename T>
	static void ReadPtr(std::istream& s, void* val)
	{
		Serializer<T>::Read(s, *(T*)val);
	}

	template <typename T>
	static void WritePtr(std::ostream& s, const void* val)
	{
		Serializer<T>::Write(s, *(const T*)val);
	}

	template <typename T>
	static Field Create(T* offset)
	{
		Field f;
		f.ReadFunction = &ReadPtr<T>;
		f.WriteFunction = &WritePtr<T>;
		f.FieldOffset = (std::size_t)offset;
		return f;
	}

	static Field CreateEmpty()
	{
		return { nullptr, nullptr, SIZE_MAX };
	}
};

template <typename T>
struct FieldSerializerFields {};

template <typename T>
struct FieldSerializerImpl
{
	static void Read(std::istream& s, T& val)
	{
		char* ptr = (char*)&val;
		auto f = FieldSerializerFields<T>::GetFields();
		while (f->FieldOffset != SIZE_MAX)
		{
			f->ReadFunction(s, ptr + f->FieldOffset);
			f++;
		}
	}

	static void Write(std::ostream& s, const T& val)
	{
		const char* ptr = (const char*)&val;
		auto f = FieldSerializerFields<T>::GetFields();
		while (f->FieldOffset != SIZE_MAX)
		{
			f->WriteFunction(s, ptr + f->FieldOffset);
			f++;
		}
	}
};

#define FIELD_SERIALIZER_BEGIN(type) \
template <> struct Serializer<type> : FieldSerializerImpl<type> {}; \
template <> \
struct FieldSerializerFields<type> \
{ \
	static FieldSerializer::Field* GetFields() \
	{ \
		static type* nullObject = nullptr; \
		static FieldSerializer::Field ret[] = { \

#define SERIALIZE_FIELD(field) \
			FieldSerializer::Create(&nullObject->field),

#define FIELD_SERIALIZER_END() \
			FieldSerializer::CreateEmpty(), \
		}; \
		return ret; \
	} \
};
