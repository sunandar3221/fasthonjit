#include "VM.hpp"
#include "Jit.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "BytecodeCompiler.hpp"
#include "Diagnostic.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <cstdlib>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

VM::VM() {
    stack_.resize(STACK_MAX);
    frames_.resize(FRAMES_MAX);
}

void VM::registerFunction(const std::string& name, std::shared_ptr<Chunk> chunk) {
    functions_[name] = std::move(chunk);
}

void VM::registerFunctions(const std::unordered_map<std::string, std::shared_ptr<Chunk>>& fns) {
    for (const auto& kv : fns) {
        functions_[kv.first] = kv.second;
    }
    JitEngine::instance().compileAllFunctions(functions_);
}

void VM::loadLibrary(const std::string& name) {
    loadedLibraries_.insert(name);
    if (name == "io") {
        Value ioObj = Value::makeObject();
        ioObj.setProperty("input", Value(ValueType::FUNCTION, "io.input"));
        ioObj.setProperty("ask", Value(ValueType::FUNCTION, "io.input"));
        ioObj.setProperty("read", Value(ValueType::FUNCTION, "io.read"));
        ioObj.setProperty("write", Value(ValueType::FUNCTION, "io.write"));
        ioObj.setProperty("print", Value(ValueType::FUNCTION, "io.print"));
        globals_["io"] = ioObj;
        globals_["input"] = Value(ValueType::FUNCTION, "io.input");
    } else if (name == "math") {
        Value mathObj = Value::makeObject();
        mathObj.setProperty("sqrt", Value(ValueType::FUNCTION, "math.sqrt"));
        mathObj.setProperty("abs", Value(ValueType::FUNCTION, "math.abs"));
        mathObj.setProperty("pow", Value(ValueType::FUNCTION, "math.pow"));
        mathObj.setProperty("floor", Value(ValueType::FUNCTION, "math.floor"));
        mathObj.setProperty("ceil", Value(ValueType::FUNCTION, "math.ceil"));
        mathObj.setProperty("round", Value(ValueType::FUNCTION, "math.round"));
        mathObj.setProperty("min", Value(ValueType::FUNCTION, "math.min"));
        mathObj.setProperty("max", Value(ValueType::FUNCTION, "math.max"));
        mathObj.setProperty("sin", Value(ValueType::FUNCTION, "math.sin"));
        mathObj.setProperty("cos", Value(ValueType::FUNCTION, "math.cos"));
        mathObj.setProperty("tan", Value(ValueType::FUNCTION, "math.tan"));
        mathObj.setProperty("random", Value(ValueType::FUNCTION, "math.random"));
        mathObj.setProperty("random_seed", Value(ValueType::FUNCTION, "math.random_seed"));
        mathObj.setProperty("randomSeed", Value(ValueType::FUNCTION, "math.random_seed"));
        mathObj.setProperty("seed", Value(ValueType::FUNCTION, "math.random_seed"));
        mathObj.setProperty("pi", Value(3.14159265358979323846));
        mathObj.setProperty("e", Value(2.71828182845904523536));
        globals_["math"] = mathObj;
        globals_["random"] = Value(ValueType::FUNCTION, "math.random");
        globals_["random_seed"] = Value(ValueType::FUNCTION, "math.random_seed");
        globals_["seed"] = Value(ValueType::FUNCTION, "math.random_seed");
    } else if (name == "time") {
        Value timeObj = Value::makeObject();
        timeObj.setProperty("sleep", Value(ValueType::FUNCTION, "time.sleep"));
        timeObj.setProperty("now", Value(ValueType::FUNCTION, "time.now"));
        globals_["time"] = timeObj;
    } else if (name == "net" || name == "http") {
        Value netObj = Value::makeObject();
        netObj.setProperty("get", Value(ValueType::FUNCTION, "net.get"));
        netObj.setProperty("send", Value(ValueType::FUNCTION, "net.send"));
        globals_[name] = netObj;
    } else if (name == "gui") {
        Value guiObj = Value::makeObject();
        guiObj.setProperty("app", Value(ValueType::FUNCTION, "gui.app"));
        guiObj.setProperty("window", Value(ValueType::FUNCTION, "gui.window"));
        guiObj.setProperty("run", Value(ValueType::FUNCTION, "gui.run"));
        globals_["gui"] = guiObj;
    } else if (name == "str" || name == "string") {
        Value strObj = Value::makeObject();
        strObj.setProperty("lower", Value(ValueType::FUNCTION, "lower"));
        strObj.setProperty("upper", Value(ValueType::FUNCTION, "upper"));
        strObj.setProperty("case_sensitive", Value(ValueType::FUNCTION, "case_sensitive"));
        strObj.setProperty("incase_sensitive", Value(ValueType::FUNCTION, "incase_sensitive"));
        globals_["str"] = strObj;
        globals_["string"] = strObj;
    } else {
        std::string filename = name;
        if (filename.size() < 4 || (filename.substr(filename.size() - 4) != ".fsn" && filename.substr(filename.size() - 4) != ".eas")) {
            std::ifstream testFsn(filename + ".fsn");
            if (testFsn.good()) {
                filename += ".fsn";
            } else {
                filename += ".eas";
            }
        }
        Value fileContent = StandardLibrary::readFile(filename);
        if (!fileContent.strVal.empty()) {
            Lexer modLexer(fileContent.strVal);
            auto modTokens = modLexer.tokenize();
            if (!modLexer.hasErrors()) {
                Parser modParser(std::move(modTokens));
                auto modAst = modParser.parseProgram();
                if (!modParser.hasErrors()) {
                    BytecodeCompiler modComp;
                    auto modChunk = modComp.compile(modAst.get());
                    for (const auto& kv : modComp.getFunctions()) {
                        functions_[kv.first] = kv.second;
                    }
                    VM modVM;
                    modVM.globals_ = globals_;
                    modVM.functions_ = functions_;
                    modVM.loadedLibraries_ = loadedLibraries_;
                    modVM.run(modChunk.get());
                    for (const auto& g : modVM.globals_) {
                        globals_[g.first] = g.second;
                    }
                    for (const auto& f : modVM.functions_) {
                        functions_[f.first] = f.second;
                    }
                    for (const auto& l : modVM.loadedLibraries_) {
                        loadedLibraries_.insert(l);
                    }
                }
            }
        }
    }
}

bool VM::isLibraryLoaded(const std::string& name) const {
    return loadedLibraries_.find(name) != loadedLibraries_.end();
}

