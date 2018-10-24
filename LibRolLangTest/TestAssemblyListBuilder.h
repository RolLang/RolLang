#pragma once

namespace
{
	//TODO support for FR_INST/FR_INSTI
	//TODO add test for import
	class TestAssemblyListBuilder
	{
	public:
		enum TypeReferenceType
		{
			TR_EMPTY,
			TR_ARGUMENT,
			TR_TEMP,
			TR_TEMPI,
			TR_INST,
			TR_INSTI,
		};

		enum FunctionReferenceType
		{
			FR_EMPTY,
			FR_TEMP,
			FR_TEMPI,
			FR_INST,
			FR_INSTI,
		};

		//Note that all references will be invalidated after EndAssembly()
		struct TypeReference
		{
			TypeReferenceType Type;
			std::size_t Id;
			std::vector<TypeReference> Arguments;
		};

		struct FunctionReference
		{
			FunctionReferenceType Type;
			std::size_t Id;
			std::vector<TypeReference> Arguments;
		};

		void BeginAssembly(const std::string& name)
		{
			_assembly = Assembly();
			_assembly.AssemblyName = name;
			_currentType = _currentFunction = SIZE_MAX;
		}

		void EndAssembly()
		{
			_assemblies.emplace_back(std::move(_assembly));
		}

		TypeReference ForwardDeclareType()
		{
			auto id = _assembly.Types.size();
			_assembly.Types.push_back({});
			return { TR_TEMP, id, {} };
		}

		TypeReference ImportType(const std::string& a, const std::string& name)
		{
			auto id = _assembly.ImportTypes.size();
			_assembly.ImportTypes.push_back({ a, name });
			return { TR_TEMPI, id, {} };
		}

		TypeReference BeginType(TypeStorageMode ts, const std::string& name, const TypeReference& r = {})
		{
			if (r.Type == TR_EMPTY)
			{
				auto ret = ForwardDeclareType();
				_currentType = ret.Id;
				_currentName = name;
				_assembly.Types[_currentType].GCMode = ts;
				return ret;
			}
			else if (r.Type == TR_TEMP)
			{
				_currentType = r.Id;
				_currentName = name;
				_assembly.Types[_currentType].GCMode = ts;
				return r;
			}
			return { TR_EMPTY, 0, {} };
		}

		void Link(bool linkExport, bool linkNative)
		{
			if (_currentType != SIZE_MAX)
			{
				if (linkExport)
				{
					_assembly.ExportTypes.push_back({ _currentType, _currentName });
				}
				if (linkNative)
				{
					_assembly.NativeTypes.push_back({ _currentType, _currentName });
				}
			}
			else if (_currentFunction != SIZE_MAX)
			{
				if (linkExport)
				{
					_assembly.ExportFunctions.push_back({ _currentFunction, _currentName });
				}
				if (linkNative)
				{
					_assembly.NativeFunctions.push_back({ _currentFunction, _currentName });
				}
			}
		}

		void AddField(const TypeReference& type)
		{
			auto id = WriteTypeRef(_assembly.Types[_currentType].Generic, type);
			_assembly.Types[_currentType].Fields.push_back(id);
		}

		void SetFinalizer(const FunctionReference& f)
		{
			auto& t = _assembly.Types[_currentType];
			t.OnFinalize = WriteFunctionRef(t.Generic, f);
		}

		void SetFinalizerEmpty()
		{
			FunctionReference f;
			f.Type = FR_EMPTY;
			SetFinalizer(f);
		}

		void EndType()
		{
			_currentType = -1;
			_currentName = "";
		}

		TypeReference AddNativeType(const std::string& name, bool exportType)
		{
			auto ret = BeginType(TSM_VALUE, name, {});
			Link(exportType, true);
			EndType();
			return ret;
		}

		TypeReference AddGenericParameter()
		{
			if (_currentType != SIZE_MAX)
			{
				auto id = _assembly.Types[_currentType].Generic.Parameters.size();
				_assembly.Types[_currentType].Generic.Parameters.push_back({});
				return { TR_ARGUMENT, id, {} };
			}
			else if (_currentFunction != SIZE_MAX)
			{
				auto id = _assembly.Functions[_currentFunction].Generic.Parameters.size();
				_assembly.Functions[_currentFunction].Generic.Parameters.push_back({});
				return { TR_ARGUMENT, id, {} };
			}
			return { TR_EMPTY, 0, {} };
		}

		TypeReference MakeType(const TypeReference& base, std::vector<TypeReference> args)
		{
			if (base.Type == TR_TEMP)
			{
				return { TR_INST, base.Id, std::move(args) };
			}
			else if (base.Type == TR_TEMPI)
			{
				return { TR_INSTI, base.Id, std::move(args) };
			}
			return { TR_EMPTY, 0, {} };
		}

		FunctionReference ForwardDeclareFunction()
		{
			auto id = _assembly.Functions.size();
			_assembly.Functions.push_back({});
			return { FR_TEMP, id, {} };
		}

		FunctionReference BeginFunction(const std::string& name, const FunctionReference& r = {})
		{
			if (r.Type == FR_EMPTY)
			{
				auto ret = ForwardDeclareFunction();
				_currentFunction = ret.Id;
				_currentName = name;
				return ret;
			}
			else if (r.Type == FR_TEMP)
			{
				_currentFunction = r.Id;
				_currentName = name;
				return r;
			}
			return { FR_EMPTY, SIZE_MAX, {} };
		}

		void Signature(TypeReference ret, std::vector<TypeReference> args)
		{
			auto& f = _assembly.Functions[_currentFunction];
			f.ReturnValue.TypeId = WriteTypeRef(f.Generic, ret);
			f.Parameters.clear();
			for (std::size_t i = 0; i < args.size(); ++i)
			{
				f.Parameters.push_back({ WriteTypeRef(f.Generic, args[i]) });
			}
		}

