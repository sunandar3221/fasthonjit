#include "Lexer.hpp"
#include "Parser.hpp"
#include "Interpreter.hpp"
#include "BytecodeCompiler.hpp"
#include "VM.hpp"
#include "Jit.hpp"
#include "AotGenerator.hpp"
#include "Repl.hpp"
#include "StandardLibrary.hpp"
#include "Diagnostic.hpp"
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    if (argc == 1) {
        Repl repl;
        repl.run();
        return 0;
    }

    std::string arg1 = argv[1];
    if (arg1 == "-v" || arg1 == "--version" || arg1 == "version") {
        std::cout << "FasthonJIT v2.0.0 (Ultra High-Performance JIT Engine)\n";
        return 0;
    }
    if (arg1 == "--jit-info" || arg1 == "-j") {
        JitEngine::instance().printJitInfo();
        return 0;
    }
    if (arg1 == "-h" || arg1 == "--help" || arg1 == "help") {
        std::cout << "Usage: fasthonjit [script.fsn] [--bench] [--no-jit] | build <script.fsn> [-o output] [--target <windows|linux|android>] | --jit-info | --version\n";
        return 0;
    }

    if (arg1 == "build" || arg1 == "-c" || arg1 == "--compile") {
        if (argc < 3) {
            std::cerr << "Usage: fasthonjit build <input.fsn> [-o <output>] [--target <windows|linux|android>]\n";
            return 1;
        }

        std::string inputFile;
        std::string outputFile;
        std::string target;

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
                outputFile = argv[++i];
            } else if ((arg == "-t" || arg == "--target") && i + 1 < argc) {
                target = argv[++i];
            } else if (arg.rfind("--target=", 0) == 0) {
                target = arg.substr(9);
            } else if (arg == "--linux") {
                target = "linux";
            } else if (arg == "--android" || arg == "--termux") {
                target = "android";
            } else if (arg == "--windows" || arg == "--win") {
                target = "windows";
            } else if (inputFile.empty() && !arg.empty() && arg[0] != '-') {
                inputFile = arg;
            }
        }

        if (inputFile.empty()) {
            std::cerr << "Usage: fasthonjit build <input.fsn> [-o <output>] [--target <windows|linux|android>]\n";
            return 1;
        }

        std::string normTarget = target;
        for (char& c : normTarget) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (outputFile.empty()) {
            std::string base = inputFile;
            size_t slash = base.find_last_of("/\\");
            if (slash != std::string::npos) base = base.substr(slash + 1);
            if (base.size() > 4 && (base.substr(base.size() - 4) == ".fsn" || base.substr(base.size() - 4) == ".eas")) {
                base = base.substr(0, base.size() - 4);
            }
            if (normTarget == "windows" || normTarget == "win" || normTarget == "win64") {
                outputFile = base + ".exe";
            } else if (normTarget == "linux" || normTarget == "android" || normTarget == "termux") {
                outputFile = base;
            } else {
#ifdef _WIN32
                outputFile = base + ".exe";
#else
                outputFile = base;
#endif
            }
        }

        Value content = StandardLibrary::readFile(inputFile);
        if (content.strVal.empty()) {
            std::cerr << "Error: Could not read file " << inputFile << "\n";
            return 1;
        }

        Diagnostic::setSource(inputFile, content.strVal);

        Lexer lexer(content.strVal);
        auto tokens = lexer.tokenize();
        if (lexer.hasErrors()) {
            for (const auto& err : lexer.getErrors()) {
                std::cerr << err << "\n";
            }
            return 1;
        }

        Parser parser(std::move(tokens));
        auto program = parser.parseProgram();
        if (parser.hasErrors()) {
            for (const auto& err : parser.getErrors()) {
                std::cerr << err << "\n";
            }
            return 1;
        }

        AotGenerator aot;
        std::string cppCode = aot.generateCpp(program.get());

        std::string tempCpp = "_fasthonjit_build_temp.cpp";
        std::ofstream outCpp(tempCpp);
        if (!outCpp.is_open()) {
            std::cerr << "Error: Could not create temporary AOT source file.\n";
            return 1;
        }
        outCpp << cppCode;
        outCpp.close();

        bool ok = aot.buildBinary(tempCpp, outputFile, target);
        std::remove(tempCpp.c_str());
        if (!ok) {
            std::cerr << "Error: AOT Compilation failed.\n";
            return 1;
        }

        std::string targetMsg = target.empty() ? "" : (" (" + target + ")");
        std::cout << "Successfully compiled " << inputFile << " -> " << outputFile << targetMsg << "\n";
        return 0;
    }

    bool enableJit = true;
    std::string scriptPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--no-jit") {
            enableJit = false;
        } else if (a == "--jit") {
            enableJit = true;
        } else if (a == "--bench") {
            VM::setBenchMode(true);
        } else if (a == "run" || a == "--vm" || a == "-v") {
            // verb or flag
        } else if (scriptPath.empty() && !a.empty() && a[0] != '-') {
            scriptPath = a;
        }
    }
    JitEngine::instance().setEnabled(enableJit);

    if (scriptPath.empty()) {
        std::cerr << "Usage: fasthonjit <script.fsn>\n";
        return 1;
    }

    Value content = StandardLibrary::readFile(scriptPath);
    if (content.strVal.empty()) {
        if (scriptPath.find('.') == std::string::npos) {
            std::string tryFsn = scriptPath + ".fsn";
            content = StandardLibrary::readFile(tryFsn);
            if (!content.strVal.empty()) {
                scriptPath = tryFsn;
            } else {
                std::string tryEas = scriptPath + ".eas";
                content = StandardLibrary::readFile(tryEas);
                if (!content.strVal.empty()) {
                    scriptPath = tryEas;
                }
            }
        }
    }
    if (content.strVal.empty()) {
        std::ifstream testOpen(scriptPath);
        if (!testOpen.good()) {
            std::cerr << "Error: Cannot open file '" << scriptPath << "'\n";
            return 1;
        }
    }

    Diagnostic::setSource(scriptPath, content.strVal);

    try {
        Lexer lexer(content.strVal);
        auto tokens = lexer.tokenize();
        if (lexer.hasErrors()) {
            for (const auto& err : lexer.getErrors()) {
                std::cerr << err << "\n";
            }
            return 1;
        }

        Parser parser(std::move(tokens));
        auto program = parser.parseProgram();
        if (parser.hasErrors()) {
            for (const auto& err : parser.getErrors()) {
                std::cerr << err << "\n";
            }
            return 1;
        }

        BytecodeCompiler compiler;
        auto mainChunk = compiler.compile(program.get());

        VM vm;
        vm.registerFunctions(compiler.getFunctions());
        vm.run(mainChunk.get());
    } catch (const std::exception& ex) {
        std::string msg = ex.what();
        if (msg.find("\x1b[") != std::string::npos || msg.find("───") != std::string::npos ||
            msg.rfind("File \"", 0) == 0 || msg.find("Error:") != std::string::npos ||
            msg.rfind("Runtime Error", 0) == 0 || msg.rfind("Syntax Error", 0) == 0) {
            std::cerr << msg << "\n";
        } else {
            std::cerr << "Runtime Error: " << msg << "\n";
        }
        return 1;
    }

    return 0;
}
