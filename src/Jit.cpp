#include "Jit.hpp"
#include "VM.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

// ==========================================
// ExecutableMemory Implementation
// ==========================================

ExecutableMemory::ExecutableMemory(size_t size) : ptr_(nullptr), size_(size) {
#ifdef _WIN32
    ptr_ = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
    ptr_ = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (ptr_ == MAP_FAILED) {
        ptr_ = nullptr;
    }
#endif
}

ExecutableMemory::~ExecutableMemory() {
    if (ptr_) {
#ifdef _WIN32
        VirtualFree(ptr_, 0, MEM_RELEASE);
#else
        munmap(ptr_, size_);
#endif
        ptr_ = nullptr;
    }
}

void ExecutableMemory::write(const void* data, size_t len) {
    if (ptr_ && len <= size_) {
        std::memcpy(ptr_, data, len);
    }
}

// ==========================================
// X64Emitter Implementation
// ==========================================

X64Emitter::X64Emitter() {
    buffer_.reserve(1024);
}

void X64Emitter::emit8(uint8_t byte) {
    buffer_.push_back(byte);
}

void X64Emitter::emit16(uint16_t word) {
    emit8(static_cast<uint8_t>(word & 0xff));
    emit8(static_cast<uint8_t>((word >> 8) & 0xff));
}

void X64Emitter::emit32(uint32_t dword) {
    emit8(static_cast<uint8_t>(dword & 0xff));
    emit8(static_cast<uint8_t>((dword >> 8) & 0xff));
    emit8(static_cast<uint8_t>((dword >> 16) & 0xff));
    emit8(static_cast<uint8_t>((dword >> 24) & 0xff));
}

void X64Emitter::emit64(uint64_t qword) {
    emit32(static_cast<uint32_t>(qword & 0xffffffff));
    emit32(static_cast<uint32_t>((qword >> 32) & 0xffffffff));
}

void X64Emitter::patch32(size_t offset, int32_t val) {
    if (offset + 4 <= buffer_.size()) {
        buffer_[offset] = static_cast<uint8_t>(val & 0xff);
        buffer_[offset + 1] = static_cast<uint8_t>((val >> 8) & 0xff);
        buffer_[offset + 2] = static_cast<uint8_t>((val >> 16) & 0xff);
        buffer_[offset + 3] = static_cast<uint8_t>((val >> 24) & 0xff);
    }
}

void X64Emitter::emitRex(bool w, bool r, bool x, bool b) {
    uint8_t rex = 0x40 | (w ? 0x08 : 0) | (r ? 0x04 : 0) | (x ? 0x02 : 0) | (b ? 0x01 : 0);
    emit8(rex);
}

