#pragma once

#include "Bytecode.hpp"
#include "StandardLibrary.hpp"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <vector>

struct CallFrame {
    Chunk* chunk;
    size_t ip;
    Value* slots;
};

class VM {
public:
    VM();

    void registerFunction(const std::string& name, std::shared_ptr<Chunk> chunk);
    void registerFunctions(const std::unordered_map<std::string, std::shared_ptr<Chunk>>& fns);
    Value run(Chunk* chunk);
    void runtimeError(const std::string& errorType, const std::string& message, Chunk* chunk, size_t ip, size_t frameCount, const std::string& recommendation = "", const std::string& hint = "");
    void runtimeError(const std::string& message, Chunk* chunk, size_t ip, size_t frameCount);
    void loadLibrary(const std::string& name);
    bool isLibraryLoaded(const std::string& name) const;

    static void setBenchMode(bool b) { benchMode_ = b; }
    static bool isBenchMode() { return benchMode_; }

private:
    inline static bool benchMode_ = false;
    static constexpr size_t STACK_MAX = 262144;
    static constexpr size_t FRAMES_MAX = 8192;

    std::vector<Value> stack_;
    std::vector<CallFrame> frames_;
    std::unordered_map<std::string, Value> globals_;
    std::unordered_map<std::string, std::shared_ptr<Chunk>> functions_;
    std::unordered_set<std::string> loadedLibraries_;
};
