#include "stdafx.h"
#include "../LibRolLang/Serialization.h"

namespace
{
	struct SerializedEmptyData
	{
	};
	struct SerializedData
	{
		int i;
		std::string str;
		std::vector<SerializedData> list;
		SerializedEmptyData e;
	};
}

FIELD_SERIALIZER_BEGIN(SerializedEmptyData)
FIELD_SERIALIZER_END()

FIELD_SERIALIZER_BEGIN(SerializedData)
	SERIALIZE_FIELD(i)
	SERIALIZE_FIELD(str)
	SERIALIZE_FIELD(list)
FIELD_SERIALIZER_END()

inline bool operator== (const SerializedData& a, const SerializedData& b)
{
	if (!(a.i == b.i && a.str == b.str)) return false;
	if (a.list.size() != b.list.size()) return false;
	for (std::size_t i = 0; i < a.list.size(); ++i)
	{
		if (!(a.list[i] == b.list[i])) return false;
	}
	return true;
}

namespace Microsoft {
	namespace VisualStudio {
		namespace CppUnitTestFramework {
			template <typename T>
			inline std::wstring ToString(const std::vector<T>& t)
			{
				if (t.size() == 0) return L"{ }";
				std::wstringstream output;
				output << "{ ";
				bool isFirst = true;
				for (auto& i : t)
				{
					if (!isFirst)
					{
						output << ", ";
					}
					else
					{
						isFirst = false;
					}
					output << ToString(i);
				}
				output << " }";
				return output.str();
			}

			inline std::wstring ToString(const SerializedData& t)
			{
				std::wstringstream output;
				output
					<< "{ "
					<< t.i << ", " << t.str.c_str() << ", " << ToString(t.list)
					<< " }";
				return output.str();
			}
		}
	}
}

namespace LibRolLangTest
{		
	TEST_CLASS(SerializerTest)
	{
	public:
		TEST_METHOD(SerializeSimpleTypes)
		{
			std::ostringstream sout;
			Serializer<bool>::Write(sout, true);
			Serializer<bool>::Write(sout, false);
			Serializer<int>::Write(sout, 100);
			Serializer<unsigned char>::Write(sout, 255);
			Serializer<std::size_t>::Write(sout, 100000);
			Serializer<double>::Write(sout, 120.25);

			auto data = sout.str();

			std::istringstream sin(data);
			{
				bool r;
				Serializer<bool>::Read(sin, r);
				Assert::AreEqual(true, r);
				Serializer<bool>::Read(sin, r);
				Assert::AreEqual(false, r);
			}
			{
				int r;
				Serializer<int>::Read(sin, r);
				Assert::AreEqual(100, r);
			}
			{
				unsigned char r;
				Serializer<unsigned char>::Read(sin, r);
				Assert::AreEqual((unsigned char)255, r);
			}
			{
				std::size_t r;
				Serializer<std::size_t>::Read(sin, r);
				Assert::AreEqual((std::size_t)100000, r);
			}
			{
				double r;
				Serializer<double>::Read(sin, r);
				Assert::AreEqual(120.25, r);
			}
			Assert::AreEqual(data.size(), (std::size_t)sin.tellg());
		}
		
		TEST_METHOD(SerializeInts)
		{
			std::vector<int> list = { 1, 0, 100, 200, -100, INT_MAX, INT_MIN, -1 };

			std::ostringstream sout;
			for (int i : list)
			{
				Serializer<int>::Write(sout, i);
			}

			auto data = sout.str();

			std::istringstream sin(data);
			for (std::size_t i = 0; i < list.size(); ++i)
			{
				int r;
				Serializer<int>::Read(sin, r);
				Assert::AreEqual(list[i], r);
			}
			Assert::AreEqual(data.size(), (std::size_t)sin.tellg());
		}

		TEST_METHOD(SerializeVector)
		{
			std::vector<int> list = { 1, 0, 100, 200, -100, INT_MAX, INT_MIN, -1 };
			
			std::ostringstream sout;
			Serializer<std::vector<int>>::Write(sout, list);

			auto data = sout.str();

			std::istringstream sin(data);
			std::vector<int> r;
			Serializer<std::vector<int>>::Read(sin, r);
			Assert::AreEqual(data.size(), (std::size_t)sin.tellg());

			Assert::AreEqual(list, r);
		}

		TEST_METHOD(SerializeVectorOfVector)
		{
			std::vector<std::vector<char>> list =
			{
				{ 1, 2, 3, 4 },
				{ 0, 127, 0 },
				{},
				{ -128 },
			};

			std::ostringstream sout;
			Serializer<std::vector<std::vector<char>>>::Write(sout, list);

			auto data = sout.str();

			std::istringstream sin(data);
			std::vector<std::vector<char>> r;
			Serializer<std::vector<std::vector<char>>>::Read(sin, r);
			Assert::AreEqual(data.size(), (std::size_t)sin.tellg());

			Assert::AreEqual(list, r);
		}

		TEST_METHOD(SerializeField)
		{
			SerializedData x;
			x.i = 100;
			x.str = "Some string data";
			{
				SerializedData subdata;
				subdata.i = 101;
				subdata.str = "";
				x.list.push_back(subdata);
			}

			std::ostringstream sout;
			Serializer<SerializedData>::Write(sout, x);

			auto data = sout.str();

			std::istringstream sin(data);
			SerializedData r;
			Serializer<SerializedData>::Read(sin, r);
			Assert::AreEqual(data.size(), (std::size_t)sin.tellg());

			Assert::AreEqual(x, r);
		}
	};
}
