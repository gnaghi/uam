#pragma once

#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <map>
#include <string>

#include "spirv/spirv.h"
#include "spirv/GLSL.std.450.h"
#include "glsl_frontend.h" // for pipeline_stage

// Forward declarations
struct nv50_ir_prog_info;

namespace spirv {

// ============================================================================
// Type representation
// ============================================================================

enum TypeKind {
	TYPE_VOID,
	TYPE_BOOL,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_VECTOR,
	TYPE_MATRIX,
	TYPE_ARRAY,
	TYPE_RUNTIME_ARRAY,
	TYPE_STRUCT,
	TYPE_POINTER,
	TYPE_FUNCTION,
	TYPE_IMAGE,
	TYPE_SAMPLER,
	TYPE_SAMPLED_IMAGE,
};

struct Type {
	TypeKind kind;
	uint32_t id;

	// For int/float: bit width; for bool: 1
	uint32_t bitWidth;
	// For int: 0=unsigned, 1=signed
	uint32_t signedness;
	// For vector/matrix: component count
	uint32_t componentCount;
	// For vector/matrix/array/pointer: element type ID
	uint32_t elementTypeId;
	// For array: length (constant ID)
	uint32_t lengthId;
	// For pointer: storage class
	SpvStorageClass storageClass;
	// For struct: member type IDs
	std::vector<uint32_t> memberTypeIds;
	// For function: return type + param types
	uint32_t returnTypeId;
	std::vector<uint32_t> paramTypeIds;
	// For image
	uint32_t sampledTypeId;
	SpvDim dim;
	uint32_t depth;
	uint32_t arrayed;
	uint32_t multisampled;
	uint32_t sampled;

	Type() : kind(TYPE_VOID), id(0), bitWidth(0), signedness(0),
	         componentCount(0), elementTypeId(0), lengthId(0),
	         storageClass(SpvStorageClassFunction), returnTypeId(0),
	         sampledTypeId(0), dim(SpvDim1D), depth(0), arrayed(0),
	         multisampled(0), sampled(0) {}
};

// ============================================================================
// Decoration data
// ============================================================================

struct DecorationData {
	int32_t location;
	int32_t binding;
	int32_t descriptorSet;
	int32_t component;
	int32_t offset;
	int32_t arrayStride;
	int32_t matrixStride;
	SpvBuiltIn builtIn;
	bool hasBuiltIn;
	bool flat;
	bool noPerspective;
	bool centroid;
	bool sample;
	bool patch;
	bool block;
	bool bufferBlock;
	bool colMajor;
	bool rowMajor;

	DecorationData() : location(-1), binding(-1), descriptorSet(-1),
	                   component(-1), offset(-1), arrayStride(-1),
	                   matrixStride(-1), builtIn(SpvBuiltInMax),
	                   hasBuiltIn(false), flat(false),
	                   noPerspective(false), centroid(false),
	                   sample(false), patch(false), block(false),
	                   bufferBlock(false), colMajor(false), rowMajor(false) {}
};

struct MemberDecorationData {
	int32_t offset;
	int32_t matrixStride;
	SpvBuiltIn builtIn;
	bool hasBuiltIn;
	bool colMajor;
	bool rowMajor;

	MemberDecorationData() : offset(-1), matrixStride(-1),
	                         builtIn(SpvBuiltInMax), hasBuiltIn(false),
	                         colMajor(false), rowMajor(false) {}
};

// ============================================================================
// Constant value
// ============================================================================

struct Constant {
	uint32_t typeId;
	union {
		uint32_t u32;
		int32_t  i32;
		float    f32;
	} value;
	std::vector<uint32_t> constituents; // for OpConstantComposite
	bool isComposite;
	bool isNull;
	bool isTrue;
	bool isFalse;

	Constant() : typeId(0), isComposite(false), isNull(false),
	             isTrue(false), isFalse(false) { value.u32 = 0; }
};

// ============================================================================
// Variable
// ============================================================================

struct Variable {
	uint32_t id;
	uint32_t typeId;        // pointer type ID
	SpvStorageClass storageClass;
	uint32_t initializerId; // 0 if none

	Variable() : id(0), typeId(0), storageClass(SpvStorageClassFunction),
	             initializerId(0) {}
};

// ============================================================================
// SPIR-V instruction (thin wrapper over words)
// ============================================================================

struct Instruction {
	SpvOp opcode;
	uint16_t wordCount;
	const uint32_t* words; // points into the original binary

	uint32_t word(unsigned i) const { return words[i]; }
	uint32_t resultType() const;
	uint32_t resultId() const;
	bool hasResultType() const;
	bool hasResultId() const;
};

// ============================================================================
// Function / BasicBlock representation for second pass
// ============================================================================

struct BasicBlock {
	uint32_t labelId;
	std::vector<const Instruction*> instructions;
};

struct Function {
	uint32_t id;
	uint32_t resultTypeId;
	uint32_t functionTypeId;
	std::vector<uint32_t> paramIds;
	std::vector<BasicBlock> blocks;
};

// ============================================================================
// Parsed SPIR-V program
// ============================================================================

struct Program {
	// Header info
	uint32_t magic;
	uint32_t version;
	uint32_t generator;
	uint32_t bound;

	pipeline_stage stage;

	// ID-indexed lookups (sized by 'bound')
	std::vector<Type*> types;              // ID → Type (null if not a type)
	std::vector<DecorationData> decorations; // ID → decorations
	std::vector<Constant*> constants;      // ID → Constant (null if not a constant)
	std::vector<Variable*> variables;      // ID → Variable (null if not a variable)
	std::vector<std::string> names;        // ID → debug name

	// Member decorations: structTypeId → member index → decoration
	std::map<uint32_t, std::map<uint32_t, MemberDecorationData>> memberDecorations;

	// All types/variables/functions owned here
	std::vector<Type> allTypes;
	std::vector<Constant> allConstants;
	std::vector<Variable> allVariables;
	std::vector<Function> functions;

	// The entry point function ID
	uint32_t entryPointId;
	std::string entryPointName;
	std::vector<uint32_t> entryPointInterfaces; // interface variable IDs

	// Extended instruction set imports (ID → name)
	std::map<uint32_t, std::string> extInstImports;

	// All parsed instructions (for second pass)
	std::vector<Instruction> allInstructions;

	// Raw words
	const uint32_t* rawWords;
	size_t rawWordCount;

	Program() : magic(0), version(0), generator(0), bound(0),
	            stage(pipeline_stage_vertex), entryPointId(0),
	            rawWords(nullptr), rawWordCount(0) {}
	~Program();

	// Helpers
	const Type* getType(uint32_t id) const;
	const Type* getPointeeType(uint32_t ptrTypeId) const;
	const DecorationData& getDecoration(uint32_t id) const;
	const MemberDecorationData& getMemberDecoration(uint32_t typeId, uint32_t member) const;
	const Constant* getConstant(uint32_t id) const;
	const Variable* getVariable(uint32_t id) const;
	uint32_t getTypeByteSize(uint32_t typeId) const;
	uint32_t getArrayLength(uint32_t typeId) const;
};

// ============================================================================
// Public API
// ============================================================================

// Parse a SPIR-V binary. Returns nullptr on failure.
Program* program_create(const uint32_t* words, size_t wordCount, pipeline_stage stage);

} // namespace spirv
