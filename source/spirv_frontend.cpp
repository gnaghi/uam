#include "spirv_frontend.h"

#include <cstdio>
#include <cstring>
#include <cassert>

namespace spirv {

// ============================================================================
// Instruction helpers
// ============================================================================

// Determine if an opcode has a result type and/or result ID.
// Based on SPIR-V spec tables.
static bool opcodeHasResultType(SpvOp op)
{
	switch (op) {
	case SpvOpUndef:
	case SpvOpExtInst:
	case SpvOpTypeVoid:
	case SpvOpTypeBool:
	case SpvOpTypeInt:
	case SpvOpTypeFloat:
	case SpvOpTypeVector:
	case SpvOpTypeMatrix:
	case SpvOpTypeImage:
	case SpvOpTypeSampler:
	case SpvOpTypeSampledImage:
	case SpvOpTypeArray:
	case SpvOpTypeRuntimeArray:
	case SpvOpTypeStruct:
	case SpvOpTypeOpaque:
	case SpvOpTypePointer:
	case SpvOpTypeFunction:
	case SpvOpTypeEvent:
	case SpvOpTypeDeviceEvent:
	case SpvOpTypeReserveId:
	case SpvOpTypeQueue:
	case SpvOpTypePipe:
	case SpvOpTypeForwardPointer:
		// Type-definition opcodes: they have a result ID but no result type
		return false;
	case SpvOpConstantTrue:
	case SpvOpConstantFalse:
	case SpvOpConstant:
	case SpvOpConstantComposite:
	case SpvOpConstantNull:
	case SpvOpConstantSampler:
	case SpvOpSpecConstantTrue:
	case SpvOpSpecConstantFalse:
	case SpvOpSpecConstant:
	case SpvOpSpecConstantComposite:
	case SpvOpSpecConstantOp:
	case SpvOpVariable:
	case SpvOpLoad:
	case SpvOpAccessChain:
	case SpvOpInBoundsAccessChain:
	case SpvOpFunctionCall:
	case SpvOpSampledImage:
	case SpvOpImageSampleImplicitLod:
	case SpvOpImageSampleExplicitLod:
	case SpvOpImageSampleDrefImplicitLod:
	case SpvOpImageSampleDrefExplicitLod:
	case SpvOpImageFetch:
	case SpvOpImageRead:
	case SpvOpImageQuerySizeLod:
	case SpvOpImageQuerySize:
	case SpvOpImageQueryLod:
	case SpvOpImageQueryLevels:
	case SpvOpImageQuerySamples:
	case SpvOpConvertFToU:
	case SpvOpConvertFToS:
	case SpvOpConvertSToF:
	case SpvOpConvertUToF:
	case SpvOpUConvert:
	case SpvOpSConvert:
	case SpvOpFConvert:
	case SpvOpBitcast:
	case SpvOpSNegate:
	case SpvOpFNegate:
	case SpvOpIAdd:
	case SpvOpFAdd:
	case SpvOpISub:
	case SpvOpFSub:
	case SpvOpIMul:
	case SpvOpFMul:
	case SpvOpUDiv:
	case SpvOpSDiv:
	case SpvOpFDiv:
	case SpvOpUMod:
	case SpvOpSRem:
	case SpvOpSMod:
	case SpvOpFRem:
	case SpvOpFMod:
	case SpvOpVectorTimesScalar:
	case SpvOpMatrixTimesScalar:
	case SpvOpVectorTimesMatrix:
	case SpvOpMatrixTimesVector:
	case SpvOpMatrixTimesMatrix:
	case SpvOpOuterProduct:
	case SpvOpDot:
	case SpvOpIAddCarry:
	case SpvOpISubBorrow:
	case SpvOpUMulExtended:
	case SpvOpSMulExtended:
	case SpvOpAny:
	case SpvOpAll:
	case SpvOpIsNan:
	case SpvOpIsInf:
	case SpvOpLogicalEqual:
	case SpvOpLogicalNotEqual:
	case SpvOpLogicalOr:
	case SpvOpLogicalAnd:
	case SpvOpLogicalNot:
	case SpvOpSelect:
	case SpvOpIEqual:
	case SpvOpINotEqual:
	case SpvOpUGreaterThan:
	case SpvOpSGreaterThan:
	case SpvOpUGreaterThanEqual:
	case SpvOpSGreaterThanEqual:
	case SpvOpULessThan:
	case SpvOpSLessThan:
	case SpvOpULessThanEqual:
	case SpvOpSLessThanEqual:
	case SpvOpFOrdEqual:
	case SpvOpFUnordEqual:
	case SpvOpFOrdNotEqual:
	case SpvOpFUnordNotEqual:
	case SpvOpFOrdLessThan:
	case SpvOpFUnordLessThan:
	case SpvOpFOrdGreaterThan:
	case SpvOpFUnordGreaterThan:
	case SpvOpFOrdLessThanEqual:
	case SpvOpFUnordLessThanEqual:
	case SpvOpFOrdGreaterThanEqual:
	case SpvOpFUnordGreaterThanEqual:
	case SpvOpShiftRightLogical:
	case SpvOpShiftRightArithmetic:
	case SpvOpShiftLeftLogical:
	case SpvOpBitwiseOr:
	case SpvOpBitwiseXor:
	case SpvOpBitwiseAnd:
	case SpvOpNot:
	case SpvOpBitFieldInsert:
	case SpvOpBitFieldSExtract:
	case SpvOpBitFieldUExtract:
	case SpvOpBitReverse:
	case SpvOpBitCount:
	case SpvOpDPdx:
	case SpvOpDPdy:
	case SpvOpFwidth:
	case SpvOpDPdxFine:
	case SpvOpDPdyFine:
	case SpvOpFwidthFine:
	case SpvOpDPdxCoarse:
	case SpvOpDPdyCoarse:
	case SpvOpFwidthCoarse:
	case SpvOpPhi:
	case SpvOpVectorExtractDynamic:
	case SpvOpVectorInsertDynamic:
	case SpvOpVectorShuffle:
	case SpvOpCompositeConstruct:
	case SpvOpCompositeExtract:
	case SpvOpCompositeInsert:
	case SpvOpCopyObject:
	case SpvOpTranspose:
	case SpvOpFunctionParameter:
	case SpvOpImageTexelPointer:
	case SpvOpArrayLength:
		return true;
	default:
		// For unknown ops, assume no result type to be safe
		return false;
	}
}

static bool opcodeHasResultId(SpvOp op)
{
	switch (op) {
	// Opcodes with result ID but no result type
	case SpvOpTypeVoid:
	case SpvOpTypeBool:
	case SpvOpTypeInt:
	case SpvOpTypeFloat:
	case SpvOpTypeVector:
	case SpvOpTypeMatrix:
	case SpvOpTypeImage:
	case SpvOpTypeSampler:
	case SpvOpTypeSampledImage:
	case SpvOpTypeArray:
	case SpvOpTypeRuntimeArray:
	case SpvOpTypeStruct:
	case SpvOpTypeOpaque:
	case SpvOpTypePointer:
	case SpvOpTypeFunction:
	case SpvOpTypeEvent:
	case SpvOpTypeDeviceEvent:
	case SpvOpTypeReserveId:
	case SpvOpTypeQueue:
	case SpvOpTypePipe:
	case SpvOpTypeForwardPointer:
	case SpvOpExtInstImport:
	case SpvOpLabel:
	case SpvOpFunction:
		return true;
	default:
		return opcodeHasResultType(op);
	}
}

bool Instruction::hasResultType() const { return opcodeHasResultType(opcode); }
bool Instruction::hasResultId() const { return opcodeHasResultId(opcode); }

uint32_t Instruction::resultType() const
{
	if (hasResultType())
		return words[1];
	return 0;
}

uint32_t Instruction::resultId() const
{
	if (hasResultType())
		return words[2];
	if (hasResultId())
		return words[1];
	return 0;
}

// ============================================================================
// Program
// ============================================================================

Program::~Program()
{
	// allTypes, allConstants, allVariables are stored by value in vectors
	// types[], constants[], variables[] are just pointers into those
}

const Type* Program::getType(uint32_t id) const
{
	if (id < bound && types[id])
		return types[id];
	return nullptr;
}

const Type* Program::getPointeeType(uint32_t ptrTypeId) const
{
	const Type* ptrTy = getType(ptrTypeId);
	if (!ptrTy || ptrTy->kind != TYPE_POINTER)
		return nullptr;
	return getType(ptrTy->elementTypeId);
}

static DecorationData s_emptyDecoration;
static MemberDecorationData s_emptyMemberDecoration;

const DecorationData& Program::getDecoration(uint32_t id) const
{
	if (id < bound)
		return decorations[id];
	return s_emptyDecoration;
}

const MemberDecorationData& Program::getMemberDecoration(uint32_t typeId, uint32_t member) const
{
	auto it = memberDecorations.find(typeId);
	if (it != memberDecorations.end()) {
		auto jt = it->second.find(member);
		if (jt != it->second.end())
			return jt->second;
	}
	return s_emptyMemberDecoration;
}

const Constant* Program::getConstant(uint32_t id) const
{
	if (id < bound && constants[id])
		return constants[id];
	return nullptr;
}

const Variable* Program::getVariable(uint32_t id) const
{
	if (id < bound && variables[id])
		return variables[id];
	return nullptr;
}

uint32_t Program::getTypeByteSize(uint32_t typeId) const
{
	const Type* ty = getType(typeId);
	if (!ty) return 0;

	switch (ty->kind) {
	case TYPE_BOOL:
		return 4; // treated as 32-bit
	case TYPE_INT:
	case TYPE_FLOAT:
		return ty->bitWidth / 8;
	case TYPE_VECTOR:
		return ty->componentCount * getTypeByteSize(ty->elementTypeId);
	case TYPE_MATRIX:
		return ty->componentCount * getTypeByteSize(ty->elementTypeId);
	case TYPE_ARRAY: {
		uint32_t len = getArrayLength(typeId);
		return len * getTypeByteSize(ty->elementTypeId);
	}
	case TYPE_STRUCT: {
		uint32_t size = 0;
		for (size_t i = 0; i < ty->memberTypeIds.size(); i++) {
			auto& md = getMemberDecoration(typeId, (uint32_t)i);
			uint32_t memberSize = getTypeByteSize(ty->memberTypeIds[i]);
			if (md.offset >= 0)
				size = md.offset + memberSize;
			else
				size += memberSize;
		}
		return size;
	}
	case TYPE_POINTER:
		return 8; // 64-bit pointer
	default:
		return 0;
	}
}

uint32_t Program::getArrayLength(uint32_t typeId) const
{
	const Type* ty = getType(typeId);
	if (!ty || ty->kind != TYPE_ARRAY) return 0;
	const Constant* lenConst = getConstant(ty->lengthId);
	if (!lenConst) return 0;
	return lenConst->value.u32;
}

// ============================================================================
// Parser
// ============================================================================

class Parser {
public:
	Parser(const uint32_t* words, size_t wordCount, pipeline_stage stage);
	Program* parse();

private:
	bool parseHeader();
	bool parseInstructions();
	void processInstruction(const Instruction& inst);