		void AddInstruction(Opcodes opcode, std::uint32_t operand)
		{
			auto& list = _assembly.Functions[_currentFunction].Instruction;
			unsigned char ch = opcode << 3;
			if (operand <= 5)
			{
				list.push_back(ch | (unsigned char)operand);
			}
			else if (operand <= 255)
			{
				list.push_back(ch | 7);
				list.push_back((unsigned char)operand);
			}
			else
			{
				list.push_back(ch | 6);
				list.push_back((unsigned char)(operand >> 24));
				operand &= 0xFFFFFFu;
				list.push_back((unsigned char)(operand >> 16));
				operand &= 0xFFFFu;
				list.push_back((unsigned char)(operand >> 8));
				list.push_back((unsigned char)(operand & 0xFFu));
			}
		}

		template <typename T>
		std::size_t AddFunctionConstant(const TypeReference& type, const T& val)
		{
			auto typeId = WriteTypeRef(_assembly.Functions[_currentFunction].Generic, type);
			unsigned char* ptr = (unsigned char*)&val;
			auto& f = _assembly.Functions[_currentFunction];
			auto offset = f.ConstantData.size();
			auto ret = f.ConstantTable.size();
			f.ConstantData.insert(f.ConstantData.end(), ptr, ptr + sizeof(T));
			f.ConstantTable.push_back({ offset, sizeof(T), typeId });
			return ret;
		}

		std::size_t AddFunctionLocal(const TypeReference& type)
		{
			auto typeId = WriteTypeRef(_assembly.Functions[_currentFunction].Generic, type);
			auto ret = _assembly.Functions[_currentFunction].Locals.size();
			_assembly.Functions[_currentFunction].Locals.push_back({ typeId });
			return ret;
		}

		void EndFunction()
		{
			_currentFunction = SIZE_MAX;
			_currentName = "";
		}

		void AddTypeRef(const TypeReference& t)
		{
			if (_currentType != SIZE_MAX)
			{
				WriteTypeRef(_assembly.Types[_currentType].Generic, t);
			}
			else if (_currentFunction != SIZE_MAX)
			{
				WriteTypeRef(_assembly.Functions[_currentFunction].Generic, t);
			}
		}

		void AddFunctionRef(const FunctionReference& f)
		{
			if (_currentType != SIZE_MAX)
			{
				WriteFunctionRef(_assembly.Types[_currentType].Generic, f);
			}
			else if (_currentFunction != SIZE_MAX)
			{
				WriteFunctionRef(_assembly.Functions[_currentFunction].Generic, f);
			}
		}

	public:
		void WriteCoreCommon(TypeReference* i32, TypeReference* rptr, TypeReference* ptr)
		{
			auto tInt32 = BeginType(TSM_VALUE, "Core.Int32");
			Link(true, true);
			SetFinalizerEmpty();
			EndType();

			auto tRawPtr = BeginType(TSM_VALUE, "Core.RawPtr");
			Link(true, true);
			SetFinalizerEmpty();
			EndType();

			auto tPtr = BeginType(TSM_VALUE, "Core.Pointer");
			AddGenericParameter();
			Link(true, false);
			SetFinalizerEmpty();
			AddField(tRawPtr);
			EndType();

			if (i32) *i32 = tInt32;
			if (rptr) *rptr = tRawPtr;
			if (ptr) *ptr = tPtr;
		}
		
	public:
		AssemblyList Build()
		{
			return { _assemblies };
		}

	private:
		std::size_t WriteTypeRef(GenericDeclaration& g, const TypeReference& t)
		{
			std::vector<std::size_t> args;
			for (std::size_t i = 0; i < t.Arguments.size(); ++i)
			{
				args.push_back(WriteTypeRef(g, t.Arguments[i]));
			}

			std::size_t ret = g.Types.size();
			switch (t.Type)
			{
			case TR_EMPTY:
				g.Types.push_back({ REF_EMPTY, 0 });
				return ret;
			case TR_ARGUMENT:
				g.Types.push_back({ REF_ARGUMENT, t.Id });
				return ret;
			case TR_TEMP:
			case TR_INST:
				g.Types.push_back({ REF_ASSEMBLY, t.Id });
				break;
			case TR_TEMPI:
			case TR_INSTI:
				g.Types.push_back({ REF_IMPORT, t.Id });
				break;
			default:
				return SIZE_MAX;
			}
			for (std::size_t i = 0; i < args.size(); ++i)
			{
				g.Types.push_back({ REF_CLONE, args[i] });
			}
			g.Types.push_back({ REF_EMPTY, 0 });
			return ret;
		}

		std::size_t WriteFunctionRef(GenericDeclaration& g, const FunctionReference& f)
		{
			std::size_t ret = g.Functions.size();
			switch (f.Type)
			{
			case FR_EMPTY:
				g.Functions.push_back({ REF_EMPTY, 0 });
				return ret;
			case FR_TEMP:
			case FR_INST:
				g.Functions.push_back({ REF_ASSEMBLY, f.Id });
				break;
			case FR_TEMPI:
			case FR_INSTI:
				g.Functions.push_back({ REF_IMPORT, f.Id });
				break;
			default:
				return SIZE_MAX;
			}
			for (std::size_t i = 0; i < f.Arguments.size(); ++i)
			{
				g.Functions.push_back({ REF_CLONETYPE, WriteTypeRef(g, f.Arguments[i]) });
			}
			g.Functions.push_back({ REF_EMPTY, 0 });
			return ret;
		}

	private:
		std::vector<Assembly> _assemblies;
		Assembly _assembly;
		std::size_t _currentType;
		std::size_t _currentFunction;
		std::string _currentName;
	};
}
