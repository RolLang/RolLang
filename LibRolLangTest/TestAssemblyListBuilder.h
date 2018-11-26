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
			TR_SELF,
			TR_SUBTYPE,
			TR_ANY,
			TR_TRY,
		};

		enum FunctionReferenceType
		{
			FR_EMPTY,
			FR_TEMP,
			FR_TEMPI,
			FR_INST,
			FR_INSTI,
		};

		enum TraitReferenceType
		{
			CR_EMPTY,
			CR_TEMP,
			CR_TEMPI,
		};

		//Note that all references will be invalidated after EndAssembly()
		struct TypeReference
		{
			TypeReferenceType Type;
			std::size_t Id;
			std::vector<TypeReference> Arguments;
			std::string SubtypeName;
		};

		struct FunctionReference
		{
			FunctionReferenceType Type;
			std::size_t Id;
			std::vector<TypeReference> Arguments;
		};

		struct TraitReference
		{
			TraitReferenceType Type;
			std::size_t Id;
		};

		TypeReference SelfType()
		{
			return { TR_SELF, 0, {} };
		}

		TypeReference AnyType()
		{
			return { TR_ANY, 0, {} };
		}

		TypeReference TryType(const TypeReference& t)
		{
			return { TR_TRY, 0, { t } };
		}

		void BeginAssembly(const std::string& name)
		{
			_assembly = Assembly();
			_assembly.AssemblyName = name;
			_currentType = _currentFunction = _currentTrait = SIZE_MAX;
		}

		void ExportConstant(const std::string& name, std::uint32_t val)
		{
			_assembly.ExportConstants.push_back({ val, name });
		}

		std::size_t ImportConstant(const std::string& a, const std::string& n)
		{
			auto ret = _assembly.ImportConstants.size();
			_assembly.ImportConstants.push_back({ a, n, 0 });
			return ret;
		}

		void EndAssembly()
		{
			_assemblies.emplace_back(std::move(_assembly));
		}

		TypeReference ForwardDeclareType()
		{
			auto id = _assembly.Types.size();
			_assembly.Types.emplace_back();
			return { TR_TEMP, id, {} };
		}

		TypeReference ImportType(const std::string& a, const std::string& name, std::size_t nargs = 0)
		{
			auto id = _assembly.ImportTypes.size();
			_assembly.ImportTypes.push_back({ a, name, nargs });
			return { TR_TEMPI, id, {} };
		}

		void ExportType(const std::string& name, std::size_t id)
		{
			_assembly.ExportTypes.push_back({ id, name });
		}

		TypeReference BeginType(TypeStorageMode ts, const std::string& name, const TypeReference& r = {})
		{
			if (r.Type == TR_EMPTY)
			{
				auto ret = ForwardDeclareType();
				_currentType = ret.Id;
				_currentName = name;
				_assembly.Types[_currentType].GCMode = ts;
				InitType(_assembly.Types[_currentType]);
				return ret;
			}
			else if (r.Type == TR_TEMP)
			{
				_currentType = r.Id;
				_currentName = name;
				_assembly.Types[_currentType].GCMode = ts;
				InitType(_assembly.Types[_currentType]);
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
			else if (_currentTrait != SIZE_MAX)
			{
				assert(!linkNative);
				_assembly.ExportTraits.push_back({ _currentTrait, _currentName });
			}
		}

		void AddField(const TypeReference& type, const std::string& name = "")
		{
			auto type_id = WriteTypeRef(_assembly.Types[_currentType].Generic, type);
			auto field_id = _assembly.Types[_currentType].Fields.size();
			_assembly.Types[_currentType].Fields.push_back(type_id);
			if (name.length() > 0)
			{
				_assembly.Types[_currentType].PublicFields.push_back({ name, field_id });
			}
		}

		void AddSubType(const std::string& name, const TypeReference& type)
		{
			auto id = WriteTypeRef(_assembly.Types[_currentType].Generic, type, false);
			_assembly.Types[_currentType].PublicSubTypes.push_back({ name, id });
		}

		void SetTypeHandlers(const FunctionReference& initializer, const FunctionReference& finalizer)
		{
			auto& t = _assembly.Types[_currentType];
			t.Initializer = WriteFunctionRef(t.Generic, initializer);
			t.Finalizer = WriteFunctionRef(t.Generic, finalizer);
		}

		void SetBaseType(const TypeReference& itype, const TypeReference& vtab)
		{
			auto id1 = WriteTypeRef(_assembly.Types[_currentType].Generic, itype);
			auto id2 = WriteTypeRef(_assembly.Types[_currentType].Generic, vtab);
			_assembly.Types[_currentType].Base = { id1, id2 };
		}

		void AddInterface(const TypeReference& itype, const TypeReference& vtab)
		{
			auto id1 = WriteTypeRef(_assembly.Types[_currentType].Generic, itype);
			auto id2 = WriteTypeRef(_assembly.Types[_currentType].Generic, vtab);
			_assembly.Types[_currentType].Interfaces.push_back({ id1, id2 });
		}

		void EndType()
		{
			FinishType();
			_currentType = SIZE_MAX;
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
			auto id = CurrentDeclaration().ParameterCount++;
			return { TR_ARGUMENT, id, {} };
		}

		TypeReference AddAdditionalGenericParameter(std::size_t n)
		{
			auto id = CurrentDeclaration().ParameterCount + n;
			return { TR_ARGUMENT, id, {} };
		}

		void AddConstrain(TypeReference target, const std::vector<TypeReference> args,
			ConstrainType type, std::size_t id)
		{
			GenericConstrain constrain = {};
			constrain.Type = type;
			constrain.Index = id;
			constrain.Target = WriteTypeRef(constrain, target, false);
			for (auto& a : args)
			{
				constrain.Arguments.push_back(WriteTypeRef(constrain, a, false));
			}
			CurrentDeclaration().Constrains.emplace_back(std::move(constrain));
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

		TypeReference MakeSubtype(const TypeReference& parent, const std::string& name,
			std::vector<TypeReference> args)
		{
			args.insert(args.begin(), parent);
			return { TR_SUBTYPE, 0, args, name };
		}

		FunctionReference MakeFunction(const FunctionReference& base, std::vector<TypeReference> args)
		{
			if (base.Type == FR_TEMP)
			{
				return { FR_INST, base.Id, std::move(args) };
			}
			else if (base.Type == FR_TEMPI)
			{
				return { FR_INSTI, base.Id, std::move(args) };
			}
			return { FR_EMPTY, 0, {} };
		}

		FunctionReference ForwardDeclareFunction()
		{
			auto id = _assembly.Functions.size();
			_assembly.Functions.emplace_back();
			return { FR_TEMP, id, {} };
		}

		FunctionReference ImportFunction(const std::string& a, const std::string& name,
			std::size_t nargs = 0)
		{
			auto id = _assembly.ImportFunctions.size();
			_assembly.ImportFunctions.push_back({ a, name, nargs });
			return { FR_TEMPI, id, {} };
		}

		void ExportFunction(const std::string& name, std::size_t id)
		{
			_assembly.ExportFunctions.push_back({ id, name });
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

		std::size_t AddFunctionImportConstant(const TypeReference& type, std::size_t id)
		{
			auto typeId = WriteTypeRef(_assembly.Functions[_currentFunction].Generic, type);
			auto& f = _assembly.Functions[_currentFunction];
			auto ret = f.ConstantTable.size();
			f.ConstantTable.push_back({ id, 0, typeId });
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

		TraitReference ImportTrait(const std::string& a, const std::string& n, std::size_t nargs)
		{
			auto ret = _assembly.ImportTraits.size();
			_assembly.ImportTraits.push_back({ a, n, nargs });
			return { CR_TEMPI, ret };
		}

		TraitReference ForwardDeclareTrait()
		{
			auto ret = _assembly.Traits.size();
			_assembly.Traits.emplace_back();
			return { CR_TEMP, ret };
		}

		TraitReference BeginTrait(const std::string& name, TraitReference r = {})
		{
			if (r.Type == CR_EMPTY)
			{
				auto ret = ForwardDeclareTrait();
				_currentTrait = ret.Id;
				_currentName = name;
				return ret;
			}
			else if (r.Type == CR_TEMP)
			{
				_currentTrait = r.Id;
				_currentName = name;
				return r;
			}
			return {};
		}

		void AddTraitField(const TypeReference& type, const std::string& name, const std::string& export_name)
		{
			auto type_id = WriteTypeRef(_assembly.Traits[_currentTrait].Generic, type);
			_assembly.Traits[_currentTrait].Fields.push_back({ name, type_id, export_name });
		}

		void EndTrait()
		{
			_currentTrait = SIZE_MAX;
			_currentName = "";
		}

		std::size_t AddTypeRef(const TypeReference& t)
		{
			return WriteTypeRef(CurrentDeclaration(), t);
		}

		std::size_t AddFunctionRef(const FunctionReference& f)
		{
			return WriteFunctionRef(CurrentDeclaration(), f);
		}

	private:
		void InitType(Type& t)
		{
			t.Base.InheritedType = SIZE_MAX;
			t.Base.VirtualTableType = SIZE_MAX;
			t.Initializer = SIZE_MAX;
			t.Finalizer = SIZE_MAX;
		}

		void FinishType()
		{
			auto& t = _assembly.Types[_currentType];
			if (t.Base.InheritedType == SIZE_MAX)
			{
				t.Base.InheritedType = WriteTypeRef(t.Generic, {});
			}
			if (t.Base.VirtualTableType == SIZE_MAX)
			{
				t.Base.VirtualTableType = WriteTypeRef(t.Generic, {});
			}
			if (t.Initializer == SIZE_MAX)
			{
				t.Initializer = WriteFunctionRef(t.Generic, {});
			}
			if (t.Finalizer == SIZE_MAX)
			{
				t.Finalizer = WriteFunctionRef(t.Generic, {});
			}
		}

	public:
		void WriteCoreCommon(TypeReference* i32, TypeReference* rptr, TypeReference* ptr)
		{
			auto tInt32 = BeginType(TSM_VALUE, "Core.Int32");
			Link(true, true);
			SetTypeHandlers({}, {});
			EndType();

			auto tRawPtr = BeginType(TSM_VALUE, "Core.RawPtr");
			Link(true, true);
			SetTypeHandlers({}, {});
			EndType();

			auto tPtr = BeginType(TSM_VALUE, "Core.Pointer");
			AddGenericParameter();
			Link(true, false);
			SetTypeHandlers({}, {});
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
		GenericDeclaration& CurrentDeclaration()
		{
			if (_currentType != SIZE_MAX)
			{
				return _assembly.Types[_currentType].Generic;
			}
			else if (_currentFunction != SIZE_MAX)
			{
				return _assembly.Functions[_currentFunction].Generic;
			}
			else
			{
				assert(_currentTrait != SIZE_MAX);
				return _assembly.Traits[_currentTrait].Generic;
			}
		}

		struct ReferenceTypeWriteTarget
		{
			std::vector<DeclarationReference>& Types;
			std::vector<std::string>& SubtypeNames;
			ReferenceTypeWriteTarget(GenericDeclaration& g)
				: Types(g.Types), SubtypeNames(g.SubtypeNames)
			{
			}
			ReferenceTypeWriteTarget(GenericConstrain& constrain)
				: Types(constrain.TypeReferences), SubtypeNames(constrain.SubtypeNames)
			{
			}
		};

		std::size_t WriteTypeRef(ReferenceTypeWriteTarget g, const TypeReference& t, bool forceLoad = true)
		{
			std::vector<std::size_t> args;
			for (std::size_t i = 0; i < t.Arguments.size(); ++i)
			{
				args.push_back(WriteTypeRef(g, t.Arguments[i], false));
			}

			std::size_t ret = g.Types.size();
			switch (t.Type)
			{
			case TR_EMPTY:
				g.Types.push_back({ ForceLoad(REF_EMPTY, forceLoad), 0 });
				return ret;
			case TR_ARGUMENT:
				g.Types.push_back({ ForceLoad(REF_ARGUMENT, forceLoad), t.Id });
				return ret;
			case TR_TEMP:
			case TR_INST:
				g.Types.push_back({ ForceLoad(REF_ASSEMBLY, forceLoad), t.Id });
				break;
			case TR_TEMPI:
			case TR_INSTI:
				g.Types.push_back({ ForceLoad(REF_IMPORT, forceLoad), t.Id });
				break;
			case TR_SELF:
				g.Types.push_back({ ForceLoad(REF_SELF, forceLoad), 0 });
				return ret;
			case TR_SUBTYPE:
			{
				if (t.Id != 0) return SIZE_MAX;
				auto nameid = g.SubtypeNames.size();
				g.SubtypeNames.push_back(t.SubtypeName);
				g.Types.push_back({ ForceLoad(REF_SUBTYPE, forceLoad), nameid });
				break;
			}
			case TR_ANY:
			{
				g.Types.push_back({ ForceLoad(REF_ANY, forceLoad), 0 });
				return ret;
			}
			case TR_TRY:
			{
				if (args.size() != 1) return SIZE_MAX;
				g.Types.push_back({ ForceLoad(REF_TRY, forceLoad), args[0] });
				return ret;
			}
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
				g.Functions.push_back({ ForceLoad(REF_EMPTY, true), 0 });
				return ret;
			case FR_TEMP:
			case FR_INST:
				g.Functions.push_back({ ForceLoad(REF_ASSEMBLY, true), f.Id });
				break;
			case FR_TEMPI:
			case FR_INSTI:
				g.Functions.push_back({ ForceLoad(REF_IMPORT, true), f.Id });
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

		ReferenceType ForceLoad(ReferenceType v, bool f)
		{
			return f ? (ReferenceType)(v | REF_FORCELOAD) : v;
		}

	private:
		std::vector<Assembly> _assemblies;
		Assembly _assembly;
		std::size_t _currentType;
		std::size_t _currentFunction;
		std::size_t _currentTrait;
		std::string _currentName;
	};
}
