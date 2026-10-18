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
#endif

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

class JitCompiler {
public:
    static bool canCompileInt(Chunk* chunk);
    static bool compileChunk(Chunk* chunk, JitEngine* engine);
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
