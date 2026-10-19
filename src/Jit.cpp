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
#if defined(__aarch64__) || defined(_M_ARM64)
        char* start = static_cast<char*>(ptr_);
        char* end = start + len;
        __builtin___clear_cache(start, end);
#endif
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
    int totalSlots = numLocals + evalMax;
    int bytes = totalSlots * 8;
#ifdef _WIN32
    bytes += 32; // Shadow space for Windows x64 ABI
#endif
    return ((bytes + 15) / 16) * 16;
}

int X64Emitter::getLocalDisp(int slot) const {
#ifdef _WIN32
    return 32 + slot * 8;
#else
    return slot * 8;
#endif
}

int X64Emitter::getEvalDisp(int depth, int numLocals) const {
#ifdef _WIN32
    return 32 + (numLocals + depth) * 8;
#else
    return (numLocals + depth) * 8;
#endif
}

void X64Emitter::emitPrologue(int numLocals, int evalMax, int arity) {
    emit8(0x55); // push rbp
    emitRex(true, false, false, false);
    emit8(0x89);
    emitModRM(3, 4, 5); // mov rbp, rsp

    int frameSize = calculateFrameSize(numLocals, evalMax);
    emitRex(true, false, false, false);
    emit8(0x81);
    emitModRM(3, 5, 4); // sub rsp, frameSize
    emit32(frameSize);

#ifdef _WIN32
    static const Reg inArgRegs[] = { RCX, RDX, R8, R9 };
#else
    static const Reg inArgRegs[] = { RDI, RSI, RDX, RCX, R8, R9 };
#endif
    int maxRegs = sizeof(inArgRegs) / sizeof(inArgRegs[0]);
    int toCopy = std::min(arity, maxRegs);
    for (int i = 0; i < toCopy; ++i) {
        int disp = getLocalDisp(i);
        movStackReg(disp, inArgRegs[i]);
    }
}

void X64Emitter::emitEpilogue(int frameSize) {
    emitRex(true, false, false, false);
    emit8(0x81);
    emitModRM(3, 0, 4); // add rsp, frameSize
    emit32(frameSize);

    emit8(0x5d); // pop rbp
    emit8(0xc3); // ret
}

void X64Emitter::movRegImm64(Reg dst, int64_t imm) {
    emitRex(true, false, false, dst >= 8);
    emit8(0xb8 + (dst & 7));
    emit64(static_cast<uint64_t>(imm));
}

void X64Emitter::movRegReg(Reg dst, Reg src) {
    emitRex(true, src >= 8, false, dst >= 8);
    emit8(0x89);
    emitModRM(3, src & 7, dst & 7);
}

void X64Emitter::movStackReg(int stackSlot, Reg src) {
    emitRex(true, src >= 8, false, false);
    emit8(0x89);
    emitModRM(2, src & 7, 4);
    emit8(0x24);
    emitDisp(stackSlot);
}

void X64Emitter::movRegStack(Reg dst, int stackSlot) {
    emitRex(true, dst >= 8, false, false);
    emit8(0x8b);
    emitModRM(2, dst & 7, 4);
    emit8(0x24);
    emitDisp(stackSlot);
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
    emitRex(true, false, false, false);
    emit8(0x99); // cqo (sign-extend RAX into RDX:RAX)
    emitRex(true, false, false, divisor >= 8);
    emit8(0xf7);
    emitModRM(3, 7, divisor & 7); // idiv divisor
}

void X64Emitter::imodReg(Reg divisor) {
    idivReg(divisor);
    movRegReg(RAX, RDX); // result in RAX
}

void X64Emitter::incStack(int stackSlot) {
    emitRex(true, false, false, false);
    emit8(0xff);
    emitModRM(2, 0, 4);
    emit8(0x24);
    emitDisp(stackSlot);
}

void X64Emitter::decStack(int stackSlot) {
    emitRex(true, false, false, false);
    emit8(0xff);
    emitModRM(2, 1, 4);
    emit8(0x24);
    emitDisp(stackSlot);
}

void X64Emitter::addStackReg(int stackSlot, Reg src) {
    emitRex(true, src >= 8, false, false);
    emit8(0x01);
    emitModRM(2, src & 7, 4);
    emit8(0x24);
    emitDisp(stackSlot);
}

void X64Emitter::addStackImm(int stackSlot, int64_t imm) {
    emitRex(true, false, false, false);
    emit8(0x81);
    emitModRM(2, 0, 4);
    emit8(0x24);
    emitDisp(stackSlot);
    emit32(static_cast<int32_t>(imm));
}

void X64Emitter::subStackImm(int stackSlot, int64_t imm) {
    emitRex(true, false, false, false);
    emit8(0x81);
    emitModRM(2, 5, 4);
    emit8(0x24);
    emitDisp(stackSlot);
    emit32(static_cast<int32_t>(imm));
}

