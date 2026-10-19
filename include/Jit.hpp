#pragma once

#include "Bytecode.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <unordered_map>

class JitEngine;

enum class JitArch {
    X86_64,
    ARM64,
    ARM32_VM,
    PORTABLE
};

class ExecutableMemory {
public:
    ExecutableMemory(size_t size);
    ~ExecutableMemory();

    ExecutableMemory(const ExecutableMemory&) = delete;
    ExecutableMemory& operator=(const ExecutableMemory&) = delete;

    void* getPointer() const { return ptr_; }
    size_t getSize() const { return size_; }
    bool isValid() const { return ptr_ != nullptr; }

    void write(const void* data, size_t len);

private:
    void* ptr_;
    size_t size_;
};

#if defined(__x86_64__) || defined(_M_X64)
#define FASTHON_JIT_TARGET_X64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define FASTHON_JIT_TARGET_ARM64 1
#elif defined(__arm__) || defined(_M_ARM)
#define FASTHON_TARGET_ARM32 1
#endif

// ==========================================
// x86_64 JIT Emitter
// ==========================================
class X64Emitter {
public:
    enum Reg : uint8_t {
        RAX = 0,
        RCX = 1,
        RDX = 2,
        RBX = 3,
        RSP = 4,
        RBP = 5,
        RSI = 6,
        RDI = 7,
        R8  = 8,
        R9  = 9,
        R10 = 10,
        R11 = 11,
        R12 = 12,
        R13 = 13,
        R14 = 14,
        R15 = 15
    };

    X64Emitter();

    void emit8(uint8_t byte);
    void emit16(uint16_t word);
    void emit32(uint32_t dword);
    void emit64(uint64_t qword);

    size_t currentOffset() const { return buffer_.size(); }
    void patch32(size_t offset, int32_t val);

    const std::vector<uint8_t>& getCode() const { return buffer_; }

    // Instruction emitters
    void emitPrologue(int numLocals, int evalMax, int arity);
    void emitEpilogue(int frameSize);

    void movRegImm64(Reg dst, int64_t imm);
    void movRegReg(Reg dst, Reg src);
    void movStackReg(int stackSlot, Reg src);
    void movRegStack(Reg dst, int stackSlot);

    void addRegReg(Reg dst, Reg src);
    void subRegReg(Reg dst, Reg src);
    void imulRegReg(Reg dst, Reg src);
    void idivReg(Reg divisor);
    void imodReg(Reg divisor);

    void incStack(int stackSlot);
    void decStack(int stackSlot);
    void addStackReg(int stackSlot, Reg src);
    void addStackImm(int stackSlot, int64_t imm);
    void subStackImm(int stackSlot, int64_t imm);

    void incReg(Reg dst);
    void decReg(Reg dst);
    void addRegImm(Reg dst, int64_t imm);
    void subRegImm(Reg dst, int64_t imm);
    void cmpRegImm(Reg r, int64_t imm);
    void xorRegReg(Reg dst, Reg src);
    void movStackImm(int stackSlot, int64_t imm);

    void cmpRegReg(Reg r1, Reg r2);
    void cmpStackImm(int stackSlot, int64_t imm);
    void testRegReg(Reg r1, Reg r2);

    void setccReg(uint8_t condCode, Reg dst);
    void movzxReg8(Reg dst);

    void negReg(Reg dst);
    void notReg(Reg dst);

    size_t emitJmp();
    size_t emitJcc(uint8_t condCode);
    void emitCallReg(Reg targetReg);
    void emitRet();

    int calculateFrameSize(int numLocals, int evalMax) const;
    int getLocalDisp(int slot) const;
    int getEvalDisp(int depth, int numLocals) const;

private:
    std::vector<uint8_t> buffer_;

    void emitModRM(uint8_t mod, uint8_t reg, uint8_t rm);
    void emitRex(bool w, bool r, bool x, bool b);
    void emitDisp(int32_t disp);
};

