#include "Bytecode.hpp"

Chunk::Chunk(std::string chunkName, int fnArity)
    : name(std::move(chunkName)), arity(fnArity), localsCount(0),
      jitFuncInt(nullptr), isJitCompiled(false), jitAttempted(false) {}

size_t Chunk::addConstant(Value val) {
    constants.push_back(std::move(val));
    cachedChunks.push_back(nullptr);
    return constants.size() - 1;
}

void Chunk::emit(uint8_t byte, int line, int col, int len) {
    code.push_back(byte);
    lines.push_back(line);
    columns.push_back(col);
    lengths.push_back(len);
}

void Chunk::emitOp(OpCode op, int line, int col, int len) {
    emit(static_cast<uint8_t>(op), line, col, len);
}

void Chunk::emitShort(uint16_t val, int line, int col, int len) {
    emit(static_cast<uint8_t>((val >> 8) & 0xff), line, col, len);
    emit(static_cast<uint8_t>(val & 0xff), line, col, len);
}

void Chunk::writeConstant(Value val, int line, int col, int len) {
    size_t idx = addConstant(std::move(val));
    emitOp(OpCode::OP_CONSTANT, line, col, len);
    emitShort(static_cast<uint16_t>(idx), line, col, len);
}