void X64Emitter::emitModRM(uint8_t mod, uint8_t reg, uint8_t rm) {
    emit8(((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7));
}

void X64Emitter::emitDisp(int32_t disp) {
    emit32(static_cast<uint32_t>(disp));
}

int X64Emitter::calculateFrameSize(int numLocals, int evalMax) const {
    int totalSlots = 6 + numLocals + evalMax + 4;
    int bytes = totalSlots * 8 + 32;
    // We pushed 5 registers (40 bytes), so rsp % 16 == 8.
    // To ensure rsp % 16 == 0 after sub rsp, frameSize,
    // frameSize % 16 must equal 8!
    int rem = bytes % 16;
    if (rem == 0) {
        bytes += 8;
    } else if (rem != 8) {
        bytes += (24 - rem) % 16;
    }
    return bytes;
}

int X64Emitter::getLocalDisp(int slot) const {
    // Callee pushes (rbx, r12-r15) take 40 bytes.
    // Local 0 is at [rbp - 48], local k at [rbp - (48 + k*8)].
    return 48 + slot * 8;
}

int X64Emitter::getEvalDisp(int depth, int numLocals) const {
    return 48 + (numLocals + depth) * 8;
}

void X64Emitter::emitPrologue(int numLocals, int evalMax, int arity) {
    // push rbp
    emit8(0x55);
    // mov rbp, rsp
    emit8(0x48); emit8(0x89); emit8(0xe5);

    // Callee-saved registers
    emit8(0x53);             // push rbx
    emit8(0x41); emit8(0x54); // push r12
    emit8(0x41); emit8(0x55); // push r13
    emit8(0x41); emit8(0x56); // push r14
    emit8(0x41); emit8(0x57); // push r15

    int frameSize = calculateFrameSize(numLocals, evalMax);
    // sub rsp, frameSize
    emitRex(true, false, false, false);
    emit8(0x81); emit8(0xec);
    emit32(static_cast<uint32_t>(frameSize));

#ifdef _WIN32
    static const Reg argRegs[] = { RCX, RDX, R8, R9 };
#else
    static const Reg argRegs[] = { RDI, RSI, RDX, RCX, R8, R9 };
#endif
    int maxRegArgs = sizeof(argRegs) / sizeof(argRegs[0]);
    for (int i = 0; i < arity && i < maxRegArgs; ++i) {
        movStackReg(getLocalDisp(i), argRegs[i]);
    }

    // Zero-initialize remaining locals
    if (numLocals > arity) {
        movRegImm64(RAX, 0);
        for (int i = arity; i < numLocals; ++i) {
            movStackReg(getLocalDisp(i), RAX);
        }
    }
}

void X64Emitter::emitEpilogue(int frameSize) {
    // add rsp, frameSize
    emitRex(true, false, false, false);
    emit8(0x81); emit8(0xc4);
    emit32(static_cast<uint32_t>(frameSize));

    emit8(0x41); emit8(0x5f); // pop r15
    emit8(0x41); emit8(0x5e); // pop r14
    emit8(0x41); emit8(0x5d); // pop r13
    emit8(0x41); emit8(0x5c); // pop r12
    emit8(0x5b);             // pop rbx
    emit8(0x5d);             // pop rbp
    emit8(0xc3);             // ret
}

void X64Emitter::movRegImm64(Reg dst, int64_t imm) {
    if (imm == 0) {
        // xor dst, dst
        emitRex(true, dst >= 8, false, dst >= 8);
        emit8(0x31);
        emitModRM(3, dst & 7, dst & 7);
    } else {
        // mov dst, imm64
        emitRex(true, false, false, dst >= 8);
        emit8(0xb8 + (dst & 7));
        emit64(static_cast<uint64_t>(imm));
    }
}

void X64Emitter::movRegReg(Reg dst, Reg src) {
    emitRex(true, src >= 8, false, dst >= 8);
    emit8(0x89);
    emitModRM(3, src & 7, dst & 7);
}

void X64Emitter::movStackReg(int dispBytes, Reg src) {
    emitRex(true, src >= 8, false, false);
    emit8(0x89);
    if (dispBytes <= 128) {
        emitModRM(1, src & 7, 5);
        emit8(static_cast<uint8_t>(-dispBytes));
    } else {
        emitModRM(2, src & 7, 5);
        emitDisp(-dispBytes);
    }
}

void X64Emitter::movRegStack(Reg dst, int dispBytes) {
    emitRex(true, dst >= 8, false, false);
    emit8(0x8b);
    if (dispBytes <= 128) {
        emitModRM(1, dst & 7, 5);
        emit8(static_cast<uint8_t>(-dispBytes));
    } else {
        emitModRM(2, dst & 7, 5);
        emitDisp(-dispBytes);
    }
}

void X64Emitter::addRegReg(Reg dst, Reg src) {
    emitRex(true, src >= 8, false, dst >= 8);
    emit8(0x01);
    emitModRM(3, src & 7, dst & 7);
}

void X64Emitter::subRegReg(Reg dst, Reg src) {
    emitRex(true, src >= 8, false, dst >= 8);
    emit8(0x29);
    emitModRM(3, src & 7, dst & 7);
}

void X64Emitter::imulRegReg(Reg dst, Reg src) {
    emitRex(true, dst >= 8, false, src >= 8);
    emit8(0x0f);
    emit8(0xaf);
    emitModRM(3, dst & 7, src & 7);
}

void X64Emitter::idivReg(Reg divisor) {
    // cqo (sign-extend RAX into RDX:RAX)
    emit8(0x48); emit8(0x99);
    // idiv divisor
    emitRex(true, false, false, divisor >= 8);
    emit8(0xf7);
    emitModRM(3, 7, divisor & 7);
}

void X64Emitter::imodReg(Reg divisor) {
    idivReg(divisor);
    // Remainder is in RDX, move to RAX
    movRegReg(RAX, RDX);
}

void X64Emitter::incStack(int dispBytes) {
    emitRex(true, false, false, false);
    emit8(0xff);
    if (dispBytes <= 128) {
        emitModRM(1, 0, 5);
        emit8(static_cast<uint8_t>(-dispBytes));
    } else {
        emitModRM(2, 0, 5);
        emitDisp(-dispBytes);
    }
}

void X64Emitter::decStack(int dispBytes) {
    emitRex(true, false, false, false);
    emit8(0xff);
    if (dispBytes <= 128) {
        emitModRM(1, 1, 5);
        emit8(static_cast<uint8_t>(-dispBytes));
    } else {
        emitModRM(2, 1, 5);
        emitDisp(-dispBytes);
    }
}

void X64Emitter::addStackReg(int dispBytes, Reg src) {
    emitRex(true, src >= 8, false, false);
    emit8(0x01);
    if (dispBytes <= 128) {
        emitModRM(1, src & 7, 5);
        emit8(static_cast<uint8_t>(-dispBytes));
    } else {
        emitModRM(2, src & 7, 5);
        emitDisp(-dispBytes);
    }
}

void X64Emitter::addStackImm(int dispBytes, int64_t imm) {
    movRegImm64(RDX, imm);
    movRegStack(RAX, dispBytes);
    addRegReg(RAX, RDX);
    movStackReg(dispBytes, RAX);
}

void X64Emitter::subStackImm(int dispBytes, int64_t imm) {
    movRegImm64(RDX, imm);
    movRegStack(RAX, dispBytes);
    subRegReg(RAX, RDX);
    movStackReg(dispBytes, RAX);
}

void X64Emitter::cmpRegReg(Reg r1, Reg r2) {
    emitRex(true, r2 >= 8, false, r1 >= 8);
    emit8(0x39);
    emitModRM(3, r2 & 7, r1 & 7);
}

void X64Emitter::cmpStackImm(int dispBytes, int64_t imm) {
    movRegStack(RAX, dispBytes);
    movRegImm64(RDX, imm);
    cmpRegReg(RAX, RDX);
}

void X64Emitter::testRegReg(Reg r1, Reg r2) {
    emitRex(true, r2 >= 8, false, r1 >= 8);
    emit8(0x85);
    emitModRM(3, r2 & 7, r1 & 7);
}

void X64Emitter::setccReg(uint8_t condCode, Reg dst) {
    // setcc dst8
    emit8(0x0f);
    emit8(condCode);
    emitModRM(3, 0, dst & 7);
}

void X64Emitter::movzxReg8(Reg dst) {
    // movzx reg64, reg8
    emitRex(true, dst >= 8, false, dst >= 8);
    emit8(0x0f);
    emit8(0xb6);
    emitModRM(3, dst & 7, dst & 7);
}

void X64Emitter::negReg(Reg dst) {
    emitRex(true, false, false, dst >= 8);
    emit8(0xf7);
    emitModRM(3, 3, dst & 7);
}

void X64Emitter::notReg(Reg dst) {
    testRegReg(dst, dst);
    setccReg(0x94, dst); // sete dst (1 if zero)
    movzxReg8(dst);
}

size_t X64Emitter::emitJmp() {
    emit8(0xe9);
    size_t fixup = currentOffset();
    emit32(0);
    return fixup;
}

size_t X64Emitter::emitJcc(uint8_t condCode) {
    emit8(0x0f);
    emit8(condCode);
    size_t fixup = currentOffset();
    emit32(0);
    return fixup;
}

void X64Emitter::emitCallReg(Reg targetReg) {
    emitRex(false, false, false, targetReg >= 8);
    emit8(0xff);
    emitModRM(3, 2, targetReg & 7);
}

void X64Emitter::emitRet() {
    emit8(0xc3);
}

// ==========================================
// JitCompiler Implementation
// ==========================================

bool JitCompiler::canCompileInt(Chunk* chunk) {
    if (!chunk || chunk->code.empty()) return false;

    for (size_t ip = 0; ip < chunk->code.size();) {
        OpCode op = static_cast<OpCode>(chunk->code[ip++]);
        switch (op) {
            case OpCode::OP_CONSTANT: {
                if (ip + 2 > chunk->code.size()) return false;
                uint16_t idx = static_cast<uint16_t>((chunk->code[ip] << 8) | chunk->code[ip + 1]);
                ip += 2;
                if (idx >= chunk->constants.size()) return false;
                const auto& c = chunk->constants[idx];
                if (!c.isInt() && !c.isBool() && !c.isNil()) {
                    return false;
                }
                break;
            }
            case OpCode::OP_NIL:
            case OpCode::OP_TRUE:
            case OpCode::OP_FALSE:
            case OpCode::OP_POP:
                break;
            case OpCode::OP_GET_LOCAL:
            case OpCode::OP_SET_LOCAL:
                if (ip + 2 > chunk->code.size()) return false;
                ip += 2;
                break;
            case OpCode::OP_GET_GLOBAL: {
                if (ip + 2 > chunk->code.size()) return false;
                ip += 2;
                break;
            }
            case OpCode::OP_ADD:
            case OpCode::OP_SUB:
            case OpCode::OP_MUL:
            case OpCode::OP_DIV:
            case OpCode::OP_MOD:
            case OpCode::OP_NEGATE:
            case OpCode::OP_NOT:
            case OpCode::OP_EQUAL:
            case OpCode::OP_NOT_EQUAL:
            case OpCode::OP_GREATER:
            case OpCode::OP_GREATER_EQUAL:
            case OpCode::OP_LESS:
            case OpCode::OP_LESS_EQUAL:
                break;
            case OpCode::OP_JUMP:
            case OpCode::OP_JUMP_IF_FALSE:
            case OpCode::OP_LOOP:
                if (ip + 2 > chunk->code.size()) return false;
                ip += 2;
                break;
            case OpCode::OP_INC_LOCAL:
            case OpCode::OP_DEC_LOCAL:
                if (ip + 2 > chunk->code.size()) return false;
                ip += 2;
                break;
            case OpCode::OP_ADD_LOCAL_INT:
            case OpCode::OP_SUB_LOCAL_INT:
                if (ip + 4 > chunk->code.size()) return false;
                ip += 4;
                break;
            case OpCode::OP_JUMP_IF_LOCAL_GE_CONST:
            case OpCode::OP_JUMP_IF_LOCAL_GT_CONST:
            case OpCode::OP_JUMP_IF_LOCAL_LE_CONST:
            case OpCode::OP_JUMP_IF_LOCAL_LT_CONST:
                if (ip + 6 > chunk->code.size()) return false;
                ip += 6;
                break;
            case OpCode::OP_FAST_LOOP:
                if (ip + 4 > chunk->code.size()) return false;
                ip += 4;
                break;
            case OpCode::OP_APPEND_LOCAL:
                if (ip + 2 > chunk->code.size()) return false;
                ip += 2;
                break;
            case OpCode::OP_CALL:
                if (ip + 3 > chunk->code.size()) return false;
                ip += 3;
                break;
            case OpCode::OP_RETURN:
                break;
            default:
                return false;
        }
    }
    return true;
}

bool JitCompiler::compileChunk(Chunk* chunk, JitEngine* engine) {
#if !defined(FASTHON_JIT_TARGET_X64)
    return false;
#endif

    if (!chunk || chunk->isJitCompiled) {
        return false;
    }
    bool can = canCompileInt(chunk);
    if (!can) {
        return false;
    }

    chunk->jitAttempted = true;

    X64Emitter emitter;
    int numLocals = std::max(chunk->localsCount, chunk->arity);
    int evalMax = 64;
    int frameSize = emitter.calculateFrameSize(numLocals, evalMax);

    emitter.emitPrologue(numLocals, evalMax, chunk->arity);

    std::vector<size_t> bcToNative(chunk->code.size() + 16, 0);

    struct Fixup {
        size_t nativePos;
        size_t targetBcIp;
    };
    std::vector<Fixup> fixups;
    std::vector<size_t> epilogueJumps;

    int evalDepth = 0;
    const auto& code = chunk->code;
    size_t ip = 0;

#ifdef _WIN32
    static const X64Emitter::Reg argRegs[] = { X64Emitter::RCX, X64Emitter::RDX, X64Emitter::R8, X64Emitter::R9 };
#else
    static const X64Emitter::Reg argRegs[] = { X64Emitter::RDI, X64Emitter::RSI, X64Emitter::RDX, X64Emitter::RCX, X64Emitter::R8, X64Emitter::R9 };
#endif
    const int maxRegArgs = sizeof(argRegs) / sizeof(argRegs[0]);

    while (ip < code.size()) {
        size_t curBcIp = ip;
        bcToNative[curBcIp] = emitter.currentOffset();

        OpCode op = static_cast<OpCode>(code[ip++]);
        switch (op) {
            case OpCode::OP_CONSTANT: {
                uint16_t idx = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                int64_t val = chunk->constants[idx].asInt();
                emitter.movRegImm64(X64Emitter::RAX, val);
                emitter.movStackReg(emitter.getEvalDisp(evalDepth, numLocals), X64Emitter::RAX);
                evalDepth++;
                break;
            }
            case OpCode::OP_NIL:
            case OpCode::OP_FALSE: {
                emitter.movRegImm64(X64Emitter::RAX, 0);
                emitter.movStackReg(emitter.getEvalDisp(evalDepth, numLocals), X64Emitter::RAX);
                evalDepth++;
                break;
            }
            case OpCode::OP_TRUE: {
                emitter.movRegImm64(X64Emitter::RAX, 1);
                emitter.movStackReg(emitter.getEvalDisp(evalDepth, numLocals), X64Emitter::RAX);
                evalDepth++;
                break;
            }
            case OpCode::OP_POP: {
                if (evalDepth > 0) {
                    evalDepth--;
                }
                break;
            }
            case OpCode::OP_GET_LOCAL: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                emitter.movRegStack(X64Emitter::RAX, emitter.getLocalDisp(slot));
                emitter.movStackReg(emitter.getEvalDisp(evalDepth, numLocals), X64Emitter::RAX);
                evalDepth++;
                break;
            }
            case OpCode::OP_SET_LOCAL: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                if (evalDepth > 0) {
                    emitter.movRegStack(X64Emitter::RAX, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.movStackReg(emitter.getLocalDisp(slot), X64Emitter::RAX);
                }
                break;
            }
            case OpCode::OP_GET_GLOBAL: {
                // Ignore function identifiers pushed before call
                ip += 2;
                break;
            }
            case OpCode::OP_ADD: {
                if (evalDepth >= 2) {
                    evalDepth--;
                    emitter.movRegStack(X64Emitter::RDX, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.movRegStack(X64Emitter::RAX, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.addRegReg(X64Emitter::RAX, X64Emitter::RDX);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), X64Emitter::RAX);
                }
                break;
            }
            case OpCode::OP_SUB: {
                if (evalDepth >= 2) {
                    evalDepth--;
                    emitter.movRegStack(X64Emitter::RDX, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.movRegStack(X64Emitter::RAX, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.subRegReg(X64Emitter::RAX, X64Emitter::RDX);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), X64Emitter::RAX);
                }
                break;
            }
            case OpCode::OP_MUL: {
                if (evalDepth >= 2) {
                    evalDepth--;
                    emitter.movRegStack(X64Emitter::RDX, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.movRegStack(X64Emitter::RAX, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.imulRegReg(X64Emitter::RAX, X64Emitter::RDX);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), X64Emitter::RAX);
                }
                break;
            }
            case OpCode::OP_DIV: {
                if (evalDepth >= 2) {
                    evalDepth--;
                    emitter.movRegStack(X64Emitter::RBX, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.movRegStack(X64Emitter::RAX, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.idivReg(X64Emitter::RBX);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), X64Emitter::RAX);
                }
                break;
            }
            case OpCode::OP_MOD: {
                if (evalDepth >= 2) {
                    evalDepth--;
                    emitter.movRegStack(X64Emitter::RBX, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.movRegStack(X64Emitter::RAX, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.imodReg(X64Emitter::RBX);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), X64Emitter::RAX);
                }
                break;
            }
            case OpCode::OP_NEGATE: {
                if (evalDepth >= 1) {
                    emitter.movRegStack(X64Emitter::RAX, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.negReg(X64Emitter::RAX);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), X64Emitter::RAX);
                }
                break;
            }
            case OpCode::OP_NOT: {
                if (evalDepth >= 1) {
                    emitter.movRegStack(X64Emitter::RAX, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.notReg(X64Emitter::RAX);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), X64Emitter::RAX);
                }
                break;
            }
            case OpCode::OP_EQUAL:
            case OpCode::OP_NOT_EQUAL:
            case OpCode::OP_LESS:
            case OpCode::OP_LESS_EQUAL:
            case OpCode::OP_GREATER:
            case OpCode::OP_GREATER_EQUAL: {
                if (evalDepth >= 2) {
                    evalDepth--;
                    emitter.movRegStack(X64Emitter::RDX, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.movRegStack(X64Emitter::RAX, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.cmpRegReg(X64Emitter::RAX, X64Emitter::RDX);

                    uint8_t cc = 0x94; // sete
                    if (op == OpCode::OP_NOT_EQUAL) cc = 0x95;
                    else if (op == OpCode::OP_LESS) cc = 0x9c;
                    else if (op == OpCode::OP_LESS_EQUAL) cc = 0x9e;
                    else if (op == OpCode::OP_GREATER) cc = 0x9f;
                    else if (op == OpCode::OP_GREATER_EQUAL) cc = 0x9d;

                    emitter.setccReg(cc, X64Emitter::RAX);
                    emitter.movzxReg8(X64Emitter::RAX);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), X64Emitter::RAX);
                }
                break;
            }
            case OpCode::OP_JUMP: {
                uint16_t offset = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                size_t targetBcIp = ip + offset;
                size_t fixup = emitter.emitJmp();
                fixups.push_back({fixup, targetBcIp});
                break;
            }
            case OpCode::OP_JUMP_IF_FALSE: {
                uint16_t offset = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                size_t targetBcIp = ip + offset;
                if (evalDepth > 0) {
                    emitter.movRegStack(X64Emitter::RAX, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.testRegReg(X64Emitter::RAX, X64Emitter::RAX);
                    size_t fixup = emitter.emitJcc(0x84); // jz
                    fixups.push_back({fixup, targetBcIp});
                }
                break;
            }
            case OpCode::OP_LOOP: {
                uint16_t offset = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                size_t targetBcIp = ip - offset;
                size_t fixup = emitter.emitJmp();
                fixups.push_back({fixup, targetBcIp});
                break;
            }
            case OpCode::OP_INC_LOCAL: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                emitter.incStack(emitter.getLocalDisp(slot));
                break;
            }
            case OpCode::OP_DEC_LOCAL: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                emitter.decStack(emitter.getLocalDisp(slot));
                break;
            }
            case OpCode::OP_ADD_LOCAL_INT: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                uint16_t constIdx = static_cast<uint16_t>((code[ip + 2] << 8) | code[ip + 3]);
                ip += 4;
                int64_t imm = chunk->constants[constIdx].asInt();
                emitter.addStackImm(emitter.getLocalDisp(slot), imm);
                break;
            }
            case OpCode::OP_SUB_LOCAL_INT: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                uint16_t constIdx = static_cast<uint16_t>((code[ip + 2] << 8) | code[ip + 3]);
                ip += 4;
                int64_t imm = chunk->constants[constIdx].asInt();
                emitter.subStackImm(emitter.getLocalDisp(slot), imm);
                break;
            }
            case OpCode::OP_JUMP_IF_LOCAL_GE_CONST:
            case OpCode::OP_JUMP_IF_LOCAL_GT_CONST:
            case OpCode::OP_JUMP_IF_LOCAL_LE_CONST:
            case OpCode::OP_JUMP_IF_LOCAL_LT_CONST: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                uint16_t constIdx = static_cast<uint16_t>((code[ip + 2] << 8) | code[ip + 3]);
                uint16_t offset = static_cast<uint16_t>((code[ip + 4] << 8) | code[ip + 5]);
                ip += 6;
                int64_t imm = chunk->constants[constIdx].asInt();
                size_t targetBcIp = ip + offset;

                emitter.cmpStackImm(emitter.getLocalDisp(slot), imm);
                uint8_t cc = 0x8d; // jge
                if (op == OpCode::OP_JUMP_IF_LOCAL_GT_CONST) cc = 0x8f; // jg
                else if (op == OpCode::OP_JUMP_IF_LOCAL_LE_CONST) cc = 0x8e; // jle
                else if (op == OpCode::OP_JUMP_IF_LOCAL_LT_CONST) cc = 0x8c; // jl

                size_t fixup = emitter.emitJcc(cc);
                fixups.push_back({fixup, targetBcIp});
                break;
            }
            case OpCode::OP_FAST_LOOP: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                uint16_t offset = static_cast<uint16_t>((code[ip + 2] << 8) | code[ip + 3]);
                ip += 4;
                size_t targetBcIp = ip - offset;
                emitter.decStack(emitter.getLocalDisp(slot));
                emitter.movRegStack(X64Emitter::RAX, emitter.getLocalDisp(slot));
                emitter.movRegImm64(X64Emitter::RDX, 0);
                emitter.cmpRegReg(X64Emitter::RAX, X64Emitter::RDX);
                size_t fixup = emitter.emitJcc(0x8f); // jg
                fixups.push_back({fixup, targetBcIp});
                break;
            }
            case OpCode::OP_APPEND_LOCAL: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                if (evalDepth > 0) {
                    evalDepth--;
                    emitter.movRegStack(X64Emitter::RAX, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.addStackReg(emitter.getLocalDisp(slot), X64Emitter::RAX);
                }
                break;
            }
            case OpCode::OP_CALL: {
                uint16_t fnIdx = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                uint8_t argCount = code[ip++];

                Chunk* targetChunk = (fnIdx < chunk->cachedChunks.size()) ? chunk->cachedChunks[fnIdx] : nullptr;
                if (!targetChunk && chunk->constants[fnIdx].strVal == chunk->name) {
                    targetChunk = chunk;
                }

                // Move arguments from eval stack into call registers
                for (int i = 0; i < argCount && i < maxRegArgs; ++i) {
                    int slotDisp = emitter.getEvalDisp(evalDepth - argCount + i, numLocals);
                    emitter.movRegStack(argRegs[i], slotDisp);
                }

                if (targetChunk) {
                    // Indirect call via targetChunk->jitFuncInt pointer
                    void* slotAddress = reinterpret_cast<void*>(&targetChunk->jitFuncInt);
                    emitter.movRegImm64(X64Emitter::R10, reinterpret_cast<int64_t>(slotAddress));
                    // mov rax, [r10]
                    emitter.emit8(0x49); emitter.emit8(0x8b); emitter.emit8(0x02);
                    emitter.emitCallReg(X64Emitter::RAX);
                } else {
                    return false;
                }

                evalDepth -= argCount;
                emitter.movStackReg(emitter.getEvalDisp(evalDepth, numLocals), X64Emitter::RAX);
                evalDepth++;
                break;
            }
            case OpCode::OP_RETURN: {
                if (evalDepth > 0) {
                    emitter.movRegStack(X64Emitter::RAX, emitter.getEvalDisp(evalDepth - 1, numLocals));
                } else {
                    emitter.movRegImm64(X64Emitter::RAX, 0);
                }
                size_t jmpPos = emitter.emitJmp();
                epilogueJumps.push_back(jmpPos);
                break;
            }
            default:
                return false;
        }
    }

    size_t epilogueOffset = emitter.currentOffset();
    emitter.emitEpilogue(frameSize);

    // Patch all jumps
    for (const auto& f : fixups) {
        size_t targetNative = (f.targetBcIp >= chunk->code.size()) ? epilogueOffset : bcToNative[f.targetBcIp];
        int32_t rel = static_cast<int32_t>(targetNative) - static_cast<int32_t>(f.nativePos + 4);
        emitter.patch32(f.nativePos, rel);
    }

    for (size_t epJmp : epilogueJumps) {
        int32_t rel = static_cast<int32_t>(epilogueOffset) - static_cast<int32_t>(epJmp + 4);
        emitter.patch32(epJmp, rel);
    }

    void* codePtr = engine->allocateCode(emitter.getCode());
    if (codePtr) {
        chunk->jitFuncInt = codePtr;
        chunk->isJitCompiled = true;
        return true;
    }

    return false;
}