// ==========================================
// AArch64 (ARM64) JIT Emitter
// ==========================================
class Arm64Emitter {
public:
    enum Reg : uint8_t {
        X0 = 0,
        X1 = 1,
        X2 = 2,
        X3 = 3,
        X4 = 4,
        X5 = 5,
        X6 = 6,
        X7 = 7,
        X8 = 8,
        X9 = 9,
        X10 = 10,
        X11 = 11,
        X12 = 12,
        X13 = 13,
        X14 = 14,
        X15 = 15,
        X16 = 16,
        X17 = 17,
        X18 = 18,
        X19 = 19,
        X20 = 20,
        X21 = 21,
        X22 = 22,
        X23 = 23,
        X24 = 24,
        X25 = 25,
        X26 = 26,
        X27 = 27,
        X28 = 28,
        X29 = 29, // Frame Pointer (FP)
        X30 = 30, // Link Register (LR)
        SP  = 31, // Stack Pointer
        XZR = 31  // Zero Register
    };

    enum class Cond : uint8_t {
        EQ = 0x0,
        NE = 0x1,
        CS = 0x2,
        CC = 0x3,
        MI = 0x4,
        PL = 0x5,
        VS = 0x6,
        VC = 0x7,
        HI = 0x8,
        LS = 0x9,
        GE = 0xa,
        LT = 0xb,
        GT = 0xc,
        LE = 0xd,
        AL = 0xe,
        NV = 0xf
    };

    Arm64Emitter();

    void emit32(uint32_t val);
    size_t currentOffset() const { return buffer_.size(); }
    const std::vector<uint8_t>& getCode() const { return buffer_; }

    void patchBranch(size_t branchOffset, size_t targetOffset);

    // Frame setup and teardown
    void emitPrologue(int numLocals, int evalMax, int arity);
    void emitEpilogue();

    // Data movement
    void movRegImm64(Reg dst, int64_t imm);
    void movRegReg(Reg dst, Reg src);
    void movStackReg(int stackDisp, Reg src);
    void movRegStack(Reg dst, int stackDisp);

    // Arithmetic
    void addRegReg(Reg dst, Reg src);
    void subRegReg(Reg dst, Reg src);
    void imulRegReg(Reg dst, Reg src);
    void idivReg(Reg dst, Reg divisor);
    void imodReg(Reg dst, Reg divisor);
    void negReg(Reg dst);
    void notReg(Reg dst);

    // In-place stack manipulation
    void incStack(int stackDisp);
    void decStack(int stackDisp);
    void addStackReg(int stackDisp, Reg src);
    void addStackImm(int stackDisp, int64_t imm);
    void subStackImm(int stackDisp, int64_t imm);

    // Register operations
    void incReg(Reg dst);
    void decReg(Reg dst);
    void addRegImm(Reg dst, int64_t imm);
    void subRegImm(Reg dst, int64_t imm);
    void cmpRegImm(Reg r, int64_t imm);
    void movStackImm(int stackDisp, int64_t imm);

    // Comparison & conditions
    void cmpRegReg(Reg r1, Reg r2);
    void cmpStackImm(int stackDisp, int64_t imm);
    void testRegReg(Reg r1, Reg r2);
    void setccReg(Cond cond, Reg dst);
    void movzxReg8(Reg dst);

    // Branches and jumps
    size_t emitJmp();
    size_t emitJcc(Cond cond);
    void emitCallReg(Reg targetReg);
    void emitRet();

    int calculateFrameSize(int numLocals, int evalMax) const;
    int getLocalDisp(int slot) const;
    int getEvalDisp(int depth, int numLocals) const;

private:
    std::vector<uint8_t> buffer_;
};

// ==========================================
// JitCompiler & JitEngine
// ==========================================
class JitCompiler {
public:
    static bool canCompileInt(Chunk* chunk);
    static bool compileChunk(Chunk* chunk, JitEngine* engine);

    static bool compileChunkX64(Chunk* chunk, JitEngine* engine);
    static bool compileChunkArm64(Chunk* chunk, JitEngine* engine);
};

class JitEngine {
public:
    static JitEngine& instance();

    bool isEnabled() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }

    void compileAllFunctions(const std::unordered_map<std::string, std::shared_ptr<Chunk>>& fns);
    bool compileFunction(Chunk* chunk);

    void* allocateCode(const std::vector<uint8_t>& code);
    void printJitInfo() const;

    size_t getCompiledCount() const { return compiledCount_; }
    size_t getTotalBytes() const { return totalBytes_; }

private:
    JitEngine();
    ~JitEngine();

    bool enabled_;
    size_t compiledCount_;
    size_t totalBytes_;
    std::vector<std::unique_ptr<ExecutableMemory>> pages_;
};