void X64Emitter::cmpRegReg(Reg r1, Reg r2) {
    emitRex(true, r2 >= 8, false, r1 >= 8);
    emit8(0x39);
    emitModRM(3, r2 & 7, r1 & 7); // cmp r1, r2
}

void X64Emitter::cmpStackImm(int stackSlot, int64_t imm) {
    emitRex(true, false, false, false);
    emit8(0x81);
    emitModRM(2, 7, 4);
    emit8(0x24);
    emitDisp(stackSlot);
    emit32(static_cast<int32_t>(imm));
}

void X64Emitter::testRegReg(Reg r1, Reg r2) {
    emitRex(true, r2 >= 8, false, r1 >= 8);
    emit8(0x85);
    emitModRM(3, r2 & 7, r1 & 7);
}

void X64Emitter::setccReg(uint8_t condCode, Reg dst) {
    emitRex(false, false, false, dst >= 8);
    emit8(0x0f);
    emit8(0x90 | (condCode & 0x0f));
    emitModRM(3, 0, dst & 7);
}

void X64Emitter::movzxReg8(Reg dst) {
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
    movRegReg(RDX, dst);
    movRegImm64(dst, 0);
    testRegReg(RDX, RDX);
    setccReg(4, dst); // sete (zf=1)
    movzxReg8(dst);
}

size_t X64Emitter::emitJmp() {
    emit8(0xe9);
    size_t pos = buffer_.size();
    emit32(0);
    return pos;
}

