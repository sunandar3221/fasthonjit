#pragma once

#include "Value.hpp"
#include <vector>
#include <cstdint>
#include <string>

enum class OpCode : uint8_t {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_NEGATE,
    OP_NOT,
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_RETURN,
    OP_PRINT,
    OP_SILENT_PRINT,
    OP_READ,
    OP_WRITE,
    OP_APP,
    OP_WINDOW,
    OP_RUN,
    OP_NEW,
    OP_GET_PROP,
    OP_SET_PROP,
    OP_BUILD_LIST,
    OP_GET_INDEX,
    OP_SET_INDEX,
    OP_USE,
    OP_INC_LOCAL,
    OP_DEC_LOCAL,
    OP_ADD_LOCAL_INT,
    OP_SUB_LOCAL_INT,
    OP_JUMP_IF_LOCAL_GE_CONST,
    OP_JUMP_IF_LOCAL_GT_CONST,
    OP_JUMP_IF_LOCAL_LE_CONST,
    OP_JUMP_IF_LOCAL_LT_CONST,
    OP_FAST_LOOP,
    OP_APPEND_LOCAL
};

class Chunk {
public:
    std::string name;
    int arity;
    int localsCount;
    std::vector<uint8_t> code;
    std::vector<Value> constants;
    std::vector<Chunk*> cachedChunks;
    std::unordered_map<std::string, int> localIndices;
    std::vector<int> lines;
    std::vector<int> columns;
    std::vector<int> lengths;

    void* jitFuncInt;
    bool isJitCompiled;
    bool jitAttempted;

    Chunk(std::string chunkName = "", int fnArity = 0);

    size_t addConstant(Value val);
    void emit(uint8_t byte, int line = 1, int col = 1, int len = 1);
    void emitOp(OpCode op, int line = 1, int col = 1, int len = 1);
    void emitShort(uint16_t val, int line = 1, int col = 1, int len = 1);
    void writeConstant(Value val, int line = 1, int col = 1, int len = 1);
};