void VM::runtimeError(const std::string& errorType, const std::string& message, Chunk* chunk, size_t ip, size_t frameCount, const std::string& recommendation, const std::string& hint) {
    (void)frameCount;
    int line = 1;
    int col = 1;
    int len = 1;
    if (chunk && !chunk->lines.empty()) {
        size_t lineIdx = (ip > 0 && ip - 1 < chunk->lines.size()) ? ip - 1 : 0;
        line = chunk->lines[lineIdx];
        if (lineIdx < chunk->columns.size()) col = chunk->columns[lineIdx];
        if (lineIdx < chunk->lengths.size()) len = chunk->lengths[lineIdx];
    }
    std::string err = Diagnostic::format(errorType, message, line, col, len, recommendation, hint);
    throw std::runtime_error(err);
}

void VM::runtimeError(const std::string& message, Chunk* chunk, size_t ip, size_t frameCount) {
    runtimeError("RuntimeError", message, chunk, ip, frameCount);
}

Value VM::run(Chunk* chunk) {
    if (!chunk) return Value();

    auto resolveChunk = [this](Chunk* c) {
        if (!c) return;
        c->cachedChunks.resize(c->constants.size(), nullptr);
        for (size_t i = 0; i < c->constants.size(); ++i) {
            if (c->constants[i].isString()) {
                auto it = functions_.find(c->constants[i].strVal);
                if (it != functions_.end()) {
                    c->cachedChunks[i] = it->second.get();
                }
            }
        }
    };

    resolveChunk(chunk);
    for (auto& kv : functions_) {
        resolveChunk(kv.second.get());
    }
    JitEngine::instance().compileAllFunctions(functions_);

    Value* top = stack_.data();
    size_t frameCount = 0;

    CallFrame* frame = &frames_[frameCount++];
    frame->chunk = chunk;
    frame->ip = 0;
    frame->slots = top;

    for (int i = 0; i < chunk->localsCount; ++i) {
        *top++ = Value();
    }

    Chunk* curChunk = frame->chunk;
    uint8_t* code = curChunk->code.data();
    size_t ip = 0;
    Value* slots = frame->slots;

    auto requireModule = [&](const std::string& modName, const std::string& targetName) {
        if (!isLibraryLoaded(modName)) {
            runtimeError("ImportError", "Modul '" + modName + "' belum dimuat. " + (targetName.empty() ? "" : ("'" + targetName + "' ")) + "memerlukan modul '" + modName + "'.", curChunk, ip, frameCount, "use " + modName, "Tambahkan perintah 'use " + modName + "' di bagian atas skrip Anda.");
        }
    };

    while (true) {
        uint8_t instruction = code[ip++];
        switch (static_cast<OpCode>(instruction)) {
            case OpCode::OP_CONSTANT: {
                uint16_t idx = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                *top++ = curChunk->constants[idx];
                break;
            }
            case OpCode::OP_NIL:
                *top++ = Value();
                break;
            case OpCode::OP_TRUE:
                *top++ = Value(true);
                break;
            case OpCode::OP_FALSE:
                *top++ = Value(false);
                break;
            case OpCode::OP_POP:
                if (top > slots) {
                    top--;
                }
                break;
            case OpCode::OP_GET_LOCAL: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                *top++ = slots[slot];
                break;
            }
            case OpCode::OP_SET_LOCAL: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                slots[slot] = *(top - 1);
                break;
            }
            case OpCode::OP_GET_GLOBAL: {
                uint16_t idx = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                const std::string& name = curChunk->constants[idx].strVal;
                auto it = globals_.find(name);
                if (it != globals_.end()) {
                    *top++ = it->second;
                } else {
                    auto fit = functions_.find(name);
                    if (fit != functions_.end()) {
                        *top++ = Value(ValueType::FUNCTION, name);
                    } else if (name == "true") {
                        *top++ = Value(true);
                    } else if (name == "false") {
                        *top++ = Value(false);
                    } else if (name == "print" || name == "silent_print" || name == "len") {
                        *top++ = Value(ValueType::FUNCTION, name);
                    } else if (name == "io") {
                        runtimeError("ImportError", "Modul 'io' belum dimuat.", curChunk, ip, frameCount, "", "Tambahkan 'use io' atau 'import io' di baris awal berkas.");
                    } else if (name == "input" || name == "ask") {
                        runtimeError("ImportError", "Fungsi '" + name + "' memerlukan modul 'io'.", curChunk, ip, frameCount, "", "Tambahkan 'use io' atau 'import io' di baris awal berkas.");
                    } else if (name == "read" || name == "write") {
                        runtimeError("ImportError", "Operasi berkas '" + name + "' memerlukan modul 'io'.", curChunk, ip, frameCount, "", "Tambahkan 'use io' atau 'import io' di baris awal berkas.");
                    } else if (name == "math") {
                        runtimeError("ImportError", "Modul 'math' belum dimuat.", curChunk, ip, frameCount, "", "Tambahkan 'use math' atau 'import math' di baris awal berkas.");
                    } else if (name == "time") {
                        runtimeError("ImportError", "Modul 'time' belum dimuat.", curChunk, ip, frameCount, "", "Tambahkan 'use time' atau 'import time' di baris awal berkas.");
                    } else if (name == "net" || name == "http") {
                        runtimeError("ImportError", "Modul '" + name + "' belum dimuat.", curChunk, ip, frameCount, "", "Tambahkan 'use " + name + "' di baris awal berkas.");
                    } else if (name == "gui") {
                        runtimeError("ImportError", "Modul 'gui' belum dimuat.", curChunk, ip, frameCount, "", "Tambahkan 'use gui' di baris awal berkas.");
                    } else {
                        // Check if it's a keyword typo
                        std::string kwMatch = Diagnostic::suggestSimilar(name, Diagnostic::getKeywords());
                        if (!kwMatch.empty()) {
                            runtimeError("SyntaxError", "Keyword '" + name + "' tidak dikenali.", curChunk, ip, frameCount, "Apakah maksud Anda '" + kwMatch + "'?");
                        }
                        // Check if it's a global variable typo
                        std::vector<std::string> varNames;
                        for (const auto& g : globals_) varNames.push_back(g.first);
                        std::string varMatch = Diagnostic::suggestSimilar(name, varNames);
                        if (!varMatch.empty()) {
                            runtimeError("NameError", "Variabel '" + name + "' belum didefinisikan.", curChunk, ip, frameCount, "Apakah maksud Anda variabel '" + varMatch + "'?");
                        }
                        // Check if it's a builtin function typo
                        std::string fnMatch = Diagnostic::suggestSimilar(name, Diagnostic::getBuiltinFunctions());
                        if (!fnMatch.empty()) {
                            runtimeError("NameError", "Nama '" + name + "' belum didefinisikan.", curChunk, ip, frameCount, "Apakah maksud Anda fungsi '" + fnMatch + "'?");
                        }
                        runtimeError("NameError", "Variabel '" + name + "' belum didefinisikan.", curChunk, ip, frameCount);
                    }
                }
                break;
            }
            case OpCode::OP_SET_GLOBAL: {
                uint16_t idx = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                globals_[curChunk->constants[idx].strVal] = *(top - 1);
                break;
            }
            case OpCode::OP_ADD: {
                Value& a = *(top - 2);
                const Value& b = *(top - 1);
                if (a.type == ValueType::INT && b.type == ValueType::INT) {
                    a.intVal += b.intVal;
                    a.floatVal = static_cast<double>(a.intVal);
                } else if (a.isString()) {
                    if (b.isString()) {
                        a.strVal.append(b.strVal);
                    } else {
                        a.strVal.append(b.toString());
                    }
                } else if (b.isString()) {
                    std::string s = a.toString();
                    s.append(b.strVal);
                    a = Value(std::move(s));
                } else if (a.isNil() || b.isNil()) {
                    runtimeError("TypeError", "Operasi '+' tidak dapat dilakukan pada 'nil'.", curChunk, ip, frameCount, "", "Pastikan variabel memiliki nilai numerik atau string yang valid.");
                } else {
                    a = a + b;
                }
                top--;
                break;
            }
            case OpCode::OP_SUB: {
                Value& a = *(top - 2);
                const Value& b = *(top - 1);
                if (a.isNil() || b.isNil()) {
                    runtimeError("TypeError", "Operasi '-' tidak dapat dilakukan pada 'nil'.", curChunk, ip, frameCount, "", "Pastikan variabel memiliki nilai numerik yang valid.");
                }
                if (a.type == ValueType::INT && b.type == ValueType::INT) {
                    a.intVal -= b.intVal;
                } else {
                    a = a - b;
                }
                top--;
                break;
            }
            case OpCode::OP_MUL: {
                Value& a = *(top - 2);
                const Value& b = *(top - 1);
                if (a.isNil() || b.isNil()) {
                    runtimeError("TypeError", "Operasi '*' tidak dapat dilakukan pada 'nil'.", curChunk, ip, frameCount, "", "Pastikan variabel memiliki nilai numerik yang valid.");
                }
                if (a.type == ValueType::INT && b.type == ValueType::INT) {
                    a.intVal *= b.intVal;
                } else {
                    a = a * b;
                }
                top--;
                break;
            }
            case OpCode::OP_DIV: {
                Value& a = *(top - 2);
                const Value& b = *(top - 1);
                if (a.isNil() || b.isNil()) {
                    runtimeError("TypeError", "Operasi '/' tidak dapat dilakukan pada 'nil'.", curChunk, ip, frameCount, "", "Pastikan variabel memiliki nilai numerik yang valid.");
                }
                if (b.asFloat() == 0.0) {
                    runtimeError("ZeroDivisionError", "Pembagian dengan angka nol tidak diperbolehkan (division by zero).", curChunk, ip, frameCount);
                }
                if (a.type == ValueType::INT && b.type == ValueType::INT && !(a.intVal == INT64_MIN && b.intVal == -1) && (a.intVal % b.intVal == 0)) {
                    a.intVal /= b.intVal;
                } else {
                    a = a / b;
                }
                top--;
                break;
            }
            case OpCode::OP_MOD: {
                Value& a = *(top - 2);
                const Value& b = *(top - 1);
                if (a.isNil() || b.isNil()) {
                    runtimeError("TypeError", "Operasi '%' tidak dapat dilakukan pada 'nil'.", curChunk, ip, frameCount, "", "Pastikan variabel memiliki nilai numerik yang valid.");
                }
                if (b.asInt() == 0) {
                    runtimeError("ZeroDivisionError", "Operasi modulo dengan angka nol tidak diperbolehkan (modulo by zero).", curChunk, ip, frameCount);
                }
                if (a.type == ValueType::INT && b.type == ValueType::INT) {
                    if (a.intVal == INT64_MIN && b.intVal == -1) {
                        a.intVal = 0;
                    } else {
                        a.intVal %= b.intVal;
                    }
                } else {
                    a = a % b;
                }
                top--;
                break;
            }
            case OpCode::OP_NEGATE: {
                Value& a = *(top - 1);
                if (a.isNil()) {
                    runtimeError("TypeError", "Operasi negasi '-' tidak dapat dilakukan pada 'nil'.", curChunk, ip, frameCount, "", "Pastikan variabel memiliki nilai numerik yang valid.");
                }
                if (a.type == ValueType::INT) {
                    a.intVal = -a.intVal;
                } else if (a.type == ValueType::FLOAT) {
                    a.floatVal = -a.floatVal;
                } else {
                    a = Value(0) - a;
                }
                break;
            }
            case OpCode::OP_NOT: {
                Value& a = *(top - 1);
                bool b = !a.isTruthy();
                a.type = ValueType::BOOL;
                a.boolVal = b;
                a.intVal = b ? 1 : 0;
                a.floatVal = b ? 1.0 : 0.0;
                break;
            }
            case OpCode::OP_EQUAL: {
                Value& a = *(top - 2);
                const Value& b = *(top - 1);
                if (a.type == ValueType::INT && b.type == ValueType::INT) {
                    a.type = ValueType::BOOL;
                    a.boolVal = (a.intVal == b.intVal);
                    a.intVal = a.boolVal ? 1 : 0;
                    a.floatVal = a.boolVal ? 1.0 : 0.0;
                } else {
                    a = Value(a == b);
                }
                top--;
                break;
            }
            case OpCode::OP_NOT_EQUAL: {
                Value& a = *(top - 2);
                const Value& b = *(top - 1);
                if (a.type == ValueType::INT && b.type == ValueType::INT) {
                    a.type = ValueType::BOOL;
                    a.boolVal = (a.intVal != b.intVal);
                    a.intVal = a.boolVal ? 1 : 0;
                    a.floatVal = a.boolVal ? 1.0 : 0.0;
                } else {
                    a = Value(a != b);
                }
                top--;
                break;
            }
            case OpCode::OP_LESS: {
                Value& a = *(top - 2);
                const Value& b = *(top - 1);
                if (a.isNil() || b.isNil()) {
                    runtimeError("TypeError", "Operator '<' tidak dapat membandingkan '" + a.getTypeName() + "' dengan '" + b.getTypeName() + "'.", curChunk, ip, frameCount, "", "Periksa apakah variabel bernilai 'nil' sebelum melakukan perbandingan.");
                }
                if (a.type == ValueType::INT && b.type == ValueType::INT) {
                    a.type = ValueType::BOOL;
                    a.boolVal = (a.intVal < b.intVal);
                    a.intVal = a.boolVal ? 1 : 0;
                    a.floatVal = a.boolVal ? 1.0 : 0.0;
                } else {
                    a = Value(a < b);
                }
                top--;
                break;
            }
            case OpCode::OP_LESS_EQUAL: {
                Value& a = *(top - 2);
                const Value& b = *(top - 1);
                if (a.isNil() || b.isNil()) {
                    runtimeError("TypeError", "Operator '<=' tidak dapat membandingkan '" + a.getTypeName() + "' dengan '" + b.getTypeName() + "'.", curChunk, ip, frameCount, "", "Periksa apakah variabel bernilai 'nil' sebelum melakukan perbandingan.");
                }
                if (a.type == ValueType::INT && b.type == ValueType::INT) {
                    a.type = ValueType::BOOL;
                    a.boolVal = (a.intVal <= b.intVal);
                    a.intVal = a.boolVal ? 1 : 0;
                    a.floatVal = a.boolVal ? 1.0 : 0.0;
                } else {
                    a = Value(a <= b);
                }
                top--;
                break;
            }
            case OpCode::OP_GREATER: {
                Value& a = *(top - 2);
                const Value& b = *(top - 1);
                if (a.isNil() || b.isNil()) {
                    runtimeError("TypeError", "Operator '>' tidak dapat membandingkan '" + a.getTypeName() + "' dengan '" + b.getTypeName() + "'.", curChunk, ip, frameCount, "", "Periksa apakah variabel bernilai 'nil' sebelum melakukan perbandingan.");
                }
                if (a.type == ValueType::INT && b.type == ValueType::INT) {
                    a.type = ValueType::BOOL;
                    a.boolVal = (a.intVal > b.intVal);
                    a.intVal = a.boolVal ? 1 : 0;
                    a.floatVal = a.boolVal ? 1.0 : 0.0;
                } else {
                    a = Value(a > b);
                }
                top--;
                break;
            }
            case OpCode::OP_GREATER_EQUAL: {
                Value& a = *(top - 2);
                const Value& b = *(top - 1);
                if (a.isNil() || b.isNil()) {
                    runtimeError("TypeError", "Operator '>=' tidak dapat membandingkan '" + a.getTypeName() + "' dengan '" + b.getTypeName() + "'.", curChunk, ip, frameCount, "", "Periksa apakah variabel bernilai 'nil' sebelum melakukan perbandingan.");
                }
                if (a.type == ValueType::INT && b.type == ValueType::INT) {
                    a.type = ValueType::BOOL;
                    a.boolVal = (a.intVal >= b.intVal);
                    a.intVal = a.boolVal ? 1 : 0;
                    a.floatVal = a.boolVal ? 1.0 : 0.0;
                } else {
                    a = Value(a >= b);
                }
                top--;
                break;
            }
            case OpCode::OP_JUMP: {
                uint16_t offset = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2 + offset;
                break;
            }
            case OpCode::OP_JUMP_IF_FALSE: {
                uint16_t offset = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                if (!(*(top - 1)).isTruthy()) {
                    ip += offset;
                }
                break;
            }
            case OpCode::OP_LOOP: {
                uint16_t offset = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                ip -= offset;
                break;
            }
            case OpCode::OP_CALL: {
                uint16_t fnIdx = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                uint8_t argCount = code[ip++];

                Chunk* nextChunk = curChunk->cachedChunks[fnIdx];
                if (nextChunk) {
                    if (nextChunk->isJitCompiled && nextChunk->jitFuncInt && argCount == nextChunk->arity) {
                        bool allInt = true;
                        int64_t args[6] = {0};
                        for (int i = 0; i < argCount && i < 6; ++i) {
                            Value& v = *(top - argCount + i);
                            if (v.type != ValueType::INT && v.type != ValueType::BOOL) {
                                allInt = false;
                                break;
                            }
                            args[i] = v.asInt();
                        }
                        if (allInt) {
                            int64_t res = 0;
                            typedef int64_t (*Jit0)();
                            typedef int64_t (*Jit1)(int64_t);
                            typedef int64_t (*Jit2)(int64_t, int64_t);
                            typedef int64_t (*Jit3)(int64_t, int64_t, int64_t);
                            typedef int64_t (*Jit4)(int64_t, int64_t, int64_t, int64_t);
                            typedef int64_t (*Jit5)(int64_t, int64_t, int64_t, int64_t, int64_t);
                            typedef int64_t (*Jit6)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

                            switch (argCount) {
                                case 0: res = ((Jit0)nextChunk->jitFuncInt)(); break;
                                case 1: res = ((Jit1)nextChunk->jitFuncInt)(args[0]); break;
                                case 2: res = ((Jit2)nextChunk->jitFuncInt)(args[0], args[1]); break;
                                case 3: res = ((Jit3)nextChunk->jitFuncInt)(args[0], args[1], args[2]); break;
                                case 4: res = ((Jit4)nextChunk->jitFuncInt)(args[0], args[1], args[2], args[3]); break;
                                case 5: res = ((Jit5)nextChunk->jitFuncInt)(args[0], args[1], args[2], args[3], args[4]); break;
                                case 6: res = ((Jit6)nextChunk->jitFuncInt)(args[0], args[1], args[2], args[3], args[4], args[5]); break;
                            }
                            top -= argCount;
                            *top++ = Value(res);
                            break;
                        }
                    }

                    frame->ip = ip;
                    Value* nextSlots = top - argCount;
                    for (int i = argCount; i < nextChunk->localsCount; ++i) {
                        *top++ = Value();
                    }
                    CallFrame* newFrame = &frames_[frameCount++];
                    newFrame->chunk = nextChunk;
                    newFrame->ip = 0;
                    newFrame->slots = nextSlots;

                    frame = newFrame;
                    curChunk = nextChunk;
                    code = curChunk->code.data();
                    ip = 0;
                    slots = nextSlots;
                    break;
                }

                const std::string& name = curChunk->constants[fnIdx].strVal;

                if (name == "len") {
                    Value arg = *(--top);
                    if (arg.isList()) *top++ = Value(static_cast<int64_t>(arg.listVal ? arg.listVal->size() : 0));
                    else if (arg.isString()) *top++ = Value(static_cast<int64_t>(arg.strVal.size()));
                    else if (arg.isObject()) *top++ = Value(static_cast<int64_t>(arg.objVal ? arg.objVal->size() : 0));
                    else *top++ = Value(static_cast<int64_t>(0));
                } else if (name == "push" && argCount >= 2) {
                    Value val = *(--top);
                    Value target = *(--top);
                    top -= (argCount - 2);
                    if (target.isList()) {
                        if (!target.listVal) target.listVal = std::make_shared<std::vector<Value>>();
                        target.listVal->push_back(val);
                    }
                    *top++ = val;
                } else if (name == "pop" && argCount >= 1) {
                    Value target = *(--top);
                    top -= (argCount - 1);
                    if (target.isList() && target.listVal && !target.listVal->empty()) {
                        Value popped = target.listVal->back();
                        target.listVal->pop_back();
                        *top++ = popped;
                    } else {
                        *top++ = Value();
                    }
                } else if (name == "str") {
                    Value arg = argCount > 0 ? *(--top) : Value("");
                    *top++ = Value(arg.toString());
                } else if (name == "int") {
                    Value arg = argCount > 0 ? *(--top) : Value();
                    *top++ = StandardLibrary::toInt(arg);
                } else if (name == "float") {
                    Value arg = argCount > 0 ? *(--top) : Value();
                    *top++ = StandardLibrary::toFloat(arg);
                } else if (name == "lower" || name == "to_lower" || name == "lowercase" || name == "kecil" || name == "str.lower") {
                    Value arg = argCount > 0 ? *(--top) : Value("");
                    *top++ = StandardLibrary::toLower(arg.toString());
                } else if (name == "upper" || name == "to_upper" || name == "uppercase" || name == "kapital" || name == "str.upper") {
                    Value arg = argCount > 0 ? *(--top) : Value("");
                    *top++ = StandardLibrary::toUpper(arg.toString());
                } else if (name == "incase_sensitive" || name == "incaseSensitive" || name == "incase_sensitif" ||
                           name == "incaseSensitif" || name == "incasesensitive" || name == "incasesensitif" ||
                           name == "incase" || name == "icase" || name == "iequals" || name == "iequal" || name == "str.incase_sensitive") {
                    if (argCount == 1) {
                        Value a = *(--top);
                        *top++ = StandardLibrary::toLower(a.toString());
                    } else if (argCount >= 2) {
                        Value b = *(--top);
                        Value a = *(--top);
                        top -= (argCount - 2);
                        *top++ = StandardLibrary::incaseSensitive(a.toString(), b.toString());
                    } else {
                        *top++ = Value(false);
                    }
                } else if (name == "case_sensitive" || name == "caseSensitive" || name == "case_sensitif" ||
                           name == "caseSensitif" || name == "casesensitive" || name == "casesensitif" ||
                           name == "case" || name == "equals" || name == "equal" || name == "str.case_sensitive") {
                    if (argCount == 1) {
                        Value a = *(--top);
                        *top++ = a;
                    } else if (argCount >= 2) {
                        Value b = *(--top);
                        Value a = *(--top);
                        top -= (argCount - 2);
                        *top++ = StandardLibrary::caseSensitive(a.toString(), b.toString());
                    } else {
                        *top++ = Value(false);
                    }
                } else if (name == "print" || name == "io.print") {
                    std::vector<Value> args;
                    for (size_t i = 0; i < argCount; ++i) {
                        args.push_back(*(top - argCount + i));
                    }
                    top -= argCount;
                    *top++ = StandardLibrary::print(args);
                } else if (name == "silent_print") {
                    std::vector<Value> args;
                    for (size_t i = 0; i < argCount; ++i) {
                        args.push_back(*(top - argCount + i));
                    }
                    top -= argCount;
                    *top++ = StandardLibrary::silentPrint(args);
                } else if (name == "input" || name == "io.input" || name == "io.ask" || name == "ask") {
                    requireModule("io", name);
                    std::string prompt;
                    if (argCount > 0) {
                        prompt = (*(top - argCount)).toString();
                        top -= argCount;
                    }
                    *top++ = StandardLibrary::input(prompt);
                } else if (name == "read" || name == "io.read" || name == "readFile" || name == "io.readFile") {
                    requireModule("io", name);
                    Value path = *(--top);
                    *top++ = StandardLibrary::readFile(path.toString());
                } else if (name == "write" || name == "io.write" || name == "writeFile" || name == "io.writeFile") {
                    requireModule("io", name);
                    Value c = *(--top);
                    Value p = *(--top);
                    *top++ = StandardLibrary::writeFile(p.toString(), c.toString());
                } else if (name == "append" || name == "io.append" || name == "appendFile" || name == "io.appendFile") {
                    requireModule("io", name);
                    Value c = *(--top);
                    Value p = *(--top);
                    *top++ = StandardLibrary::appendFile(p.toString(), c.toString());
                } else if (name == "write_lines" || name == "io.write_lines" || name == "writeLines" || name == "io.writeLines") {
                    requireModule("io", name);
                    Value lines = *(--top);
                    Value p = *(--top);
                    *top++ = StandardLibrary::writeLines(p.toString(), lines);
                } else if (name == "open" || name == "io.open") {
                    requireModule("io", name);
                    std::string mode = "w";
                    if (argCount >= 2) {
                        mode = (*(--top)).toString();
                        Value p = *(--top);
                        if (argCount > 2) top -= (argCount - 2);
                        *top++ = StandardLibrary::openFile(p.toString(), mode);
                    } else if (argCount == 1) {
                        Value p = *(--top);
                        *top++ = StandardLibrary::openFile(p.toString(), "w");
                    } else {
                        *top++ = Value();
                    }
                } else if (name == "math.sqrt") {
                    requireModule("math", name);
                    double val = argCount > 0 ? (*(--top)).asFloat() : 0.0;
                    *top++ = StandardLibrary::mathSqrt(val);
                } else if (name == "math.abs") {
                    requireModule("math", name);
                    double val = argCount > 0 ? (*(--top)).asFloat() : 0.0;
                    *top++ = StandardLibrary::mathAbs(val);
                } else if (name == "math.pow") {
                    requireModule("math", name);
                    double exp = (*(--top)).asFloat();
                    double base = (*(--top)).asFloat();
                    *top++ = StandardLibrary::mathPow(base, exp);
                } else if (name == "math.floor") {
                    requireModule("math", name);
                    double val = argCount > 0 ? (*(--top)).asFloat() : 0.0;
                    *top++ = StandardLibrary::mathFloor(val);
                } else if (name == "math.ceil") {
                    requireModule("math", name);
                    double val = argCount > 0 ? (*(--top)).asFloat() : 0.0;
                    *top++ = StandardLibrary::mathCeil(val);
                } else if (name == "math.round") {
                    requireModule("math", name);
                    double val = argCount > 0 ? (*(--top)).asFloat() : 0.0;
                    *top++ = StandardLibrary::mathRound(val);
                } else if (name == "math.min") {
                    requireModule("math", name);
                    double b = (*(--top)).asFloat();
                    double a = (*(--top)).asFloat();
                    *top++ = StandardLibrary::mathMin(a, b);
                } else if (name == "math.max") {
                    requireModule("math", name);
                    double b = (*(--top)).asFloat();
                    double a = (*(--top)).asFloat();
                    *top++ = StandardLibrary::mathMax(a, b);
                } else if (name == "math.random" || name == "random") {
                    requireModule("math", name);
                    if (argCount == 0) {
                        *top++ = StandardLibrary::mathRandom();
                    } else if (argCount == 1) {
                        double max = (*(--top)).asFloat();
                        *top++ = StandardLibrary::mathRandom(max);
                    } else {
                        double b = (*(--top)).asFloat();
                        double a = (*(--top)).asFloat();
                        top -= (argCount - 2);
                        *top++ = StandardLibrary::mathRandom(a, b);
                    }
                } else if (name == "math.random_seed" || name == "math.randomSeed" || name == "math.seed" || name == "random_seed" || name == "seed") {
                    requireModule("math", name);
                    if (argCount == 0) {
                        *top++ = StandardLibrary::mathRandomSeed();
                    } else {
                        int64_t s = (*(--top)).asInt();
                        top -= (argCount - 1);
                        *top++ = StandardLibrary::mathRandomSeed(s);
                    }
                } else if (name == "math.sin") {
                    requireModule("math", name);
                    double val = argCount > 0 ? (*(--top)).asFloat() : 0.0;
                    *top++ = StandardLibrary::mathSin(val);
                } else if (name == "math.cos") {
                    requireModule("math", name);
                    double val = argCount > 0 ? (*(--top)).asFloat() : 0.0;
                    *top++ = StandardLibrary::mathCos(val);
                } else if (name == "math.tan") {
                    requireModule("math", name);
                    double val = argCount > 0 ? (*(--top)).asFloat() : 0.0;
                    *top++ = StandardLibrary::mathTan(val);
                } else if (name == "time.sleep") {
                    requireModule("time", name);
                    int64_t ms = argCount > 0 ? (*(--top)).asInt() : 0;
                    *top++ = StandardLibrary::timeSleep(ms);
                } else if (name == "time.now") {
                    top -= argCount;
                    *top++ = StandardLibrary::timeNow();
                } else if (name == "get" || name == "net.get" || name == "http.get") {
                    if (argCount == 1) {
                        Value u = *(--top);
                        *top++ = StandardLibrary::httpGet(u.toString());
                    } else if (argCount >= 2) {
                        Value prop = *(--top);
                        Value tgt = *(--top);
                        *top++ = tgt.getProperty(prop.toString());
                    }
                } else if (name == "send" || name == "net.send" || name == "http.send") {
                    Value d = *(--top);
                    Value t = *(--top);
                    *top++ = StandardLibrary::httpSend(t.toString(), d.toString());
                } else if (name == "app" || name == "gui.app") {
                    Value t = *(--top);
                    StandardLibrary::setAppTitle(t.toString());
                    *top++ = Value();
                } else if (name == "window" || name == "gui.window") {
                    Value h = *(--top);
                    Value w = *(--top);
                    StandardLibrary::setWindowSize(static_cast<int>(w.asInt()), static_cast<int>(h.asInt()));
                    *top++ = Value();
                } else if (name == "run" || name == "gui.run") {
                    int timeout = argCount > 0 ? static_cast<int>((*(--top)).asInt()) : -1;
                    *top++ = StandardLibrary::runApp(timeout);
                } else {
                    std::string targetName = name;
                    auto git = globals_.find(name);
                    if (git != globals_.end() && git->second.isFunction()) {
                        targetName = git->second.strVal;
                    }
                    auto it = functions_.find(targetName);
                    if (it != functions_.end()) {
                        Chunk* nChunk = it->second.get();
                        curChunk->cachedChunks[fnIdx] = nChunk;
                        if (!nChunk->isJitCompiled && !nChunk->jitAttempted) {
                            JitEngine::instance().compileFunction(nChunk);
                        }
                        if (nChunk->isJitCompiled && nChunk->jitFuncInt && argCount == nChunk->arity) {
                            bool allInt = true;
                            int64_t args[6] = {0};
                            for (int i = 0; i < argCount && i < 6; ++i) {
                                Value& v = *(top - argCount + i);
                                if (v.type != ValueType::INT && v.type != ValueType::BOOL) {
                                    allInt = false;
                                    break;
                                }
                                args[i] = v.asInt();
                            }
                            if (allInt) {
                                int64_t res = 0;
                                typedef int64_t (*Jit0)();
                                typedef int64_t (*Jit1)(int64_t);
                                typedef int64_t (*Jit2)(int64_t, int64_t);
                                typedef int64_t (*Jit3)(int64_t, int64_t, int64_t);
                                typedef int64_t (*Jit4)(int64_t, int64_t, int64_t, int64_t);
                                typedef int64_t (*Jit5)(int64_t, int64_t, int64_t, int64_t, int64_t);
                                typedef int64_t (*Jit6)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

                                switch (argCount) {
                                    case 0: res = ((Jit0)nChunk->jitFuncInt)(); break;
                                    case 1: res = ((Jit1)nChunk->jitFuncInt)(args[0]); break;
                                    case 2: res = ((Jit2)nChunk->jitFuncInt)(args[0], args[1]); break;
                                    case 3: res = ((Jit3)nChunk->jitFuncInt)(args[0], args[1], args[2]); break;
                                    case 4: res = ((Jit4)nChunk->jitFuncInt)(args[0], args[1], args[2], args[3]); break;
                                    case 5: res = ((Jit5)nChunk->jitFuncInt)(args[0], args[1], args[2], args[3], args[4]); break;
                                    case 6: res = ((Jit6)nChunk->jitFuncInt)(args[0], args[1], args[2], args[3], args[4], args[5]); break;
                                }
                                top -= argCount;
                                *top++ = Value(res);
                                break;
                            }
                        }

                        frame->ip = ip;
                        Value* nextSlots = top - argCount;
                        for (int i = argCount; i < nChunk->localsCount; ++i) {
                            *top++ = Value();
                        }
                        CallFrame* newFrame = &frames_[frameCount++];
                        newFrame->chunk = nChunk;
                        newFrame->ip = 0;
                        newFrame->slots = nextSlots;

                        frame = newFrame;
                        curChunk = nChunk;
                        code = curChunk->code.data();
                        ip = 0;
                        slots = nextSlots;
                    } else {
                        size_t dotPos = name.find('.');
                        if (dotPos != std::string::npos) {
                            std::string varName = name.substr(0, dotPos);
                            std::string method = name.substr(dotPos + 1);

                            Value tgt;
                            bool found = false;
                            auto locIt = curChunk->localIndices.find(varName);
                            if (locIt != curChunk->localIndices.end() && slots) {
                                tgt = slots[locIt->second];
                                found = true;
                            } else {
                                auto git = globals_.find(varName);
                                if (git != globals_.end()) {
                                    tgt = git->second;
                                    found = true;
                                }
                            }

                            if (found && tgt.isObject() && tgt.objVal) {
                                auto hIt = tgt.objVal->find("__handle");
                                if (hIt != tgt.objVal->end()) {
                                    int64_t handleId = hIt->second.asInt();
                                    if (method == "write") {
                                        std::string text = argCount > 0 ? (*(--top)).toString() : "";
                                        if (argCount > 1) top -= (argCount - 1);
                                        *top++ = StandardLibrary::fileWrite(handleId, text);
                                        break;
                                    } else if (method == "writeline" || method == "write_line" || method == "writeLine") {
                                        std::string text = argCount > 0 ? (*(--top)).toString() : "";
                                        if (argCount > 1) top -= (argCount - 1);
                                        *top++ = StandardLibrary::fileWriteLine(handleId, text);
                                        break;
                                    } else if (method == "flush") {
                                        if (argCount > 0) top -= argCount;
                                        *top++ = StandardLibrary::fileFlush(handleId);
                                        break;
                                    } else if (method == "close") {
                                        if (argCount > 0) top -= argCount;
                                        *top++ = StandardLibrary::fileClose(handleId);
                                        break;
                                    }
                                }
                            }

                            std::string mod = varName;
                            std::string fnName = method;
                            auto modMembers = Diagnostic::getModuleMembers(mod);
                            std::string memberMatch = Diagnostic::suggestSimilar(fnName, modMembers);
                            if (!memberMatch.empty()) {
                                runtimeError("NameError", "Fungsi '" + name + "' tidak ditemukan pada modul '" + mod + "'.", curChunk, ip, frameCount, "Apakah maksud Anda '" + mod + "." + memberMatch + "'?");
                            }
                        }

                        std::vector<std::string> candidates;
                        for (const auto& fn : functions_) candidates.push_back(fn.first);
                        for (const auto& b : Diagnostic::getBuiltinFunctions()) candidates.push_back(b);
                        std::string fnMatch = Diagnostic::suggestSimilar(name, candidates);
                        std::string rec = fnMatch.empty() ? "" : "Apakah maksud Anda '" + fnMatch + "'?";
                        runtimeError("NameError", "Fungsi '" + name + "' tidak ditemukan.", curChunk, ip, frameCount, rec);
                    }
                }
                break;
            }
            case OpCode::OP_RETURN: {
                Value result = (top > slots) ? *(--top) : Value();
                frameCount--;
                if (frameCount == 0) {
                    return result;
                }
                top = slots;
                *top++ = result;
                frame = &frames_[frameCount - 1];
                curChunk = frame->chunk;
                code = curChunk->code.data();
                ip = frame->ip;
                slots = frame->slots;
                break;
            }
            case OpCode::OP_PRINT: {
                uint8_t argCount = code[ip++];
                std::vector<Value> args;
                for (size_t i = 0; i < argCount; ++i) {
                    args.push_back(*(top - argCount + i));
                }
                top -= argCount;
                *top++ = StandardLibrary::print(args);
                break;
            }
            case OpCode::OP_SILENT_PRINT: {
                uint8_t argCount = code[ip++];
                std::vector<Value> args;
                for (size_t i = 0; i < argCount; ++i) {
                    args.push_back(*(top - argCount + i));
                }
                top -= argCount;
                *top++ = StandardLibrary::silentPrint(args);
                break;
            }
            case OpCode::OP_WRITE: {
                requireModule("io", "write");
                Value content = *(--top);
                Value path = *(--top);
                *top++ = StandardLibrary::writeFile(path.toString(), content.toString());
                break;
            }
            case OpCode::OP_READ: {
                requireModule("io", "read");
                Value path = *(--top);
                *top++ = StandardLibrary::readFile(path.toString());
                break;
            }
            case OpCode::OP_APP: {
                Value title = *(--top);
                StandardLibrary::setAppTitle(title.toString());
                break;
            }
            case OpCode::OP_WINDOW: {
                Value h = *(--top);
                Value w = *(--top);
                StandardLibrary::setWindowSize(static_cast<int>(w.asInt()), static_cast<int>(h.asInt()));
                break;
            }
            case OpCode::OP_RUN: {
                Value dur = *(--top);
                int timeout = dur.isNil() ? -1 : static_cast<int>(dur.asInt());
                *top++ = StandardLibrary::runApp(timeout);
                break;
            }
            case OpCode::OP_NEW:
                *top++ = Value::makeObject();
                break;
            case OpCode::OP_GET_PROP: {
                Value prop = *(--top);
                Value tgt = *(--top);
                std::string propName = prop.toString();
                if (tgt.isObject()) {
                    auto it = tgt.objVal->find(propName);
                    if (it != tgt.objVal->end()) {
                        *top++ = it->second;
                    } else {
                        std::vector<std::string> objProps;
                        for (const auto& kv : *tgt.objVal) objProps.push_back(kv.first);
                        std::string sug = Diagnostic::suggestSimilar(propName, objProps);
                        std::string rec = sug.empty() ? "" : "Apakah maksud Anda '" + sug + "'?";
                        runtimeError("NameError", "Properti atau fungsi '" + propName + "' tidak ditemukan.", curChunk, ip, frameCount, rec);
                    }
                } else {
                    *top++ = tgt.getProperty(propName);
                }
                break;
            }
            case OpCode::OP_SET_PROP: {
                Value val = *(--top);
                Value prop = *(--top);
                Value tgt = *(--top);
                tgt.setProperty(prop.toString(), val);
                *top++ = val;
                break;
            }
            case OpCode::OP_BUILD_LIST: {
                uint16_t count = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                std::vector<Value> list;
                for (size_t i = 0; i < count; ++i) {
                    list.push_back(*(top - count + i));
                }
                top -= count;
                *top++ = Value(list);
                break;
            }
            case OpCode::OP_GET_INDEX: {
                Value idx = *(--top);
                Value tgt = *(--top);
                *top++ = tgt.getIndex(idx);
                break;
            }
            case OpCode::OP_SET_INDEX: {
                Value val = *(--top);
                Value idx = *(--top);
                Value tgt = *(--top);
                tgt.setIndex(idx, val);
                *top++ = val;
                break;
            }
            case OpCode::OP_USE: {
                uint16_t idx = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                const std::string& modName = curChunk->constants[idx].strVal;
                loadLibrary(modName);
                break;
            }
            case OpCode::OP_INC_LOCAL: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                if (__builtin_expect(slots[slot].type == ValueType::INT, 1)) {
                    slots[slot].intVal++;
                } else if (slots[slot].type == ValueType::FLOAT) {
                    slots[slot].floatVal += 1.0;
                } else if (slots[slot].isNil()) {
                    runtimeError("TypeError", "Operasi '+' tidak dapat dilakukan pada 'nil'.", curChunk, ip, frameCount, "", "Pastikan variabel memiliki nilai numerik atau string yang valid.");
                } else {
                    slots[slot] = slots[slot] + Value(static_cast<int64_t>(1));
                }
                break;
            }
            case OpCode::OP_DEC_LOCAL: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                if (__builtin_expect(slots[slot].type == ValueType::INT, 1)) {
                    slots[slot].intVal--;
                } else if (slots[slot].type == ValueType::FLOAT) {
                    slots[slot].floatVal -= 1.0;
                } else if (slots[slot].isNil()) {
                    runtimeError("TypeError", "Operasi '-' tidak dapat dilakukan pada 'nil'.", curChunk, ip, frameCount, "", "Pastikan variabel memiliki nilai numerik yang valid.");
                } else {
                    slots[slot] = slots[slot] - Value(static_cast<int64_t>(1));
                }
                break;
            }
            case OpCode::OP_ADD_LOCAL_INT: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                uint16_t constIdx = static_cast<uint16_t>((code[ip + 2] << 8) | code[ip + 3]);
                ip += 4;
                if (__builtin_expect(slots[slot].type == ValueType::INT, 1)) {
                    slots[slot].intVal += curChunk->constants[constIdx].intVal;
                } else if (slots[slot].isNil()) {
                    runtimeError("TypeError", "Operasi '+' tidak dapat dilakukan pada 'nil'.", curChunk, ip, frameCount, "", "Pastikan variabel memiliki nilai numerik atau string yang valid.");
                } else {
                    slots[slot] = slots[slot] + curChunk->constants[constIdx];
                }
                break;
            }
            case OpCode::OP_SUB_LOCAL_INT: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                uint16_t constIdx = static_cast<uint16_t>((code[ip + 2] << 8) | code[ip + 3]);
                ip += 4;
                if (__builtin_expect(slots[slot].type == ValueType::INT, 1)) {
                    slots[slot].intVal -= curChunk->constants[constIdx].intVal;
                } else if (slots[slot].isNil()) {
                    runtimeError("TypeError", "Operasi '-' tidak dapat dilakukan pada 'nil'.", curChunk, ip, frameCount, "", "Pastikan variabel memiliki nilai numerik yang valid.");
                } else {
                    slots[slot] = slots[slot] - curChunk->constants[constIdx];
                }
                break;
            }
            case OpCode::OP_JUMP_IF_LOCAL_GE_CONST: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                uint16_t constIdx = static_cast<uint16_t>((code[ip + 2] << 8) | code[ip + 3]);
                uint16_t offset = static_cast<uint16_t>((code[ip + 4] << 8) | code[ip + 5]);
                ip += 6;
                if (__builtin_expect(slots[slot].type == ValueType::INT, 1)) {
                    if (slots[slot].intVal >= curChunk->constants[constIdx].intVal) {
                        ip += offset;
                    }
                } else if (slots[slot].isNil()) {
                    runtimeError("TypeError", "Operator '<' tidak dapat membandingkan 'nil' dengan 'int'.", curChunk, ip, frameCount, "", "Periksa apakah variabel bernilai 'nil' sebelum melakukan perbandingan.");
                } else {
                    if (slots[slot] >= curChunk->constants[constIdx]) {
                        ip += offset;
                    }
                }
                break;
            }
            case OpCode::OP_JUMP_IF_LOCAL_GT_CONST: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                uint16_t constIdx = static_cast<uint16_t>((code[ip + 2] << 8) | code[ip + 3]);
                uint16_t offset = static_cast<uint16_t>((code[ip + 4] << 8) | code[ip + 5]);
                ip += 6;
                if (__builtin_expect(slots[slot].type == ValueType::INT, 1)) {
                    if (slots[slot].intVal > curChunk->constants[constIdx].intVal) {
                        ip += offset;
                    }
                } else if (slots[slot].isNil()) {
                    runtimeError("TypeError", "Operator '<=' tidak dapat membandingkan 'nil' dengan 'int'.", curChunk, ip, frameCount, "", "Periksa apakah variabel bernilai 'nil' sebelum melakukan perbandingan.");
                } else {
                    if (slots[slot] > curChunk->constants[constIdx]) {
                        ip += offset;
                    }
                }
                break;
            }
            case OpCode::OP_JUMP_IF_LOCAL_LE_CONST: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                uint16_t constIdx = static_cast<uint16_t>((code[ip + 2] << 8) | code[ip + 3]);
                uint16_t offset = static_cast<uint16_t>((code[ip + 4] << 8) | code[ip + 5]);
                ip += 6;
                if (__builtin_expect(slots[slot].type == ValueType::INT, 1)) {
                    if (slots[slot].intVal <= curChunk->constants[constIdx].intVal) {
                        ip += offset;
                    }
                } else if (slots[slot].isNil()) {
                    runtimeError("TypeError", "Operator '>' tidak dapat membandingkan 'nil' dengan 'int'.", curChunk, ip, frameCount, "", "Periksa apakah variabel bernilai 'nil' sebelum melakukan perbandingan.");
                } else {
                    if (slots[slot] <= curChunk->constants[constIdx]) {
                        ip += offset;
                    }
                }
                break;
            }
            case OpCode::OP_JUMP_IF_LOCAL_LT_CONST: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                uint16_t constIdx = static_cast<uint16_t>((code[ip + 2] << 8) | code[ip + 3]);
                uint16_t offset = static_cast<uint16_t>((code[ip + 4] << 8) | code[ip + 5]);
                ip += 6;
                if (__builtin_expect(slots[slot].type == ValueType::INT, 1)) {
                    if (slots[slot].intVal < curChunk->constants[constIdx].intVal) {
                        ip += offset;
                    }
                } else if (slots[slot].isNil()) {
                    runtimeError("TypeError", "Operator '>=' tidak dapat membandingkan 'nil' dengan 'int'.", curChunk, ip, frameCount, "", "Periksa apakah variabel bernilai 'nil' sebelum melakukan perbandingan.");
                } else {
                    if (slots[slot] < curChunk->constants[constIdx]) {
                        ip += offset;
                    }
                }
                break;
            }
            case OpCode::OP_FAST_LOOP: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                uint16_t offset = static_cast<uint16_t>((code[ip + 2] << 8) | code[ip + 3]);
                ip += 4;
                if (--slots[slot].intVal > 0) {
                    ip -= offset;
                }
                break;
            }
            case OpCode::OP_APPEND_LOCAL: {
                uint16_t slot = static_cast<uint16_t>((code[ip] << 8) | code[ip + 1]);
                ip += 2;
                Value val = *(--top);
                Value& loc = slots[slot];
                if (__builtin_expect(loc.type == ValueType::STRING, 1)) {
                    if (val.type == ValueType::STRING) {
                        loc.strVal.append(val.strVal);
                    } else {
                        loc.strVal.append(val.toString());
                    }
                } else if (loc.type == ValueType::INT && val.type == ValueType::INT) {
                    loc.intVal += val.intVal;
                    loc.floatVal = static_cast<double>(loc.intVal);
                } else if (loc.type == ValueType::LIST && val.type == ValueType::LIST) {
                    if (val.listVal) {
                        if (!loc.listVal) loc.listVal = std::make_shared<std::vector<Value>>();
                        loc.listVal->insert(loc.listVal->end(), val.listVal->begin(), val.listVal->end());
                    }
                } else {
                    loc = loc + val;
                }
                break;
            }
        }
    }
}