size_t X64Emitter::emitJcc(uint8_t condCode) {
    emit8(0x0f);
    emit8(condCode);
    size_t pos = buffer_.size();
    emit32(0);
    return pos;
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
// Arm64Emitter Implementation
// ==========================================

Arm64Emitter::Arm64Emitter() {
    buffer_.reserve(1024);
}

void Arm64Emitter::emit32(uint32_t val) {
    buffer_.push_back(static_cast<uint8_t>(val & 0xff));
    buffer_.push_back(static_cast<uint8_t>((val >> 8) & 0xff));
    buffer_.push_back(static_cast<uint8_t>((val >> 16) & 0xff));
    buffer_.push_back(static_cast<uint8_t>((val >> 24) & 0xff));
}

void Arm64Emitter::patchBranch(size_t branchOffset, size_t targetOffset) {
    if (branchOffset + 4 <= buffer_.size()) {
        int32_t byteRel = static_cast<int32_t>(targetOffset) - static_cast<int32_t>(branchOffset);
        int32_t wordRel = byteRel / 4;

        uint32_t inst = static_cast<uint32_t>(buffer_[branchOffset]) |
                       (static_cast<uint32_t>(buffer_[branchOffset + 1]) << 8) |
                       (static_cast<uint32_t>(buffer_[branchOffset + 2]) << 16) |
                       (static_cast<uint32_t>(buffer_[branchOffset + 3]) << 24);

        if ((inst & 0xfe000000) == 0x54000000) {
            // b.cond: imm19 is at bits [23:5]
            inst = (inst & 0xff00001f) | ((static_cast<uint32_t>(wordRel) & 0x7ffff) << 5);
        } else if ((inst & 0xfc000000) == 0x14000000) {
            // b: imm26 is at bits [25:0]
            inst = 0x14000000 | (static_cast<uint32_t>(wordRel) & 0x03ffffff);
        }

        buffer_[branchOffset]     = static_cast<uint8_t>(inst & 0xff);
        buffer_[branchOffset + 1] = static_cast<uint8_t>((inst >> 8) & 0xff);
        buffer_[branchOffset + 2] = static_cast<uint8_t>((inst >> 16) & 0xff);
        buffer_[branchOffset + 3] = static_cast<uint8_t>((inst >> 24) & 0xff);
    }
}

int Arm64Emitter::calculateFrameSize(int numLocals, int evalMax) const {
    int totalSlots = numLocals + evalMax;
    int bytes = totalSlots * 8;
    return ((bytes + 15) / 16) * 16;
}

int Arm64Emitter::getLocalDisp(int slot) const {
    return slot * 8;
}

int Arm64Emitter::getEvalDisp(int depth, int numLocals) const {
    return (numLocals + depth) * 8;
}

void Arm64Emitter::emitPrologue(int numLocals, int evalMax, int arity) {
    // 1. stp x29, x30, [sp, #-16]!
    emit32(0xa9bf7bfd);
    // 2. mov x29, sp
    emit32(0x910003fd);
    // 3. Stack frame allocation
    int frameSize = calculateFrameSize(numLocals, evalMax);
    if (frameSize <= 4095) {
        // sub sp, sp, #frameSize
        emit32(0xd10003ff | ((frameSize & 0xfff) << 10));
    } else {
        movRegImm64(X9, frameSize);
        // sub sp, sp, x9
        emit32(0xcb2963ff);
    }
    // 4. Save incoming arguments (in X0..X5) into local variable slots [sp, #disp]
    static const Reg inArgRegs[] = { X0, X1, X2, X3, X4, X5 };
    int toCopy = std::min(arity, 6);
    for (int i = 0; i < toCopy; ++i) {
        int disp = getLocalDisp(i);
        movStackReg(disp, inArgRegs[i]);
    }
}

void Arm64Emitter::emitEpilogue() {
    // 1. mov sp, x29
    emit32(0x910003bf);
    // 2. ldp x29, x30, [sp], #16
    emit32(0xa8c17bfd);
    // 3. ret
    emit32(0xd65f03c0);
}

void Arm64Emitter::movRegImm64(Reg dst, int64_t imm) {
    uint64_t u = static_cast<uint64_t>(imm);
    if (u == 0) {
        // mov dst, xzr (orr dst, xzr, xzr)
        emit32(0xaa1f03e0 | (dst & 0x1f));
        return;
    }
    uint16_t h[4];
    h[0] = u & 0xffff;
    h[1] = (u >> 16) & 0xffff;
    h[2] = (u >> 32) & 0xffff;
    h[3] = (u >> 48) & 0xffff;

    int first = -1;
    for (int i = 0; i < 4; ++i) {
        if (h[i] != 0) {
            first = i;
            break;
        }
    }
    // movz dst, #val, lsl #(first * 16)
    emit32(0xd2800000 | ((first & 3) << 21) | ((h[first] & 0xffff) << 5) | (dst & 0x1f));
    for (int i = first + 1; i < 4; ++i) {
        if (h[i] != 0) {
            // movk dst, #val, lsl #(i * 16)
            emit32(0xf2800000 | ((i & 3) << 21) | ((h[i] & 0xffff) << 5) | (dst & 0x1f));
        }
    }
}

void Arm64Emitter::movRegReg(Reg dst, Reg src) {
    // orr dst, xzr, src
    emit32(0xaa0003e0 | ((src & 0x1f) << 16) | (dst & 0x1f));
}

void Arm64Emitter::movStackReg(int stackDisp, Reg src) {
    if (stackDisp >= 0 && stackDisp <= 32760 && (stackDisp % 8 == 0)) {
        uint32_t imm12 = static_cast<uint32_t>(stackDisp / 8);
        // str src, [sp, #(imm12 * 8)]
        emit32(0xf9000000 | ((imm12 & 0xfff) << 10) | (31 << 5) | (src & 0x1f));
    } else {
        movRegImm64(X9, stackDisp);
        // str src, [sp, x9]
        emit32(0xf8296bf0 | (src & 0x1f));
    }
}

void Arm64Emitter::movRegStack(Reg dst, int stackDisp) {
    if (stackDisp >= 0 && stackDisp <= 32760 && (stackDisp % 8 == 0)) {
        uint32_t imm12 = static_cast<uint32_t>(stackDisp / 8);
        // ldr dst, [sp, #(imm12 * 8)]
        emit32(0xf9400000 | ((imm12 & 0xfff) << 10) | (31 << 5) | (dst & 0x1f));
    } else {
        movRegImm64(X9, stackDisp);
        // ldr dst, [sp, x9]
        emit32(0xf8696bf0 | (dst & 0x1f));
    }
}

void Arm64Emitter::addRegReg(Reg dst, Reg src) {
    // add dst, dst, src
    emit32(0x8b000000 | ((src & 0x1f) << 16) | ((dst & 0x1f) << 5) | (dst & 0x1f));
}

void Arm64Emitter::subRegReg(Reg dst, Reg src) {
    // sub dst, dst, src
    emit32(0xcb000000 | ((src & 0x1f) << 16) | ((dst & 0x1f) << 5) | (dst & 0x1f));
}

void Arm64Emitter::imulRegReg(Reg dst, Reg src) {
    // madd dst, dst, src, xzr
    emit32(0x9b007c00 | ((src & 0x1f) << 16) | ((dst & 0x1f) << 5) | (dst & 0x1f));
}

void Arm64Emitter::idivReg(Reg dst, Reg divisor) {
    // sdiv dst, dst, divisor
    emit32(0x9ac00c00 | ((divisor & 0x1f) << 16) | ((dst & 0x1f) << 5) | (dst & 0x1f));
}

void Arm64Emitter::imodReg(Reg dst, Reg divisor) {
    // sdiv x9, dst, divisor
    emit32(0x9ac00c00 | ((divisor & 0x1f) << 16) | ((dst & 0x1f) << 5) | 9);
    // msub dst, x9, divisor, dst
    emit32(0x9b008000 | ((divisor & 0x1f) << 16) | ((dst & 0x1f) << 10) | (9 << 5) | (dst & 0x1f));
}

void Arm64Emitter::negReg(Reg dst) {
    // sub dst, xzr, dst
    emit32(0xcb0003e0 | ((dst & 0x1f) << 16) | (dst & 0x1f));
}

void Arm64Emitter::notReg(Reg dst) {
    // cmp dst, #0 (subs xzr, dst, #0)
    emit32(0xf100001f | ((dst & 0x1f) << 5));
    // cset dst, eq
    setccReg(Cond::EQ, dst);
}

void Arm64Emitter::incStack(int stackDisp) {
    movRegStack(X9, stackDisp);
    emit32(0x91000529); // add x9, x9, #1
    movStackReg(stackDisp, X9);
}

void Arm64Emitter::decStack(int stackDisp) {
    movRegStack(X9, stackDisp);
    emit32(0xd1000529); // sub x9, x9, #1
    movStackReg(stackDisp, X9);
}

void Arm64Emitter::addStackReg(int stackDisp, Reg src) {
    movRegStack(X9, stackDisp);
    addRegReg(X9, src);
    movStackReg(stackDisp, X9);
}

void Arm64Emitter::addStackImm(int stackDisp, int64_t imm) {
    movRegStack(X9, stackDisp);
    if (imm >= 0 && imm <= 4095) {
        emit32(0x91000000 | ((imm & 0xfff) << 10) | (9 << 5) | 9);
    } else if (imm < 0 && -imm <= 4095) {
        emit32(0xd1000000 | (((-imm) & 0xfff) << 10) | (9 << 5) | 9);
    } else {
        movRegImm64(X10, imm);
        addRegReg(X9, X10);
    }
    movStackReg(stackDisp, X9);
}

void Arm64Emitter::subStackImm(int stackDisp, int64_t imm) {
    addStackImm(stackDisp, -imm);
}

void Arm64Emitter::cmpRegReg(Reg r1, Reg r2) {
    // subs xzr, r1, r2
    emit32(0xeb00001f | ((r2 & 0x1f) << 16) | ((r1 & 0x1f) << 5));
}

void Arm64Emitter::cmpStackImm(int stackDisp, int64_t imm) {
    movRegStack(X9, stackDisp);
    if (imm >= 0 && imm <= 4095) {
        emit32(0xf100001f | ((imm & 0xfff) << 10) | (9 << 5));
    } else {
        movRegImm64(X10, imm);
        cmpRegReg(X9, X10);
    }
}

void Arm64Emitter::testRegReg(Reg r1, Reg r2) {
    // ands xzr, r1, r2
    emit32(0xea00001f | ((r2 & 0x1f) << 16) | ((r1 & 0x1f) << 5));
}

void Arm64Emitter::setccReg(Cond cond, Reg dst) {
    uint8_t condInv = static_cast<uint8_t>(cond) ^ 1;
    // csinc dst, xzr, xzr, condInv
    emit32(0x9a9f07e0 | ((condInv & 0xf) << 12) | (dst & 0x1f));
}

void Arm64Emitter::movzxReg8(Reg dst) {
    (void)dst;
}

size_t Arm64Emitter::emitJmp() {
    size_t pos = buffer_.size();
    emit32(0x14000000); // b #0 placeholder
    return pos;
}

size_t Arm64Emitter::emitJcc(Cond cond) {
    size_t pos = buffer_.size();
    emit32(0x54000000 | (static_cast<uint8_t>(cond) & 0xf)); // b.cond #0 placeholder
    return pos;
}

void Arm64Emitter::emitCallReg(Reg targetReg) {
    // blr targetReg
    emit32(0xd63f0000 | ((targetReg & 0x1f) << 5));
}

void Arm64Emitter::emitRet() {
    // ret
    emit32(0xd65f03c0);
}

// ==========================================
// JitCompiler Implementation
// ==========================================

bool JitCompiler::canCompileInt(Chunk* chunk) {
#if !defined(FASTHON_JIT_TARGET_X64) && !defined(FASTHON_JIT_TARGET_ARM64)
    return false;
#endif

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

bool JitCompiler::compileChunkX64(Chunk* chunk, JitEngine* engine) {
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

    while (ip < code.size()) {
        size_t curBcIp = ip;
        bcToNative[curBcIp] = emitter.currentOffset();

        OpCode op = static_cast<OpCode>(code[ip++]);
        switch (op) {
            case OpCode::OP_CONSTANT: {
                uint16_t idx = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                const auto& c = chunk->constants[idx];
                int64_t val = (c.isInt() ? c.asInt() : (c.isBool() ? (c.boolVal ? 1 : 0) : 0));
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
                if (evalDepth > 0) evalDepth--;
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
                return false;
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
            case OpCode::OP_GREATER:
            case OpCode::OP_GREATER_EQUAL:
            case OpCode::OP_LESS:
            case OpCode::OP_LESS_EQUAL: {
                if (evalDepth >= 2) {
                    evalDepth--;
                    emitter.movRegStack(X64Emitter::RDX, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.movRegStack(X64Emitter::RAX, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.cmpRegReg(X64Emitter::RAX, X64Emitter::RDX);

                    uint8_t cc = 4; // sete
                    switch (op) {
                        case OpCode::OP_EQUAL: cc = 4; break;
                        case OpCode::OP_NOT_EQUAL: cc = 5; break;
                        case OpCode::OP_GREATER: cc = 15; break;
                        case OpCode::OP_GREATER_EQUAL: cc = 13; break;
                        case OpCode::OP_LESS: cc = 12; break;
                        case OpCode::OP_LESS_EQUAL: cc = 14; break;
                        default: break;
                    }
                    emitter.setccReg(cc, X64Emitter::RAX);
                    emitter.movzxReg8(X64Emitter::RAX);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), X64Emitter::RAX);
                }
                break;
            }
            case OpCode::OP_JUMP: {
                uint16_t offset = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                size_t fixup = emitter.emitJmp();
                fixups.push_back({ fixup, ip + offset });
                break;
            }
            case OpCode::OP_JUMP_IF_FALSE: {
                uint16_t offset = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                if (evalDepth > 0) {
                    emitter.movRegStack(X64Emitter::RAX, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.testRegReg(X64Emitter::RAX, X64Emitter::RAX);
                    size_t fixup = emitter.emitJcc(0x84); // jz
                    fixups.push_back({ fixup, ip + offset });
                }
                break;
            }
            case OpCode::OP_LOOP: {
                uint16_t offset = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                size_t fixup = emitter.emitJmp();
                fixups.push_back({ fixup, ip - offset });
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
                int16_t imm = static_cast<int16_t>((code[ip + 2] << 8) | code[ip + 3]);
                ip += 4;
                emitter.addStackImm(emitter.getLocalDisp(slot), imm);
                break;
            }
            case OpCode::OP_SUB_LOCAL_INT: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                int16_t imm = static_cast<int16_t>((code[ip + 2] << 8) | code[ip + 3]);
                ip += 4;
                emitter.subStackImm(emitter.getLocalDisp(slot), imm);
                break;
            }
            case OpCode::OP_JUMP_IF_LOCAL_GE_CONST:
            case OpCode::OP_JUMP_IF_LOCAL_GT_CONST:
            case OpCode::OP_JUMP_IF_LOCAL_LE_CONST:
            case OpCode::OP_JUMP_IF_LOCAL_LT_CONST: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                int16_t imm = static_cast<int16_t>((code[ip + 2] << 8) | code[ip + 3]);
                uint16_t offset = static_cast<uint16_t>((code[ip + 4] << 8) | code[ip + 5]);
                ip += 6;

                emitter.cmpStackImm(emitter.getLocalDisp(slot), imm);
                uint8_t cc = 0x8d; // jge
                if (op == OpCode::OP_JUMP_IF_LOCAL_GT_CONST) cc = 0x8f; // jg
                else if (op == OpCode::OP_JUMP_IF_LOCAL_LE_CONST) cc = 0x8e; // jle
                else if (op == OpCode::OP_JUMP_IF_LOCAL_LT_CONST) cc = 0x8c; // jl

                size_t fixup = emitter.emitJcc(cc);
                fixups.push_back({ fixup, ip + offset });
                break;
            }
            case OpCode::OP_FAST_LOOP: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                uint16_t offset = static_cast<uint16_t>((code[ip + 2] << 8) | code[ip + 3]);
                ip += 4;

                emitter.decStack(emitter.getLocalDisp(slot));
                emitter.movRegStack(X64Emitter::RAX, emitter.getLocalDisp(slot));
                emitter.movRegImm64(X64Emitter::RDX, 0);
                emitter.cmpRegReg(X64Emitter::RAX, X64Emitter::RDX);
                size_t fixup = emitter.emitJcc(0x8f); // jg
                fixups.push_back({ fixup, ip - offset });
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
                uint8_t argCount = code[ip + 2];
                ip += 3;

                Chunk* targetChunk = (fnIdx < chunk->cachedChunks.size()) ? chunk->cachedChunks[fnIdx] : nullptr;
                if (!targetChunk) {
                    return false;
                }

                void* slotAddress = reinterpret_cast<void*>(&targetChunk->jitFuncInt);

                for (int i = 0; i < argCount && i < 6; ++i) {
                    int slotDisp = emitter.getEvalDisp(evalDepth - argCount + i, numLocals);
                    emitter.movRegStack(argRegs[i], slotDisp);
                }

                emitter.movRegImm64(X64Emitter::R10, reinterpret_cast<uintptr_t>(slotAddress));
                // mov rax, [r10]
                emitter.emit8(0x49); emitter.emit8(0x8b); emitter.emit8(0x02);
                emitter.emitCallReg(X64Emitter::RAX);

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

bool JitCompiler::compileChunkArm64(Chunk* chunk, JitEngine* engine) {
    if (!chunk || chunk->isJitCompiled) {
        return false;
    }
    bool can = canCompileInt(chunk);
    if (!can) {
        return false;
    }

    chunk->jitAttempted = true;

    Arm64Emitter emitter;
    int numLocals = std::max(chunk->localsCount, chunk->arity);
    int evalMax = 64;

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

    static const Arm64Emitter::Reg callArgs[] = {
        Arm64Emitter::X0, Arm64Emitter::X1, Arm64Emitter::X2,
        Arm64Emitter::X3, Arm64Emitter::X4, Arm64Emitter::X5
    };

    while (ip < code.size()) {
        size_t curBcIp = ip;
        bcToNative[curBcIp] = emitter.currentOffset();

        OpCode op = static_cast<OpCode>(code[ip++]);
        switch (op) {
            case OpCode::OP_CONSTANT: {
                uint16_t idx = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                const auto& c = chunk->constants[idx];
                int64_t val = (c.isInt() ? c.asInt() : (c.isBool() ? (c.boolVal ? 1 : 0) : 0));
                emitter.movRegImm64(Arm64Emitter::X0, val);
                emitter.movStackReg(emitter.getEvalDisp(evalDepth, numLocals), Arm64Emitter::X0);
                evalDepth++;
                break;
            }
            case OpCode::OP_NIL:
            case OpCode::OP_FALSE: {
                emitter.movRegImm64(Arm64Emitter::X0, 0);
                emitter.movStackReg(emitter.getEvalDisp(evalDepth, numLocals), Arm64Emitter::X0);
                evalDepth++;
                break;
            }
            case OpCode::OP_TRUE: {
                emitter.movRegImm64(Arm64Emitter::X0, 1);
                emitter.movStackReg(emitter.getEvalDisp(evalDepth, numLocals), Arm64Emitter::X0);
                evalDepth++;
                break;
            }
            case OpCode::OP_POP: {
                if (evalDepth > 0) evalDepth--;
                break;
            }
            case OpCode::OP_GET_LOCAL: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                emitter.movRegStack(Arm64Emitter::X0, emitter.getLocalDisp(slot));
                emitter.movStackReg(emitter.getEvalDisp(evalDepth, numLocals), Arm64Emitter::X0);
                evalDepth++;
                break;
            }
            case OpCode::OP_SET_LOCAL: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                if (evalDepth > 0) {
                    emitter.movRegStack(Arm64Emitter::X0, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.movStackReg(emitter.getLocalDisp(slot), Arm64Emitter::X0);
                }
                break;
            }
            case OpCode::OP_GET_GLOBAL: {
                return false;
            }
            case OpCode::OP_ADD: {
                if (evalDepth >= 2) {
                    evalDepth--;
                    emitter.movRegStack(Arm64Emitter::X1, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.movRegStack(Arm64Emitter::X0, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.addRegReg(Arm64Emitter::X0, Arm64Emitter::X1);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), Arm64Emitter::X0);
                }
                break;
            }
            case OpCode::OP_SUB: {
                if (evalDepth >= 2) {
                    evalDepth--;
                    emitter.movRegStack(Arm64Emitter::X1, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.movRegStack(Arm64Emitter::X0, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.subRegReg(Arm64Emitter::X0, Arm64Emitter::X1);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), Arm64Emitter::X0);
                }
                break;
            }
            case OpCode::OP_MUL: {
                if (evalDepth >= 2) {
                    evalDepth--;
                    emitter.movRegStack(Arm64Emitter::X1, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.movRegStack(Arm64Emitter::X0, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.imulRegReg(Arm64Emitter::X0, Arm64Emitter::X1);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), Arm64Emitter::X0);
                }
                break;
            }
            case OpCode::OP_DIV: {
                if (evalDepth >= 2) {
                    evalDepth--;
                    emitter.movRegStack(Arm64Emitter::X1, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.movRegStack(Arm64Emitter::X0, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.idivReg(Arm64Emitter::X0, Arm64Emitter::X1);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), Arm64Emitter::X0);
                }
                break;
            }
            case OpCode::OP_MOD: {
                if (evalDepth >= 2) {
                    evalDepth--;
                    emitter.movRegStack(Arm64Emitter::X1, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.movRegStack(Arm64Emitter::X0, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.imodReg(Arm64Emitter::X0, Arm64Emitter::X1);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), Arm64Emitter::X0);
                }
                break;
            }
            case OpCode::OP_NEGATE: {
                if (evalDepth >= 1) {
                    emitter.movRegStack(Arm64Emitter::X0, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.negReg(Arm64Emitter::X0);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), Arm64Emitter::X0);
                }
                break;
            }
            case OpCode::OP_NOT: {
                if (evalDepth >= 1) {
                    emitter.movRegStack(Arm64Emitter::X0, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.notReg(Arm64Emitter::X0);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), Arm64Emitter::X0);
                }
                break;
            }
            case OpCode::OP_EQUAL:
            case OpCode::OP_NOT_EQUAL:
            case OpCode::OP_GREATER:
            case OpCode::OP_GREATER_EQUAL:
            case OpCode::OP_LESS:
            case OpCode::OP_LESS_EQUAL: {
                if (evalDepth >= 2) {
                    evalDepth--;
                    emitter.movRegStack(Arm64Emitter::X1, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.movRegStack(Arm64Emitter::X0, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.cmpRegReg(Arm64Emitter::X0, Arm64Emitter::X1);

                    Arm64Emitter::Cond cond = Arm64Emitter::Cond::EQ;
                    switch (op) {
                        case OpCode::OP_EQUAL: cond = Arm64Emitter::Cond::EQ; break;
                        case OpCode::OP_NOT_EQUAL: cond = Arm64Emitter::Cond::NE; break;
                        case OpCode::OP_GREATER: cond = Arm64Emitter::Cond::GT; break;
                        case OpCode::OP_GREATER_EQUAL: cond = Arm64Emitter::Cond::GE; break;
                        case OpCode::OP_LESS: cond = Arm64Emitter::Cond::LT; break;
                        case OpCode::OP_LESS_EQUAL: cond = Arm64Emitter::Cond::LE; break;
                        default: break;
                    }
                    emitter.setccReg(cond, Arm64Emitter::X0);
                    emitter.movStackReg(emitter.getEvalDisp(evalDepth - 1, numLocals), Arm64Emitter::X0);
                }
                break;
            }
            case OpCode::OP_JUMP: {
                uint16_t offset = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                size_t fixup = emitter.emitJmp();
                fixups.push_back({ fixup, ip + offset });
                break;
            }
            case OpCode::OP_JUMP_IF_FALSE: {
                uint16_t offset = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                if (evalDepth > 0) {
                    emitter.movRegStack(Arm64Emitter::X0, emitter.getEvalDisp(evalDepth - 1, numLocals));
                    emitter.cmpRegReg(Arm64Emitter::X0, Arm64Emitter::XZR);
                    size_t fixup = emitter.emitJcc(Arm64Emitter::Cond::EQ); // jump if false (0)
                    fixups.push_back({ fixup, ip + offset });
                }
                break;
            }
            case OpCode::OP_LOOP: {
                uint16_t offset = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                size_t fixup = emitter.emitJmp();
                fixups.push_back({ fixup, ip - offset });
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
                int16_t imm = static_cast<int16_t>((code[ip + 2] << 8) | code[ip + 3]);
                ip += 4;
                emitter.addStackImm(emitter.getLocalDisp(slot), imm);
                break;
            }
            case OpCode::OP_SUB_LOCAL_INT: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                int16_t imm = static_cast<int16_t>((code[ip + 2] << 8) | code[ip + 3]);
                ip += 4;
                emitter.subStackImm(emitter.getLocalDisp(slot), imm);
                break;
            }
            case OpCode::OP_JUMP_IF_LOCAL_GE_CONST:
            case OpCode::OP_JUMP_IF_LOCAL_GT_CONST:
            case OpCode::OP_JUMP_IF_LOCAL_LE_CONST:
            case OpCode::OP_JUMP_IF_LOCAL_LT_CONST: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                int16_t imm = static_cast<int16_t>((code[ip + 2] << 8) | code[ip + 3]);
                uint16_t offset = static_cast<uint16_t>((code[ip + 4] << 8) | code[ip + 5]);
                ip += 6;

                emitter.cmpStackImm(emitter.getLocalDisp(slot), imm);
                Arm64Emitter::Cond cond = Arm64Emitter::Cond::GE;
                if (op == OpCode::OP_JUMP_IF_LOCAL_GT_CONST) cond = Arm64Emitter::Cond::GT;
                else if (op == OpCode::OP_JUMP_IF_LOCAL_LE_CONST) cond = Arm64Emitter::Cond::LE;
                else if (op == OpCode::OP_JUMP_IF_LOCAL_LT_CONST) cond = Arm64Emitter::Cond::LT;

                size_t fixup = emitter.emitJcc(cond);
                fixups.push_back({ fixup, ip + offset });
                break;
            }
            case OpCode::OP_FAST_LOOP: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                uint16_t offset = static_cast<uint16_t>((code[ip + 2] << 8) | code[ip + 3]);
                ip += 4;

                emitter.decStack(emitter.getLocalDisp(slot));
                emitter.movRegStack(Arm64Emitter::X0, emitter.getLocalDisp(slot));
                emitter.cmpRegReg(Arm64Emitter::X0, Arm64Emitter::XZR);
                size_t fixup = emitter.emitJcc(Arm64Emitter::Cond::GT);
                fixups.push_back({ fixup, ip - offset });
                break;
            }
            case OpCode::OP_APPEND_LOCAL: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                if (evalDepth > 0) {
                    evalDepth--;
                    emitter.movRegStack(Arm64Emitter::X0, emitter.getEvalDisp(evalDepth, numLocals));
                    emitter.addStackReg(emitter.getLocalDisp(slot), Arm64Emitter::X0);
                }
                break;
            }
            case OpCode::OP_CALL: {
                uint16_t fnIdx = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                uint8_t argCount = code[ip + 2];
                ip += 3;

                Chunk* targetChunk = (fnIdx < chunk->cachedChunks.size()) ? chunk->cachedChunks[fnIdx] : nullptr;
                if (!targetChunk) {
                    return false;
                }

                void* slotAddress = reinterpret_cast<void*>(&targetChunk->jitFuncInt);

                for (int i = 0; i < argCount && i < 6; ++i) {
                    int slotDisp = emitter.getEvalDisp(evalDepth - argCount + i, numLocals);
                    emitter.movRegStack(callArgs[i], slotDisp);
                }

                emitter.movRegImm64(Arm64Emitter::X10, reinterpret_cast<uintptr_t>(slotAddress));
                // ldr x9, [x10, #0]
                emitter.emit32(0xf9400149);
                emitter.emitCallReg(Arm64Emitter::X9);

                evalDepth -= argCount;
                emitter.movStackReg(emitter.getEvalDisp(evalDepth, numLocals), Arm64Emitter::X0);
                evalDepth++;
                break;
            }
            case OpCode::OP_RETURN: {
                if (evalDepth > 0) {
                    emitter.movRegStack(Arm64Emitter::X0, emitter.getEvalDisp(evalDepth - 1, numLocals));
                } else {
                    emitter.movRegImm64(Arm64Emitter::X0, 0);
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
    emitter.emitEpilogue();

    // Patch all jumps
    for (const auto& f : fixups) {
        size_t targetNative = (f.targetBcIp >= chunk->code.size()) ? epilogueOffset : bcToNative[f.targetBcIp];
        emitter.patchBranch(f.nativePos, targetNative);
    }

    for (size_t epJmp : epilogueJumps) {
        emitter.patchBranch(epJmp, epilogueOffset);
    }

    void* codePtr = engine->allocateCode(emitter.getCode());
    if (codePtr) {
        chunk->jitFuncInt = codePtr;
        chunk->isJitCompiled = true;
        return true;
    }

    return false;
}

bool JitCompiler::compileChunk(Chunk* chunk, JitEngine* engine) {
#if defined(FASTHON_JIT_TARGET_X64)
    return compileChunkX64(chunk, engine);
#elif defined(FASTHON_JIT_TARGET_ARM64)
    return compileChunkArm64(chunk, engine);
#else
    (void)chunk;
    (void)engine;
    return false;
#endif
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
    std::cout << "  JIT Status: " << (enabled_ ? "Enabled (Active)" : "Disabled") << "\n";
#elif defined(FASTHON_JIT_TARGET_ARM64)
    std::cout << "  Architecture: AArch64 / ARM64 (Direct Native Machine Code JIT)\n";
    std::cout << "  JIT Mode: Silicon Level Direct Assembly Execution\n";
    std::cout << "  JIT Status: " << (enabled_ ? "Enabled (Active)" : "Disabled") << "\n";
#elif defined(FASTHON_TARGET_ARM32)
    std::cout << "  Architecture: ARM32 / armv7l (VM Mode - JIT Not Supported)\n";
    std::cout << "  JIT Mode: In-Memory Flat-Stack Virtual Machine\n";
    std::cout << "  JIT Status: Disabled (Architecture without JIT support)\n";
#else
    std::cout << "  Architecture: Generic Portable (VM Mode)\n";
    std::cout << "  JIT Mode: In-Memory Flat-Stack Virtual Machine\n";
    std::cout << "  JIT Status: Disabled\n";
#endif
    std::cout << "  Compiled Functions: " << compiledCount_ << "\n";
    std::cout << "  Total JIT Code Size: " << totalBytes_ << " bytes\n";
}