	// First pass: types, decorations, constants, variables
	void handleOpName(const Instruction& inst);
	void handleOpMemberName(const Instruction& inst);
	void handleOpEntryPoint(const Instruction& inst);
	void handleOpExecutionMode(const Instruction& inst);
	void handleOpDecorate(const Instruction& inst);
	void handleOpMemberDecorate(const Instruction& inst);
	void handleOpExtInstImport(const Instruction& inst);

	void handleOpTypeVoid(const Instruction& inst);
	void handleOpTypeBool(const Instruction& inst);
	void handleOpTypeInt(const Instruction& inst);
	void handleOpTypeFloat(const Instruction& inst);
	void handleOpTypeVector(const Instruction& inst);
	void handleOpTypeMatrix(const Instruction& inst);
	void handleOpTypeArray(const Instruction& inst);
	void handleOpTypeRuntimeArray(const Instruction& inst);
	void handleOpTypeStruct(const Instruction& inst);
	void handleOpTypePointer(const Instruction& inst);
	void handleOpTypeFunction(const Instruction& inst);
	void handleOpTypeImage(const Instruction& inst);
	void handleOpTypeSampler(const Instruction& inst);
	void handleOpTypeSampledImage(const Instruction& inst);

	void handleOpConstant(const Instruction& inst);
	void handleOpConstantTrue(const Instruction& inst);
	void handleOpConstantFalse(const Instruction& inst);
	void handleOpConstantComposite(const Instruction& inst);
	void handleOpConstantNull(const Instruction& inst);