// ==========================================
// JitEngine Implementation
// ==========================================

JitEngine::JitEngine() : enabled_(true), compiledCount_(0), totalBytes_(0) {}

JitEngine::~JitEngine() {
    pages_.clear();
}

JitEngine& JitEngine::instance() {
    static JitEngine inst;
    return inst;
}

void* JitEngine::allocateCode(const std::vector<uint8_t>& code) {
    if (code.empty()) return nullptr;

    size_t pageSize = ((code.size() + 4095) / 4096) * 4096;
    auto page = std::make_unique<ExecutableMemory>(pageSize);
    if (!page->isValid()) return nullptr;

    page->write(code.data(), code.size());
    void* ptr = page->getPointer();
    pages_.push_back(std::move(page));

    compiledCount_++;
    totalBytes_ += code.size();
    return ptr;
}

void JitEngine::compileAllFunctions(const std::unordered_map<std::string, std::shared_ptr<Chunk>>& fns) {
    if (!enabled_) return;

    for (const auto& kv : fns) {
        compileFunction(kv.second.get());
    }
}

bool JitEngine::compileFunction(Chunk* chunk) {
    if (!enabled_ || !chunk || chunk->isJitCompiled) return false;
    return JitCompiler::compileChunk(chunk, this);
}

void JitEngine::printJitInfo() const {
    std::cout << "FasthonJIT Engine Information:\n";
#if defined(FASTHON_JIT_TARGET_X64)
    std::cout << "  Architecture: x86_64 (Direct Native Machine Code JIT)\n";
    std::cout << "  JIT Mode: Silicon Level Direct Assembly Execution\n";
#else
    std::cout << "  Architecture: Portable Direct Dispatch\n";
#endif
    std::cout << "  JIT Status: " << (enabled_ ? "Enabled (Active)" : "Disabled") << "\n";
    std::cout << "  Compiled Functions: " << compiledCount_ << "\n";
    std::cout << "  Total JIT Code Size: " << totalBytes_ << " bytes\n";
}