	void handleOpVariable(const Instruction& inst);

	// Second pass: functions and basic blocks
	void parseFunctions();

	Type& addType(uint32_t id, TypeKind kind);
	Constant& addConstant(uint32_t id);

	const uint32_t* m_words;
	size_t m_wordCount;
	pipeline_stage m_stage;
	Program* m_prog;
};

Parser::Parser(const uint32_t* words, size_t wordCount, pipeline_stage stage)
	: m_words(words), m_wordCount(wordCount), m_stage(stage), m_prog(nullptr)
{
}

Type& Parser::addType(uint32_t id, TypeKind kind)
{
	m_prog->allTypes.push_back(Type());
	Type& ty = m_prog->allTypes.back();
	ty.id = id;
	ty.kind = kind;
	m_prog->types[id] = &m_prog->allTypes.back();
	return ty;
}

Constant& Parser::addConstant(uint32_t id)
{
	m_prog->allConstants.push_back(Constant());
	Constant& c = m_prog->allConstants.back();
	m_prog->constants[id] = &m_prog->allConstants.back();
	return c;
}

Program* Parser::parse()
{
	m_prog = new Program();
	m_prog->rawWords = m_words;
	m_prog->rawWordCount = m_wordCount;
	m_prog->stage = m_stage;

	if (!parseHeader()) {
		delete m_prog;
		return nullptr;
	}

	// Allocate ID-indexed arrays
	m_prog->types.resize(m_prog->bound, nullptr);
	m_prog->decorations.resize(m_prog->bound);
	m_prog->constants.resize(m_prog->bound, nullptr);
	m_prog->variables.resize(m_prog->bound, nullptr);
	m_prog->names.resize(m_prog->bound);

	// Reserve space for types/constants to avoid reallocation invalidating pointers
	m_prog->allTypes.reserve(m_prog->bound);
	m_prog->allConstants.reserve(m_prog->bound);
	m_prog->allVariables.reserve(m_prog->bound);

	if (!parseInstructions()) {
		delete m_prog;
		return nullptr;
	}

	parseFunctions();

	return m_prog;
}

bool Parser::parseHeader()
{
	if (m_wordCount < 5) {
		fprintf(stderr, "SPIR-V binary too small\n");
		return false;
	}

	m_prog->magic = m_words[0];
	if (m_prog->magic != SpvMagicNumber) {
		fprintf(stderr, "Invalid SPIR-V magic number: 0x%08x (expected 0x%08x)\n",
		        m_prog->magic, SpvMagicNumber);
		return false;
	}

	m_prog->version = m_words[1];
	m_prog->generator = m_words[2];
	m_prog->bound = m_words[3];
	// words[4] is reserved (schema)

	return true;
}

bool Parser::parseInstructions()
{
	size_t offset = 5; // skip header

	while (offset < m_wordCount) {
		uint32_t firstWord = m_words[offset];
		uint16_t wordCount = (uint16_t)(firstWord >> SpvWordCountShift);
		SpvOp opcode = (SpvOp)(firstWord & SpvOpCodeMask);

		if (wordCount == 0 || offset + wordCount > m_wordCount) {
			fprintf(stderr, "SPIR-V: invalid instruction at word %zu (opcode %u, wc %u)\n",
			        offset, opcode, wordCount);
			return false;
		}

		Instruction inst;
		inst.opcode = opcode;
		inst.wordCount = wordCount;
		inst.words = &m_words[offset];

		processInstruction(inst);

		m_prog->allInstructions.push_back(inst);

		offset += wordCount;
	}

	return true;
}

void Parser::processInstruction(const Instruction& inst)
{
	switch (inst.opcode) {
	case SpvOpName:            handleOpName(inst); break;
	case SpvOpMemberName:      handleOpMemberName(inst); break;
	case SpvOpEntryPoint:      handleOpEntryPoint(inst); break;
	case SpvOpExecutionMode:   handleOpExecutionMode(inst); break;
	case SpvOpDecorate:        handleOpDecorate(inst); break;
	case SpvOpMemberDecorate:  handleOpMemberDecorate(inst); break;
	case SpvOpExtInstImport:   handleOpExtInstImport(inst); break;

	case SpvOpTypeVoid:        handleOpTypeVoid(inst); break;
	case SpvOpTypeBool:        handleOpTypeBool(inst); break;
	case SpvOpTypeInt:         handleOpTypeInt(inst); break;
	case SpvOpTypeFloat:       handleOpTypeFloat(inst); break;
	case SpvOpTypeVector:      handleOpTypeVector(inst); break;
	case SpvOpTypeMatrix:      handleOpTypeMatrix(inst); break;
	case SpvOpTypeArray:       handleOpTypeArray(inst); break;
	case SpvOpTypeRuntimeArray:handleOpTypeRuntimeArray(inst); break;
	case SpvOpTypeStruct:      handleOpTypeStruct(inst); break;
	case SpvOpTypePointer:     handleOpTypePointer(inst); break;
	case SpvOpTypeFunction:    handleOpTypeFunction(inst); break;
	case SpvOpTypeImage:       handleOpTypeImage(inst); break;
	case SpvOpTypeSampler:     handleOpTypeSampler(inst); break;
	case SpvOpTypeSampledImage:handleOpTypeSampledImage(inst); break;

	case SpvOpConstant:          handleOpConstant(inst); break;
	case SpvOpConstantTrue:      handleOpConstantTrue(inst); break;
	case SpvOpConstantFalse:     handleOpConstantFalse(inst); break;
	case SpvOpConstantComposite: handleOpConstantComposite(inst); break;
	case SpvOpConstantNull:      handleOpConstantNull(inst); break;

	case SpvOpVariable:        handleOpVariable(inst); break;
	default: break;
	}
}

// ============================================================================
// Name / Entry Point / Decorations
// ============================================================================

void Parser::handleOpName(const Instruction& inst)
{
	// OpName %id "name"
	uint32_t id = inst.word(1);
	const char* name = (const char*)&inst.words[2];
	if (id < m_prog->bound)
		m_prog->names[id] = name;
}

void Parser::handleOpMemberName(const Instruction& inst)
{
	// OpMemberName %type member "name"
	// We don't store member names currently, but could add if needed
	(void)inst;
}

void Parser::handleOpEntryPoint(const Instruction& inst)
{
	// OpEntryPoint ExecutionModel %id "name" %interface...
	// word(1)=execution model, word(2)=function id, word(3..n-1)=name, rest=interfaces
	uint32_t funcId = inst.word(2);
	const char* name = (const char*)&inst.words[3];

	// Only capture the entry point matching our stage
	SpvExecutionModel model = (SpvExecutionModel)inst.word(1);
	bool matches = false;
	switch (m_stage) {
	case pipeline_stage_vertex:    matches = (model == SpvExecutionModelVertex); break;
	case pipeline_stage_tess_ctrl: matches = (model == SpvExecutionModelTessellationControl); break;
	case pipeline_stage_tess_eval: matches = (model == SpvExecutionModelTessellationEvaluation); break;
	case pipeline_stage_geometry:  matches = (model == SpvExecutionModelGeometry); break;
	case pipeline_stage_fragment:  matches = (model == SpvExecutionModelFragment); break;
	case pipeline_stage_compute:   matches = (model == SpvExecutionModelGLCompute); break;
	}

	if (!matches)
		return;

	m_prog->entryPointId = funcId;
	m_prog->entryPointName = name;

	// Skip past the null-terminated name string to find interface variables
	size_t nameWords = (strlen(name) + 4) / 4; // +1 for null, +3 for rounding, /4 for words
	size_t ifaceStart = 3 + nameWords;
	for (size_t i = ifaceStart; i < inst.wordCount; i++)
		m_prog->entryPointInterfaces.push_back(inst.word((unsigned)i));
}

void Parser::handleOpExecutionMode(const Instruction& inst)
{
	// OpExecutionMode %entrypoint mode [literals...]
	// Handled during conversion; we store instructions for reference
	(void)inst;
}

void Parser::handleOpDecorate(const Instruction& inst)
{
	// OpDecorate %target decoration [literals...]
	uint32_t target = inst.word(1);
	SpvDecoration decoration = (SpvDecoration)inst.word(2);

	if (target >= m_prog->bound) return;
	DecorationData& dec = m_prog->decorations[target];

	switch (decoration) {
	case SpvDecorationLocation:
		dec.location = (int32_t)inst.word(3);
		break;
	case SpvDecorationBinding:
		dec.binding = (int32_t)inst.word(3);
		break;
	case SpvDecorationDescriptorSet:
		dec.descriptorSet = (int32_t)inst.word(3);
		break;
	case SpvDecorationComponent:
		dec.component = (int32_t)inst.word(3);
		break;
	case SpvDecorationOffset:
		dec.offset = (int32_t)inst.word(3);
		break;
	case SpvDecorationArrayStride:
		dec.arrayStride = (int32_t)inst.word(3);
		break;
	case SpvDecorationMatrixStride:
		dec.matrixStride = (int32_t)inst.word(3);
		break;
	case SpvDecorationBuiltIn:
		dec.builtIn = (SpvBuiltIn)inst.word(3);
		dec.hasBuiltIn = true;
		break;
	case SpvDecorationFlat:
		dec.flat = true;
		break;
	case SpvDecorationNoPerspective:
		dec.noPerspective = true;
		break;
	case SpvDecorationCentroid:
		dec.centroid = true;
		break;
	case SpvDecorationSample:
		dec.sample = true;
		break;
	case SpvDecorationPatch:
		dec.patch = true;
		break;
	case SpvDecorationBlock:
		dec.block = true;
		break;
	case SpvDecorationBufferBlock:
		dec.bufferBlock = true;
		break;
	case SpvDecorationColMajor:
		dec.colMajor = true;
		break;
	case SpvDecorationRowMajor:
		dec.rowMajor = true;
		break;
	default:
		break;
	}
}

void Parser::handleOpMemberDecorate(const Instruction& inst)
{
	// OpMemberDecorate %structType member decoration [literals...]
	uint32_t structType = inst.word(1);
	uint32_t member = inst.word(2);
	SpvDecoration decoration = (SpvDecoration)inst.word(3);

	MemberDecorationData& md = m_prog->memberDecorations[structType][member];

	switch (decoration) {
	case SpvDecorationOffset:
		md.offset = (int32_t)inst.word(4);
		break;
	case SpvDecorationMatrixStride:
		md.matrixStride = (int32_t)inst.word(4);
		break;
	case SpvDecorationBuiltIn:
		md.builtIn = (SpvBuiltIn)inst.word(4);
		md.hasBuiltIn = true;
		break;
	case SpvDecorationColMajor:
		md.colMajor = true;
		break;
	case SpvDecorationRowMajor:
		md.rowMajor = true;
		break;
	default:
		break;
	}
}

void Parser::handleOpExtInstImport(const Instruction& inst)
{
	// OpExtInstImport %result "name"
	uint32_t id = inst.word(1);
	const char* name = (const char*)&inst.words[2];
	m_prog->extInstImports[id] = name;
}

// ============================================================================
// Type parsing
// ============================================================================

void Parser::handleOpTypeVoid(const Instruction& inst)
{
	addType(inst.word(1), TYPE_VOID);
}

void Parser::handleOpTypeBool(const Instruction& inst)
{
	Type& ty = addType(inst.word(1), TYPE_BOOL);
	ty.bitWidth = 1;
}

void Parser::handleOpTypeInt(const Instruction& inst)
{
	// OpTypeInt %result width signedness
	Type& ty = addType(inst.word(1), TYPE_INT);
	ty.bitWidth = inst.word(2);
	ty.signedness = inst.word(3);
}

void Parser::handleOpTypeFloat(const Instruction& inst)
{
	// OpTypeFloat %result width
	Type& ty = addType(inst.word(1), TYPE_FLOAT);
	ty.bitWidth = inst.word(2);
}

void Parser::handleOpTypeVector(const Instruction& inst)
{
	// OpTypeVector %result %componentType componentCount
	Type& ty = addType(inst.word(1), TYPE_VECTOR);
	ty.elementTypeId = inst.word(2);
	ty.componentCount = inst.word(3);
}

void Parser::handleOpTypeMatrix(const Instruction& inst)
{
	// OpTypeMatrix %result %columnType columnCount
	Type& ty = addType(inst.word(1), TYPE_MATRIX);
	ty.elementTypeId = inst.word(2);
	ty.componentCount = inst.word(3);
}

void Parser::handleOpTypeArray(const Instruction& inst)
{
	// OpTypeArray %result %elementType %length
	Type& ty = addType(inst.word(1), TYPE_ARRAY);
	ty.elementTypeId = inst.word(2);
	ty.lengthId = inst.word(3);
}

void Parser::handleOpTypeRuntimeArray(const Instruction& inst)
{
	Type& ty = addType(inst.word(1), TYPE_RUNTIME_ARRAY);
	ty.elementTypeId = inst.word(2);
}

void Parser::handleOpTypeStruct(const Instruction& inst)
{
	// OpTypeStruct %result %member0 %member1 ...
	Type& ty = addType(inst.word(1), TYPE_STRUCT);
	for (uint16_t i = 2; i < inst.wordCount; i++)
		ty.memberTypeIds.push_back(inst.word(i));
}

void Parser::handleOpTypePointer(const Instruction& inst)
{
	// OpTypePointer %result storageClass %type
	Type& ty = addType(inst.word(1), TYPE_POINTER);
	ty.storageClass = (SpvStorageClass)inst.word(2);
	ty.elementTypeId = inst.word(3);
}

void Parser::handleOpTypeFunction(const Instruction& inst)
{
	// OpTypeFunction %result %returnType %param0 %param1 ...
	Type& ty = addType(inst.word(1), TYPE_FUNCTION);
	ty.returnTypeId = inst.word(2);
	for (uint16_t i = 3; i < inst.wordCount; i++)
		ty.paramTypeIds.push_back(inst.word(i));
}

void Parser::handleOpTypeImage(const Instruction& inst)
{
	// OpTypeImage %result %sampledType Dim Depth Arrayed MS Sampled Format
	Type& ty = addType(inst.word(1), TYPE_IMAGE);
	ty.sampledTypeId = inst.word(2);
	ty.dim = (SpvDim)inst.word(3);
	ty.depth = inst.word(4);
	ty.arrayed = inst.word(5);
	ty.multisampled = inst.word(6);
	ty.sampled = inst.word(7);
}

void Parser::handleOpTypeSampler(const Instruction& inst)
{
	addType(inst.word(1), TYPE_SAMPLER);
}

void Parser::handleOpTypeSampledImage(const Instruction& inst)
{
	// OpTypeSampledImage %result %imageType
	Type& ty = addType(inst.word(1), TYPE_SAMPLED_IMAGE);
	ty.elementTypeId = inst.word(2);
}

// ============================================================================
// Constant parsing
// ============================================================================

void Parser::handleOpConstant(const Instruction& inst)
{
	// OpConstant %resultType %result literal...
	uint32_t typeId = inst.word(1);
	uint32_t id = inst.word(2);
	Constant& c = addConstant(id);
	c.typeId = typeId;
	// For 32-bit types, the literal is one word
	c.value.u32 = inst.word(3);
}

void Parser::handleOpConstantTrue(const Instruction& inst)
{
	uint32_t id = inst.word(2);
	Constant& c = addConstant(id);
	c.typeId = inst.word(1);
	c.isTrue = true;
	c.value.u32 = 1;
}

void Parser::handleOpConstantFalse(const Instruction& inst)
{
	uint32_t id = inst.word(2);
	Constant& c = addConstant(id);
	c.typeId = inst.word(1);
	c.isFalse = true;
	c.value.u32 = 0;
}

void Parser::handleOpConstantComposite(const Instruction& inst)
{
	// OpConstantComposite %resultType %result %constituent0 %constituent1 ...
	uint32_t id = inst.word(2);
	Constant& c = addConstant(id);
	c.typeId = inst.word(1);
	c.isComposite = true;
	for (uint16_t i = 3; i < inst.wordCount; i++)
		c.constituents.push_back(inst.word(i));
}

void Parser::handleOpConstantNull(const Instruction& inst)
{
	uint32_t id = inst.word(2);
	Constant& c = addConstant(id);
	c.typeId = inst.word(1);
	c.isNull = true;
	c.value.u32 = 0;
}

// ============================================================================
// Variable parsing
// ============================================================================

void Parser::handleOpVariable(const Instruction& inst)
{
	// OpVariable %resultType %result StorageClass [Initializer]
	m_prog->allVariables.push_back(Variable());
	Variable& var = m_prog->allVariables.back();
	var.typeId = inst.word(1);
	var.id = inst.word(2);
	var.storageClass = (SpvStorageClass)inst.word(3);
	if (inst.wordCount > 4)
		var.initializerId = inst.word(4);
	m_prog->variables[var.id] = &m_prog->allVariables.back();
}

// ============================================================================
// Function / BasicBlock parsing (second pass)
// ============================================================================

void Parser::parseFunctions()
{
	Function* currentFunc = nullptr;
	BasicBlock* currentBB = nullptr;

	for (size_t i = 0; i < m_prog->allInstructions.size(); i++) {
		const Instruction& inst = m_prog->allInstructions[i];

		switch (inst.opcode) {
		case SpvOpFunction: {
			m_prog->functions.push_back(Function());
			currentFunc = &m_prog->functions.back();
			currentFunc->resultTypeId = inst.word(1);
			currentFunc->id = inst.word(2);
			// word(3) = function control
			currentFunc->functionTypeId = inst.word(4);
			currentBB = nullptr;
			break;
		}
		case SpvOpFunctionParameter: {
			if (currentFunc)
				currentFunc->paramIds.push_back(inst.word(2));
			break;
		}
		case SpvOpLabel: {
			if (currentFunc) {
				currentFunc->blocks.push_back(BasicBlock());
				currentBB = &currentFunc->blocks.back();
				currentBB->labelId = inst.word(1);
			}
			break;
		}
		case SpvOpFunctionEnd: {
			currentFunc = nullptr;
			currentBB = nullptr;
			break;
		}
		default: {
			if (currentBB)
				currentBB->instructions.push_back(&m_prog->allInstructions[i]);
			break;
		}
		}
	}
}

// ============================================================================
// Public API
// ============================================================================

Program* program_create(const uint32_t* words, size_t wordCount, pipeline_stage stage)
{
	Parser parser(words, wordCount, stage);
	return parser.parse();
}

} // namespace spirv
