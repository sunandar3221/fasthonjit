#include "Interpreter.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Diagnostic.hpp"
#include <fstream>
#include <limits>

struct ReturnException {
    Value value;
    ReturnException(Value val) : value(std::move(val)) {}
};

struct BreakException {};
struct ContinueException {};

Interpreter::Interpreter()
    : globalEnv_(std::make_shared<Environment>()),
      currentEnv_(globalEnv_) {
    registerBuiltins();
}

Interpreter::Interpreter(std::shared_ptr<Environment> env)
    : globalEnv_(std::move(env)),
      currentEnv_(globalEnv_) {
    registerBuiltins();
}

void Interpreter::registerBuiltins() {
    globalEnv_->define("true", Value(true));
    globalEnv_->define("false", Value(false));
    globalEnv_->define("nil", Value());
}

bool Interpreter::isLibraryLoaded(const std::string& name) const {
    return loadedLibraries_.find(name) != loadedLibraries_.end();
}

void Interpreter::loadLibrary(const std::string& name) {
    loadedLibraries_.insert(name);
    if (name == "io") {
        Value ioObj = Value::makeObject();
        ioObj.setProperty("input", Value(ValueType::FUNCTION, "io.input"));
        ioObj.setProperty("ask", Value(ValueType::FUNCTION, "io.ask"));
        ioObj.setProperty("read", Value(ValueType::FUNCTION, "io.read"));
        ioObj.setProperty("write", Value(ValueType::FUNCTION, "io.write"));
        ioObj.setProperty("print", Value(ValueType::FUNCTION, "print"));
        globalEnv_->assign("io", ioObj);
        globalEnv_->assign("input", Value(ValueType::FUNCTION, "input"));
        globalEnv_->assign("ask", Value(ValueType::FUNCTION, "ask"));
    } else if (name == "math") {
        Value mathObj = Value::makeObject();
        mathObj.setProperty("pi", Value(3.141592653589793));
        mathObj.setProperty("e", Value(2.718281828459045));
        mathObj.setProperty("sqrt", Value(ValueType::FUNCTION, "math.sqrt"));
        mathObj.setProperty("sin", Value(ValueType::FUNCTION, "math.sin"));
        mathObj.setProperty("cos", Value(ValueType::FUNCTION, "math.cos"));
        mathObj.setProperty("tan", Value(ValueType::FUNCTION, "math.tan"));
        mathObj.setProperty("asin", Value(ValueType::FUNCTION, "math.asin"));
        mathObj.setProperty("acos", Value(ValueType::FUNCTION, "math.acos"));
        mathObj.setProperty("atan", Value(ValueType::FUNCTION, "math.atan"));
        mathObj.setProperty("atan2", Value(ValueType::FUNCTION, "math.atan2"));
        mathObj.setProperty("sinh", Value(ValueType::FUNCTION, "math.sinh"));
        mathObj.setProperty("cosh", Value(ValueType::FUNCTION, "math.cosh"));
        mathObj.setProperty("tanh", Value(ValueType::FUNCTION, "math.tanh"));
        mathObj.setProperty("exp", Value(ValueType::FUNCTION, "math.exp"));
        mathObj.setProperty("log", Value(ValueType::FUNCTION, "math.log"));
        mathObj.setProperty("log10", Value(ValueType::FUNCTION, "math.log10"));
        mathObj.setProperty("log2", Value(ValueType::FUNCTION, "math.log2"));
        mathObj.setProperty("degrees", Value(ValueType::FUNCTION, "math.degrees"));
        mathObj.setProperty("radians", Value(ValueType::FUNCTION, "math.radians"));
        mathObj.setProperty("hypot", Value(ValueType::FUNCTION, "math.hypot"));
        mathObj.setProperty("trunc", Value(ValueType::FUNCTION, "math.trunc"));
        mathObj.setProperty("clamp", Value(ValueType::FUNCTION, "math.clamp"));
        mathObj.setProperty("abs", Value(ValueType::FUNCTION, "math.abs"));
        mathObj.setProperty("floor", Value(ValueType::FUNCTION, "math.floor"));
        mathObj.setProperty("ceil", Value(ValueType::FUNCTION, "math.ceil"));
        mathObj.setProperty("round", Value(ValueType::FUNCTION, "math.round"));
        mathObj.setProperty("min", Value(ValueType::FUNCTION, "math.min"));
        mathObj.setProperty("max", Value(ValueType::FUNCTION, "math.max"));
        mathObj.setProperty("pow", Value(ValueType::FUNCTION, "math.pow"));
        mathObj.setProperty("random", Value(ValueType::FUNCTION, "math.random"));
        mathObj.setProperty("random_seed", Value(ValueType::FUNCTION, "math.random_seed"));
        mathObj.setProperty("randomSeed", Value(ValueType::FUNCTION, "math.random_seed"));
        mathObj.setProperty("seed", Value(ValueType::FUNCTION, "math.random_seed"));
        mathObj.setProperty("pi", Value(3.14159265358979323846));
        mathObj.setProperty("e", Value(2.71828182845904523536));
        mathObj.setProperty("tau", Value(6.28318530717958647692));
        mathObj.setProperty("inf", Value(std::numeric_limits<double>::infinity()));
        globalEnv_->assign("math", mathObj);
        globalEnv_->assign("random", Value(ValueType::FUNCTION, "math.random"));
        globalEnv_->assign("random_seed", Value(ValueType::FUNCTION, "math.random_seed"));
        globalEnv_->assign("seed", Value(ValueType::FUNCTION, "math.random_seed"));
    } else if (name == "time") {
        Value timeObj = Value::makeObject();
        timeObj.setProperty("now", Value(ValueType::FUNCTION, "time.now"));
        timeObj.setProperty("sleep", Value(ValueType::FUNCTION, "time.sleep"));
        timeObj.setProperty("sleep_s", Value(ValueType::FUNCTION, "time.sleep_s"));
        timeObj.setProperty("time", Value(ValueType::FUNCTION, "time.time"));
        timeObj.setProperty("time_ns", Value(ValueType::FUNCTION, "time.time_ns"));
        timeObj.setProperty("format", Value(ValueType::FUNCTION, "time.format"));
        globalEnv_->assign("time", timeObj);
    } else if (name == "net" || name == "http") {
        Value netObj = Value::makeObject();
        netObj.setProperty("get", Value(ValueType::FUNCTION, "get"));
        netObj.setProperty("send", Value(ValueType::FUNCTION, "send"));
        globalEnv_->assign(name, netObj);
    } else if (name == "gui") {
        Value guiObj = Value::makeObject();
        guiObj.setProperty("app", Value(ValueType::FUNCTION, "app"));
        guiObj.setProperty("window", Value(ValueType::FUNCTION, "window"));
        guiObj.setProperty("run", Value(ValueType::FUNCTION, "run"));
        globalEnv_->assign("gui", guiObj);
    } else if (name == "str" || name == "string") {
        Value strObj = Value::makeObject();
        strObj.setProperty("lower", Value(ValueType::FUNCTION, "lower"));
        strObj.setProperty("upper", Value(ValueType::FUNCTION, "upper"));
        strObj.setProperty("case_sensitive", Value(ValueType::FUNCTION, "case_sensitive"));
        strObj.setProperty("incase_sensitive", Value(ValueType::FUNCTION, "incase_sensitive"));
        strObj.setProperty("split", Value(ValueType::FUNCTION, "str.split"));
        strObj.setProperty("join", Value(ValueType::FUNCTION, "str.join"));
        strObj.setProperty("replace", Value(ValueType::FUNCTION, "str.replace"));
        strObj.setProperty("trim", Value(ValueType::FUNCTION, "str.trim"));
        strObj.setProperty("startswith", Value(ValueType::FUNCTION, "str.startswith"));
        strObj.setProperty("endswith", Value(ValueType::FUNCTION, "str.endswith"));
        strObj.setProperty("contains", Value(ValueType::FUNCTION, "str.contains"));
        strObj.setProperty("find", Value(ValueType::FUNCTION, "str.find"));
        globalEnv_->assign("str", strObj);
        globalEnv_->assign("string", strObj);
    } else if (name == "os") {
        Value osObj = Value::makeObject();
        osObj.setProperty("name", StandardLibrary::osName());
        osObj.setProperty("sep", StandardLibrary::osSep());
        osObj.setProperty("linesep", StandardLibrary::osLinesep());
        osObj.setProperty("devnull", StandardLibrary::osDevnull());
        osObj.setProperty("getcwd", Value(ValueType::FUNCTION, "os.getcwd"));
        osObj.setProperty("chdir", Value(ValueType::FUNCTION, "os.chdir"));
        osObj.setProperty("listdir", Value(ValueType::FUNCTION, "os.listdir"));
        osObj.setProperty("mkdir", Value(ValueType::FUNCTION, "os.mkdir"));
        osObj.setProperty("makedirs", Value(ValueType::FUNCTION, "os.makedirs"));
        osObj.setProperty("remove", Value(ValueType::FUNCTION, "os.remove"));
        osObj.setProperty("unlink", Value(ValueType::FUNCTION, "os.remove"));
        osObj.setProperty("rmdir", Value(ValueType::FUNCTION, "os.rmdir"));
        osObj.setProperty("removedirs", Value(ValueType::FUNCTION, "os.removedirs"));
        osObj.setProperty("rmtree", Value(ValueType::FUNCTION, "os.rmtree"));
        osObj.setProperty("rename", Value(ValueType::FUNCTION, "os.rename"));
        osObj.setProperty("system", Value(ValueType::FUNCTION, "os.system"));
        osObj.setProperty("popen", Value(ValueType::FUNCTION, "os.popen"));
        osObj.setProperty("getenv", Value(ValueType::FUNCTION, "os.getenv"));
        osObj.setProperty("setenv", Value(ValueType::FUNCTION, "os.setenv"));
        osObj.setProperty("putenv", Value(ValueType::FUNCTION, "os.setenv"));
        osObj.setProperty("unsetenv", Value(ValueType::FUNCTION, "os.unsetenv"));
        osObj.setProperty("getpid", Value(ValueType::FUNCTION, "os.getpid"));
        osObj.setProperty("cpu_count", Value(ValueType::FUNCTION, "os.cpu_count"));
        osObj.setProperty("exit", Value(ValueType::FUNCTION, "os.exit"));
        osObj.setProperty("exists", Value(ValueType::FUNCTION, "os.path.exists"));
        osObj.setProperty("isfile", Value(ValueType::FUNCTION, "os.path.isfile"));
        osObj.setProperty("isdir", Value(ValueType::FUNCTION, "os.path.isdir"));
        osObj.setProperty("getsize", Value(ValueType::FUNCTION, "os.path.getsize"));

        Value pathObj = Value::makeObject();
        pathObj.setProperty("exists", Value(ValueType::FUNCTION, "os.path.exists"));
        pathObj.setProperty("isfile", Value(ValueType::FUNCTION, "os.path.isfile"));
        pathObj.setProperty("isdir", Value(ValueType::FUNCTION, "os.path.isdir"));
        pathObj.setProperty("getsize", Value(ValueType::FUNCTION, "os.path.getsize"));
        pathObj.setProperty("join", Value(ValueType::FUNCTION, "os.path.join"));
        pathObj.setProperty("basename", Value(ValueType::FUNCTION, "os.path.basename"));
        pathObj.setProperty("dirname", Value(ValueType::FUNCTION, "os.path.dirname"));
        pathObj.setProperty("abspath", Value(ValueType::FUNCTION, "os.path.abspath"));
        pathObj.setProperty("splitext", Value(ValueType::FUNCTION, "os.path.splitext"));
        pathObj.setProperty("isabs", Value(ValueType::FUNCTION, "os.path.isabs"));

        osObj.setProperty("path", pathObj);
        globalEnv_->assign("os", osObj);
        globalEnv_->assign("path", pathObj);
    } else if (name == "path") {
        Value pathObj = Value::makeObject();
        pathObj.setProperty("exists", Value(ValueType::FUNCTION, "os.path.exists"));
        pathObj.setProperty("isfile", Value(ValueType::FUNCTION, "os.path.isfile"));
        pathObj.setProperty("isdir", Value(ValueType::FUNCTION, "os.path.isdir"));
        pathObj.setProperty("getsize", Value(ValueType::FUNCTION, "os.path.getsize"));
        pathObj.setProperty("join", Value(ValueType::FUNCTION, "os.path.join"));
        pathObj.setProperty("basename", Value(ValueType::FUNCTION, "os.path.basename"));
        pathObj.setProperty("dirname", Value(ValueType::FUNCTION, "os.path.dirname"));
        pathObj.setProperty("abspath", Value(ValueType::FUNCTION, "os.path.abspath"));
        pathObj.setProperty("splitext", Value(ValueType::FUNCTION, "os.path.splitext"));
        pathObj.setProperty("isabs", Value(ValueType::FUNCTION, "os.path.isabs"));
        globalEnv_->assign("path", pathObj);
    } else if (name == "sys") {
        Value sysObj = Value::makeObject();
        sysObj.setProperty("platform", StandardLibrary::sysPlatform());
        sysObj.setProperty("version", StandardLibrary::sysVersion());
        sysObj.setProperty("argv", StandardLibrary::sysArgv());
        sysObj.setProperty("exit", Value(ValueType::FUNCTION, "sys.exit"));
        sysObj.setProperty("getpid", Value(ValueType::FUNCTION, "sys.getpid"));
        sysObj.setProperty("stdout_write", Value(ValueType::FUNCTION, "sys.stdout_write"));
        sysObj.setProperty("stderr_write", Value(ValueType::FUNCTION, "sys.stderr_write"));
        globalEnv_->assign("sys", sysObj);
    } else if (name == "json") {
        Value jsonObj = Value::makeObject();
        jsonObj.setProperty("dumps", Value(ValueType::FUNCTION, "json.dumps"));
        jsonObj.setProperty("loads", Value(ValueType::FUNCTION, "json.loads"));
        jsonObj.setProperty("stringify", Value(ValueType::FUNCTION, "json.dumps"));
        jsonObj.setProperty("parse", Value(ValueType::FUNCTION, "json.loads"));
        globalEnv_->assign("json", jsonObj);
    }
}

std::shared_ptr<Environment> Interpreter::getGlobalEnvironment() const {
    return globalEnv_;
}

Value Interpreter::interpret(BlockStmt* program) {
    if (!program) return Value();
    try {
        return executeBlock(program, currentEnv_);
    } catch (const ReturnException& ret) {
        return ret.value;
    }
}

Value Interpreter::executeBlock(BlockStmt* block, std::shared_ptr<Environment> env) {
    if (!block) return Value();
    std::shared_ptr<Environment> previous = currentEnv_;
    if (env) {
        currentEnv_ = env;
    }
    Value lastValue;
    try {
        for (const auto& stmt : block->statements) {
            lastValue = execute(stmt.get());
        }
    } catch (...) {
        currentEnv_ = previous;
        throw;
    }
    currentEnv_ = previous;
    return lastValue;
}

Value Interpreter::execute(Stmt* stmt) {
    if (!stmt) return Value();

    if (auto* exprStmt = dynamic_cast<ExprStmt*>(stmt)) {
        return evaluate(exprStmt->expression.get());
    }

    if (auto* retStmt = dynamic_cast<ReturnStmt*>(stmt)) {
        Value val = retStmt->value ? evaluate(retStmt->value.get()) : Value();
        throw ReturnException(val);
    }

    if (dynamic_cast<BreakStmt*>(stmt)) {
        throw BreakException();
    }

    if (dynamic_cast<ContinueStmt*>(stmt)) {
        throw ContinueException();
    }

    if (auto* assignStmt = dynamic_cast<AssignStmt*>(stmt)) {
        Value val = evaluate(assignStmt->value.get());
        currentEnv_->assign(assignStmt->name, val);
        return val;
    }

    if (auto* idxAssign = dynamic_cast<IndexAssignStmt*>(stmt)) {
        Value targetVal = evaluate(idxAssign->target.get());
        Value idxVal = evaluate(idxAssign->index.get());
        Value val = evaluate(idxAssign->value.get());
        targetVal.setIndex(idxVal, val);
        return val;
    }

    if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt)) {
        Value condVal = evaluate(ifStmt->condition.get());
        if (condVal.isTruthy()) {
            return executeBlock(ifStmt->thenBranch.get(), currentEnv_);
        } else if (ifStmt->elseBranch) {
            return executeBlock(ifStmt->elseBranch.get(), currentEnv_);
        }
        return Value();
    }

    if (auto* loopStmt = dynamic_cast<LoopStmt*>(stmt)) {
        Value cntVal = evaluate(loopStmt->count.get());
        int64_t n = cntVal.asInt();
        Value lastVal;
        for (int64_t i = 0; i < n; ++i) {
            try {
                lastVal = executeBlock(loopStmt->body.get(), currentEnv_);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                continue;
            }
        }
        return lastVal;
    }

    if (auto* whileStmt = dynamic_cast<WhileStmt*>(stmt)) {
        Value lastVal;
        while (evaluate(whileStmt->condition.get()).isTruthy()) {
            try {
                lastVal = executeBlock(whileStmt->body.get(), currentEnv_);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                continue;
            }
        }
        return lastVal;
    }

    if (auto* fnDecl = dynamic_cast<FnDeclStmt*>(stmt)) {
        FunctionDef fnDef;
        fnDef.name = fnDecl->name;
        fnDef.params = fnDecl->params;
        fnDef.body = std::shared_ptr<BlockStmt>(std::move(fnDecl->body));
        fnDef.closure = currentEnv_;
        currentEnv_->defineFunction(fnDef.name, fnDef);
        return Value();
    }

    if (auto* printStmt = dynamic_cast<PrintStmt*>(stmt)) {
        std::vector<Value> args;
        for (const auto& a : printStmt->arguments) {
            args.push_back(evaluate(a.get()));
        }
        if (printStmt->silent) {
            return StandardLibrary::silentPrint(args);
        }
        StandardLibrary::print(args);
        return Value();
    }

    if (auto* writeStmt = dynamic_cast<WriteStmt*>(stmt)) {
        if (!isLibraryLoaded("io")) {
            throw std::runtime_error("Library 'io' is not loaded. Please use 'use io' or 'import io' first.");
        }
        Value pathVal = evaluate(writeStmt->path.get());
        Value contentVal = evaluate(writeStmt->content.get());
        return StandardLibrary::writeFile(pathVal.toString(), contentVal.toString());
    }

    if (auto* appStmt = dynamic_cast<AppStmt*>(stmt)) {
        Value titleVal = evaluate(appStmt->title.get());
        StandardLibrary::setAppTitle(titleVal.toString());
        return Value();
    }

    if (auto* winStmt = dynamic_cast<WindowStmt*>(stmt)) {
        Value wVal = evaluate(winStmt->width.get());
        Value hVal = evaluate(winStmt->height.get());
        StandardLibrary::setWindowSize(static_cast<int>(wVal.asInt()), static_cast<int>(hVal.asInt()));
        return Value();
    }

    if (auto* runStmt = dynamic_cast<RunStmt*>(stmt)) {
        int timeout = -1;
        if (runStmt->duration) {
            timeout = static_cast<int>(evaluate(runStmt->duration.get()).asInt());
        }
        return StandardLibrary::runApp(timeout);
    }

    if (auto* setStmt = dynamic_cast<SetStmt*>(stmt)) {
        Value targetVal = evaluate(setStmt->target.get());
        Value propVal = evaluate(setStmt->property.get());
        Value val = evaluate(setStmt->value.get());
        targetVal.setProperty(propVal.toString(), val);
        return val;
    }

    if (auto* useStmt = dynamic_cast<UseStmt*>(stmt)) {
        std::string mod = useStmt->moduleName;
        if (mod == "io" || mod == "math" || mod == "time" || mod == "net" || mod == "http" || mod == "gui" || mod == "str" || mod == "string" || mod == "os" || mod == "sys" || mod == "json" || mod == "path") {
            loadLibrary(mod);
            return Value(true);
        }
        std::string filename = mod;
        if (filename.size() < 4 || (filename.substr(filename.size() - 4) != ".fsn" && filename.substr(filename.size() - 4) != ".eas")) {
            std::ifstream testFsn(filename + ".fsn");
            if (testFsn.good()) {
                filename += ".fsn";
            } else {
                filename += ".eas";
            }
        }
        Value fileContent = StandardLibrary::readFile(filename);
        if (fileContent.strVal.empty()) {
            return Value(false);
        }
        Lexer modLexer(fileContent.strVal);
        auto modTokens = modLexer.tokenize();
        Parser modParser(std::move(modTokens));
        auto modAst = modParser.parseProgram();
        return interpret(modAst.get());
    }

    if (auto* blockStmt = dynamic_cast<BlockStmt*>(stmt)) {
        return executeBlock(blockStmt, currentEnv_);
    }

    return Value();
}

Value Interpreter::evaluate(Expr* expr) {
    if (!expr) return Value();

    if (auto* lit = dynamic_cast<LiteralExpr*>(expr)) {
        return lit->value;
    }

    if (auto* var = dynamic_cast<VarExpr*>(expr)) {
        if ((var->name == "io" || var->name == "input" || var->name == "ask") && !isLibraryLoaded("io")) {
            throw std::runtime_error("Library 'io' is not loaded. Please use 'use io' or 'import io' first.");
        }
        if ((var->name == "read" || var->name == "write") && !isLibraryLoaded("io")) {
            throw std::runtime_error("Function '" + var->name + "' requires library 'io'. Please use 'use io' or 'import io' first.");
        }
        if (var->name == "math" && !isLibraryLoaded("math")) {
            throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' or 'import math' first.");
        }
        if (var->name == "time" && !isLibraryLoaded("time")) {
            throw std::runtime_error("Library 'time' is not loaded. Please use 'use time' or 'import time' first.");
        }
        Value val;
        if (currentEnv_->get(var->name, val)) {
            return val;
        }
        FunctionDef fnDef;
        if (currentEnv_->getFunction(var->name, fnDef)) {
            return Value(ValueType::FUNCTION, var->name);
        }
        if (var->name == "true") return Value(true);
        if (var->name == "false") return Value(false);
        if (var->name == "nil" || var->name == "null") return Value();
        if (var->name == "print" || var->name == "silent_print" || var->name == "app" || var->name == "window" || var->name == "run" || var->name == "len" || var->name == "get" || var->name == "send") {
            return Value(ValueType::FUNCTION, var->name);
        }
        throw std::runtime_error("Undefined variable '" + var->name + "' at line " + std::to_string(var->line));
    }

    if (auto* listLit = dynamic_cast<ListLiteralExpr*>(expr)) {
        std::vector<Value> elems;
        for (const auto& el : listLit->elements) {
            elems.push_back(evaluate(el.get()));
        }
        return Value(elems);
    }

    if (auto* idxExpr = dynamic_cast<IndexExpr*>(expr)) {
        Value targetVal = evaluate(idxExpr->target.get());
        Value indexVal = evaluate(idxExpr->index.get());
        return targetVal.getIndex(indexVal);
    }

    if (auto* bin = dynamic_cast<BinaryExpr*>(expr)) {
        Value left = evaluate(bin->left.get());
        Value right = evaluate(bin->right.get());

        switch (bin->op) {
            case TokenType::PLUS: {
                if ((left.isNil() || right.isNil()) && !left.isString() && !right.isString()) {
                    throw std::runtime_error(Diagnostic::format("TypeError", "Operasi '+' tidak dapat dilakukan pada 'nil'.", bin->line, bin->column, 1, "", "Pastikan variabel memiliki nilai numerik atau string yang valid."));
                }
                return left + right;
            }
            case TokenType::MINUS: {
                if (left.isNil() || right.isNil()) {
                    throw std::runtime_error(Diagnostic::format("TypeError", "Operasi '-' tidak dapat dilakukan pada 'nil'.", bin->line, bin->column, 1, "", "Pastikan variabel memiliki nilai numerik yang valid."));
                }
                return left - right;
            }
            case TokenType::STAR: {
                if (left.isNil() || right.isNil()) {
                    throw std::runtime_error(Diagnostic::format("TypeError", "Operasi '*' tidak dapat dilakukan pada 'nil'.", bin->line, bin->column, 1, "", "Pastikan variabel memiliki nilai numerik yang valid."));
                }
                return left * right;
            }
            case TokenType::SLASH: {
                if (left.isNil() || right.isNil()) {
                    throw std::runtime_error(Diagnostic::format("TypeError", "Operasi '/' tidak dapat dilakukan pada 'nil'.", bin->line, bin->column, 1, "", "Pastikan variabel memiliki nilai numerik yang valid."));
                }
                if (right.asFloat() == 0.0) {
                    throw std::runtime_error(Diagnostic::format("ZeroDivisionError", "Pembagian dengan angka nol tidak diperbolehkan (division by zero).", bin->line, bin->column, 1));
                }
                return left / right;
            }
            case TokenType::PERCENT: {
                if (left.isNil() || right.isNil()) {
                    throw std::runtime_error(Diagnostic::format("TypeError", "Operasi '%' tidak dapat dilakukan pada 'nil'.", bin->line, bin->column, 1, "", "Pastikan variabel memiliki nilai numerik yang valid."));
                }
                if (right.asInt() == 0) {
                    throw std::runtime_error(Diagnostic::format("ZeroDivisionError", "Operasi modulo dengan angka nol tidak diperbolehkan (modulo by zero).", bin->line, bin->column, 1));
                }
                return left % right;
            }
            case TokenType::EQUAL_EQUAL: return Value(left == right);
            case TokenType::BANG_EQUAL: return Value(left != right);
            case TokenType::LESS: {
                if (left.isNil() || right.isNil()) {
                    throw std::runtime_error(Diagnostic::format("TypeError", "Operator '<' tidak dapat membandingkan '" + left.getTypeName() + "' dengan '" + right.getTypeName() + "'.", bin->line, bin->column, 1, "", "Periksa apakah variabel bernilai 'nil' sebelum melakukan perbandingan."));
                }
                return Value(left < right);
            }
            case TokenType::GREATER: {
                if (left.isNil() || right.isNil()) {
                    throw std::runtime_error(Diagnostic::format("TypeError", "Operator '>' tidak dapat membandingkan '" + left.getTypeName() + "' dengan '" + right.getTypeName() + "'.", bin->line, bin->column, 1, "", "Periksa apakah variabel bernilai 'nil' sebelum melakukan perbandingan."));
                }
                return Value(left > right);
            }
            case TokenType::LESS_EQUAL: {
                if (left.isNil() || right.isNil()) {
                    throw std::runtime_error(Diagnostic::format("TypeError", "Operator '<=' tidak dapat membandingkan '" + left.getTypeName() + "' dengan '" + right.getTypeName() + "'.", bin->line, bin->column, 1, "", "Periksa apakah variabel bernilai 'nil' sebelum melakukan perbandingan."));
                }
                return Value(left <= right);
            }
            case TokenType::GREATER_EQUAL: {
                if (left.isNil() || right.isNil()) {
                    throw std::runtime_error(Diagnostic::format("TypeError", "Operator '>=' tidak dapat membandingkan '" + left.getTypeName() + "' dengan '" + right.getTypeName() + "'.", bin->line, bin->column, 1, "", "Periksa apakah variabel bernilai 'nil' sebelum melakukan perbandingan."));
                }
                return Value(left >= right);
            }
            case TokenType::AND: return Value(left.isTruthy() && right.isTruthy());
            case TokenType::OR: return Value(left.isTruthy() || right.isTruthy());
            default: return Value();
        }
    }

    if (auto* un = dynamic_cast<UnaryExpr*>(expr)) {
        Value right = evaluate(un->right.get());
        if (un->op == TokenType::MINUS) {
            return Value(static_cast<int64_t>(0)) - right;
        }
        if (un->op == TokenType::NOT) {
            return Value(!right.isTruthy());
        }
        return right;
    }

    if (auto* call = dynamic_cast<CallExpr*>(expr)) {
        const std::string& name = call->callee;

        if (name == "len" && !call->arguments.empty()) {
            Value arg = evaluate(call->arguments[0].get());
            if (arg.isList()) return Value(static_cast<int64_t>(arg.listVal ? arg.listVal->size() : 0));
            if (arg.isString()) return Value(static_cast<int64_t>(arg.strVal.size()));
            if (arg.isObject()) return Value(static_cast<int64_t>(arg.objVal ? arg.objVal->size() : 0));
            return Value(static_cast<int64_t>(0));
        }

        if (name == "silent_print") {
            std::vector<Value> args;
            for (const auto& a : call->arguments) {
                args.push_back(evaluate(a.get()));
            }
            return StandardLibrary::silentPrint(args);
        }

        if (name == "push" && call->arguments.size() >= 2) {
            Value target = evaluate(call->arguments[0].get());
            Value val = evaluate(call->arguments[1].get());
            if (target.isList()) {
                if (!target.listVal) target.listVal = std::make_shared<std::vector<Value>>();
                target.listVal->push_back(val);
            }
            return val;
        }

        if (name == "pop" && !call->arguments.empty()) {
            Value target = evaluate(call->arguments[0].get());
            if (target.isList() && target.listVal && !target.listVal->empty()) {
                Value popped = target.listVal->back();
                target.listVal->pop_back();
                return popped;
            }
            return Value();
        }

        if (name == "str" && !call->arguments.empty()) {
            return Value(evaluate(call->arguments[0].get()).toString());
        }

        if (name == "int" && !call->arguments.empty()) {
            return StandardLibrary::toInt(evaluate(call->arguments[0].get()));
        }

        if (name == "float" && !call->arguments.empty()) {
            return StandardLibrary::toFloat(evaluate(call->arguments[0].get()));
        }

        if ((name == "lower" || name == "to_lower" || name == "lowercase" || name == "kecil" || name == "str.lower") && !call->arguments.empty()) {
            return StandardLibrary::toLower(evaluate(call->arguments[0].get()).toString());
        }

        if ((name == "upper" || name == "to_upper" || name == "uppercase" || name == "kapital" || name == "str.upper") && !call->arguments.empty()) {
            return StandardLibrary::toUpper(evaluate(call->arguments[0].get()).toString());
        }

        if (name == "incase_sensitive" || name == "incaseSensitive" || name == "incase_sensitif" ||
            name == "incaseSensitif" || name == "incasesensitive" || name == "incasesensitif" ||
            name == "incase" || name == "icase" || name == "iequals" || name == "iequal" || name == "str.incase_sensitive") {
            if (call->arguments.size() == 1) {
                return StandardLibrary::toLower(evaluate(call->arguments[0].get()).toString());
            } else if (call->arguments.size() >= 2) {
                std::string a = evaluate(call->arguments[0].get()).toString();
                std::string b = evaluate(call->arguments[1].get()).toString();
                return StandardLibrary::incaseSensitive(a, b);
            }
            return Value(false);
        }

        if (name == "case_sensitive" || name == "caseSensitive" || name == "case_sensitif" ||
            name == "caseSensitif" || name == "casesensitive" || name == "casesensitif" ||
            name == "case" || name == "equals" || name == "equal" || name == "str.case_sensitive") {
            if (call->arguments.size() == 1) {
                return evaluate(call->arguments[0].get());
            } else if (call->arguments.size() >= 2) {
                std::string a = evaluate(call->arguments[0].get()).toString();
                std::string b = evaluate(call->arguments[1].get()).toString();
                return StandardLibrary::caseSensitive(a, b);
            }
            return Value(false);
        }

        if (name == "input" || name == "io.input" || name == "io.ask" || name == "ask") {
            if (!isLibraryLoaded("io")) {
                throw std::runtime_error("Library 'io' is not loaded. Please use 'use io' or 'import io' first.");
            }
            std::string prompt = "";
            if (!call->arguments.empty()) {
                prompt = evaluate(call->arguments[0].get()).toString();
            }
            return StandardLibrary::input(prompt);
        }

        if (name == "print" || name == "io.print") {
            std::vector<Value> args;
            for (const auto& a : call->arguments) args.push_back(evaluate(a.get()));
            StandardLibrary::print(args);
            return Value();
        }

        if (name == "read" || name == "io.read") {
            if (!isLibraryLoaded("io")) {
                throw std::runtime_error("Library 'io' is not loaded. Please use 'use io' or 'import io' first.");
            }
            if (!call->arguments.empty()) {
                return StandardLibrary::readFile(evaluate(call->arguments[0].get()).toString());
            }
            return Value();
        }

        if (name == "write" || name == "io.write") {
            if (!isLibraryLoaded("io")) {
                throw std::runtime_error("Library 'io' is not loaded. Please use 'use io' or 'import io' first.");
            }
            if (call->arguments.size() >= 2) {
                return StandardLibrary::writeFile(evaluate(call->arguments[0].get()).toString(), evaluate(call->arguments[1].get()).toString());
            }
            return Value();
        }

        if (name == "append" || name == "io.append") {
            if (!isLibraryLoaded("io")) {
                throw std::runtime_error("Library 'io' is not loaded. Please use 'use io' or 'import io' first.");
            }
            if (call->arguments.size() >= 2) {
                return StandardLibrary::appendFile(evaluate(call->arguments[0].get()).toString(), evaluate(call->arguments[1].get()).toString());
            }
            return Value();
        }

        if (name == "write_lines" || name == "io.write_lines") {
            if (!isLibraryLoaded("io")) {
                throw std::runtime_error("Library 'io' is not loaded. Please use 'use io' or 'import io' first.");
            }
            if (call->arguments.size() >= 2) {
                return StandardLibrary::writeLines(evaluate(call->arguments[0].get()).toString(), evaluate(call->arguments[1].get()));
            }
            return Value();
        }

        if (name == "open" || name == "io.open") {
            if (!isLibraryLoaded("io")) {
                throw std::runtime_error("Library 'io' is not loaded. Please use 'use io' or 'import io' first.");
            }
            std::string mode = "w";
            if (call->arguments.size() >= 2) {
                mode = evaluate(call->arguments[1].get()).toString();
            }
            if (!call->arguments.empty()) {
                return StandardLibrary::openFile(evaluate(call->arguments[0].get()).toString(), mode);
            }
            return Value();
        }

        if (name == "math.sqrt") {
            if (!isLibraryLoaded("math")) {
                throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' or 'import math' first.");
            }
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathSqrt(val);
        }

        if (name == "math.abs") {
            if (!isLibraryLoaded("math")) {
                throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' or 'import math' first.");
            }
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathAbs(val);
        }

        if (name == "math.pow") {
            if (!isLibraryLoaded("math")) {
                throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' or 'import math' first.");
            }
            double base = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            double exp = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).asFloat() : 0.0;
            return StandardLibrary::mathPow(base, exp);
        }

        if (name == "math.floor") {
            if (!isLibraryLoaded("math")) {
                throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' or 'import math' first.");
            }
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathFloor(val);
        }

        if (name == "math.ceil") {
            if (!isLibraryLoaded("math")) {
                throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' or 'import math' first.");
            }
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathCeil(val);
        }

        if (name == "math.round") {
            if (!isLibraryLoaded("math")) {
                throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' or 'import math' first.");
            }
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathRound(val);
        }

        if (name == "math.min") {
            if (!isLibraryLoaded("math")) {
                throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' or 'import math' first.");
            }
            double a = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            double b = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).asFloat() : 0.0;
            return StandardLibrary::mathMin(a, b);
        }

        if (name == "math.max") {
            if (!isLibraryLoaded("math")) {
                throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' or 'import math' first.");
            }
            double a = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            double b = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).asFloat() : 0.0;
            return StandardLibrary::mathMax(a, b);
        }

        if (name == "math.random" || name == "random") {
            if (!isLibraryLoaded("math")) {
                throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' or 'import math' first.");
            }
            if (call->arguments.empty()) {
                return StandardLibrary::mathRandom();
            } else if (call->arguments.size() == 1) {
                return StandardLibrary::mathRandom(evaluate(call->arguments[0].get()).asFloat());
            } else {
                double a = evaluate(call->arguments[0].get()).asFloat();
                double b = evaluate(call->arguments[1].get()).asFloat();
                return StandardLibrary::mathRandom(a, b);
            }
        }

        if (name == "math.random_seed" || name == "math.randomSeed" || name == "math.seed" || name == "random_seed" || name == "seed") {
            if (!isLibraryLoaded("math")) {
                throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' or 'import math' first.");
            }
            if (call->arguments.empty()) {
                return StandardLibrary::mathRandomSeed();
            } else {
                return StandardLibrary::mathRandomSeed(evaluate(call->arguments[0].get()).asInt());
            }
        }

        if (name == "math.sin") {
            if (!isLibraryLoaded("math")) {
                throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' or 'import math' first.");
            }
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathSin(val);
        }

        if (name == "math.cos") {
            if (!isLibraryLoaded("math")) {
                throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' or 'import math' first.");
            }
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathCos(val);
        }

        if (name == "math.tan") {
            if (!isLibraryLoaded("math")) {
                throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' or 'import math' first.");
            }
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathTan(val);
        }

        if (name == "time.sleep") {
            if (!isLibraryLoaded("time")) {
                throw std::runtime_error("Library 'time' is not loaded. Please use 'use time' or 'import time' first.");
            }
            int64_t ms = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asInt() : 0;
            return StandardLibrary::timeSleep(ms);
        }

        if (name == "math.asin") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathAsin(val);
        }
        if (name == "math.acos") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathAcos(val);
        }
        if (name == "math.atan") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathAtan(val);
        }
        if (name == "math.atan2") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double y = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            double x = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).asFloat() : 0.0;
            return StandardLibrary::mathAtan2(y, x);
        }
        if (name == "math.sinh") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathSinh(val);
        }
        if (name == "math.cosh") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathCosh(val);
        }
        if (name == "math.tanh") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathTanh(val);
        }
        if (name == "math.exp") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathExp(val);
        }
        if (name == "math.log" || name == "math.ln") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathLog(val);
        }
        if (name == "math.log10") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathLog10(val);
        }
        if (name == "math.log2") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathLog2(val);
        }
        if (name == "math.degrees") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathDegrees(val);
        }
        if (name == "math.radians") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathRadians(val);
        }
        if (name == "math.hypot") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double x = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            double y = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).asFloat() : 0.0;
            return StandardLibrary::mathHypot(x, y);
        }
        if (name == "math.trunc") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double val = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::mathTrunc(val);
        }
        if (name == "math.clamp") {
            if (!isLibraryLoaded("math")) throw std::runtime_error("Library 'math' is not loaded. Please use 'use math' first.");
            double v = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            double minV = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).asFloat() : 0.0;
            double maxV = call->arguments.size() >= 3 ? evaluate(call->arguments[2].get()).asFloat() : 0.0;
            return StandardLibrary::mathClamp(v, minV, maxV);
        }

        if (name == "time.sleep_s") {
            if (!isLibraryLoaded("time")) throw std::runtime_error("Library 'time' is not loaded. Please use 'use time' first.");
            double s = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asFloat() : 0.0;
            return StandardLibrary::timeSleepS(s);
        }

        if (name == "time.now") {
            return StandardLibrary::timeNow();
        }
        if (name == "time.time") {
            return StandardLibrary::timeTime();
        }
        if (name == "time.time_ns") {
            return StandardLibrary::timeTimeNs();
        }
        if (name == "time.format") {
            std::string fmt = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::timeFormat(fmt);
        }

        if (name == "str.split") {
            std::string s = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).toString() : "";
            std::string delim = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).toString() : " ";
            return StandardLibrary::strSplit(s, delim);
        }
        if (name == "str.join") {
            std::string delim = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).toString() : "";
            Value listV = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()) : Value::makeList();
            return StandardLibrary::strJoin(delim, listV);
        }
        if (name == "str.replace") {
            std::string s = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).toString() : "";
            std::string oldS = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).toString() : "";
            std::string newS = call->arguments.size() >= 3 ? evaluate(call->arguments[2].get()).toString() : "";
            return StandardLibrary::strReplace(s, oldS, newS);
        }
        if (name == "str.trim") {
            std::string s = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::strTrim(s);
        }
        if (name == "str.startswith") {
            std::string s = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).toString() : "";
            std::string prefix = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).toString() : "";
            return StandardLibrary::strStartsWith(s, prefix);
        }
        if (name == "str.endswith") {
            std::string s = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).toString() : "";
            std::string suffix = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).toString() : "";
            return StandardLibrary::strEndsWith(s, suffix);
        }
        if (name == "str.contains") {
            std::string s = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).toString() : "";
            std::string sub = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).toString() : "";
            return StandardLibrary::strContains(s, sub);
        }
        if (name == "str.find") {
            std::string s = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).toString() : "";
            std::string sub = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).toString() : "";
            return StandardLibrary::strFind(s, sub);
        }

        if (name == "os.getcwd") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            return StandardLibrary::osGetcwd();
        }
        if (name == "os.chdir") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osChdir(p);
        }
        if (name == "os.listdir") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : ".";
            return StandardLibrary::osListdir(p);
        }
        if (name == "os.mkdir") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osMkdir(p);
        }
        if (name == "os.makedirs") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osMakedirs(p);
        }
        if (name == "os.remove" || name == "os.unlink") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osRemove(p);
        }
        if (name == "os.rmdir") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osRmdir(p);
        }
        if (name == "os.removedirs" || name == "os.rmtree") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osRemovedirs(p);
        }
        if (name == "os.rename") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            std::string src = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).toString() : "";
            std::string dst = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).toString() : "";
            return StandardLibrary::osRename(src, dst);
        }
        if (name == "os.system") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            std::string cmd = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osSystem(cmd);
        }
        if (name == "os.popen") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            std::string cmd = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osPopen(cmd);
        }
        if (name == "os.getenv") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            std::string k = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).toString() : "";
            std::string defV = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).toString() : "";
            return StandardLibrary::osGetenv(k, defV);
        }
        if (name == "os.setenv" || name == "os.putenv") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            std::string k = call->arguments.size() >= 1 ? evaluate(call->arguments[0].get()).toString() : "";
            std::string v = call->arguments.size() >= 2 ? evaluate(call->arguments[1].get()).toString() : "";
            return StandardLibrary::osSetenv(k, v);
        }
        if (name == "os.unsetenv") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            std::string k = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osUnsetenv(k);
        }
        if (name == "os.getpid") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            return StandardLibrary::osGetpid();
        }
        if (name == "os.cpu_count") {
            if (!isLibraryLoaded("os")) throw std::runtime_error("Library 'os' is not loaded. Please use 'use os' first.");
            return StandardLibrary::osCpuCount();
        }
        if (name == "os.exit") {
            int64_t codeVal = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asInt() : 0;
            return StandardLibrary::osExit(codeVal);
        }
        if (name == "os.path.exists" || name == "path.exists" || name == "os.exists") {
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osPathExists(p);
        }
        if (name == "os.path.isfile" || name == "path.isfile" || name == "os.isfile") {
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osPathIsFile(p);
        }
        if (name == "os.path.isdir" || name == "path.isdir" || name == "os.isdir") {
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osPathIsDir(p);
        }
        if (name == "os.path.getsize" || name == "path.getsize" || name == "os.getsize") {
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osPathGetSize(p);
        }
        if (name == "os.path.join" || name == "path.join") {
            std::vector<Value> jargs;
            for (const auto& a : call->arguments) jargs.push_back(evaluate(a.get()));
            return StandardLibrary::osPathJoin(jargs);
        }
        if (name == "os.path.basename" || name == "path.basename") {
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osPathBasename(p);
        }
        if (name == "os.path.dirname" || name == "path.dirname") {
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osPathDirname(p);
        }
        if (name == "os.path.abspath" || name == "path.abspath") {
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osPathAbspath(p);
        }
        if (name == "os.path.splitext" || name == "path.splitext") {
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osPathSplitext(p);
        }
        if (name == "os.path.isabs" || name == "path.isabs") {
            std::string p = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::osPathIsabs(p);
        }

        if (name == "sys.platform") {
            if (!isLibraryLoaded("sys")) throw std::runtime_error("Library 'sys' is not loaded. Please use 'use sys' first.");
            return StandardLibrary::sysPlatform();
        }
        if (name == "sys.version") {
            if (!isLibraryLoaded("sys")) throw std::runtime_error("Library 'sys' is not loaded. Please use 'use sys' first.");
            return StandardLibrary::sysVersion();
        }
        if (name == "sys.argv") {
            if (!isLibraryLoaded("sys")) throw std::runtime_error("Library 'sys' is not loaded. Please use 'use sys' first.");
            return StandardLibrary::sysArgv();
        }
        if (name == "sys.exit") {
            int64_t codeVal = !call->arguments.empty() ? evaluate(call->arguments[0].get()).asInt() : 0;
            return StandardLibrary::sysExit(codeVal);
        }
        if (name == "sys.getpid") {
            if (!isLibraryLoaded("sys")) throw std::runtime_error("Library 'sys' is not loaded. Please use 'use sys' first.");
            return StandardLibrary::osGetpid();
        }
        if (name == "sys.stdout_write") {
            std::string s = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::sysStdoutWrite(s);
        }
        if (name == "sys.stderr_write") {
            std::string s = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::sysStderrWrite(s);
        }

        if (name == "json.dumps" || name == "json.stringify") {
            if (!isLibraryLoaded("json")) throw std::runtime_error("Library 'json' is not loaded. Please use 'use json' first.");
            Value v = !call->arguments.empty() ? evaluate(call->arguments[0].get()) : Value();
            return StandardLibrary::jsonDumps(v);
        }
        if (name == "json.loads" || name == "json.parse") {
            if (!isLibraryLoaded("json")) throw std::runtime_error("Library 'json' is not loaded. Please use 'use json' first.");
            std::string s = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
            return StandardLibrary::jsonLoads(s);
        }

        if (name == "get" || name == "net.get" || name == "http.get") {
            if (call->arguments.size() == 1) {
                return StandardLibrary::httpGet(evaluate(call->arguments[0].get()).toString());
            } else if (call->arguments.size() >= 2) {
                Value tgt = evaluate(call->arguments[0].get());
                Value prop = evaluate(call->arguments[1].get());
                return tgt.getProperty(prop.toString());
            }
            return Value();
        }

        if ((name == "send" || name == "net.send" || name == "http.send") && call->arguments.size() >= 2) {
            return StandardLibrary::httpSend(evaluate(call->arguments[0].get()).toString(), evaluate(call->arguments[1].get()).toString());
        }

        if ((name == "app" || name == "gui.app") && !call->arguments.empty()) {
            StandardLibrary::setAppTitle(evaluate(call->arguments[0].get()).toString());
            return Value();
        }

        if ((name == "window" || name == "gui.window") && call->arguments.size() >= 2) {
            StandardLibrary::setWindowSize(static_cast<int>(evaluate(call->arguments[0].get()).asInt()), static_cast<int>(evaluate(call->arguments[1].get()).asInt()));
            return Value();
        }

        if (name == "run" || name == "gui.run") {
            int t = call->arguments.empty() ? -1 : static_cast<int>(evaluate(call->arguments[0].get()).asInt());
            return StandardLibrary::runApp(t);
        }

        FunctionDef fnDef;
        std::string targetName = name;
        Value varVal;
        if (currentEnv_->get(name, varVal) && varVal.isFunction()) {
            targetName = varVal.strVal;
        }
        if (currentEnv_->getFunction(targetName, fnDef)) {
            auto callEnv = std::make_shared<Environment>(fnDef.closure);
            for (size_t i = 0; i < fnDef.params.size() && i < call->arguments.size(); ++i) {
                callEnv->define(fnDef.params[i], evaluate(call->arguments[i].get()));
            }
            try {
                return executeBlock(fnDef.body.get(), callEnv);
            } catch (const ReturnException& ret) {
                return ret.value;
            }
        }

        size_t dotPos = name.find('.');
        if (dotPos != std::string::npos) {
            std::string varName = name.substr(0, dotPos);
            std::string method = name.substr(dotPos + 1);
            Value tgt;
            if (currentEnv_->get(varName, tgt) && tgt.isObject() && tgt.objVal) {
                auto hIt = tgt.objVal->find("__handle");
                if (hIt != tgt.objVal->end()) {
                    int64_t handleId = hIt->second.asInt();
                    if (method == "write") {
                        std::string text = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
                        return StandardLibrary::fileWrite(handleId, text);
                    } else if (method == "writeline" || method == "write_line") {
                        std::string text = !call->arguments.empty() ? evaluate(call->arguments[0].get()).toString() : "";
                        return StandardLibrary::fileWriteLine(handleId, text);
                    } else if (method == "flush") {
                        return StandardLibrary::fileFlush(handleId);
                    } else if (method == "close") {
                        return StandardLibrary::fileClose(handleId);
                    }
                }
            }
        }

        throw std::runtime_error("Undefined function '" + name + "' at line " + std::to_string(call->line));
    }

    if (auto* readExpr = dynamic_cast<ReadExpr*>(expr)) {
        if (!isLibraryLoaded("io")) {
            throw std::runtime_error("Library 'io' is not loaded. Please use 'use io' or 'import io' first.");
        }
        Value p = evaluate(readExpr->path.get());
        return StandardLibrary::readFile(p.toString());
    }

    if (auto* getExpr = dynamic_cast<GetExpr*>(expr)) {
        Value tgt = evaluate(getExpr->target.get());
        if (getExpr->property) {
            Value prop = evaluate(getExpr->property.get());
            if (tgt.isObject()) {
                return tgt.getProperty(prop.toString());
            }
            return tgt.getIndex(prop);
        }
        return StandardLibrary::httpGet(tgt.toString());
    }

    if (auto* sendExpr = dynamic_cast<SendExpr*>(expr)) {
        Value tgt = evaluate(sendExpr->target.get());
        Value d = evaluate(sendExpr->data.get());
        return StandardLibrary::httpSend(tgt.toString(), d.toString());
    }

    if (dynamic_cast<NewExpr*>(expr)) {
        return Value::makeObject();
    }

    if (auto* ifExpr = dynamic_cast<IfExpr*>(expr)) {
        Value cond = evaluate(ifExpr->condition.get());
        if (cond.isTruthy()) {
            return executeBlock(ifExpr->thenBranch.get(), currentEnv_);
        } else if (ifExpr->elseBranch) {
            return executeBlock(ifExpr->elseBranch.get(), currentEnv_);
        }
        return Value();
    }

    if (auto* loopExpr = dynamic_cast<LoopExpr*>(expr)) {
        Value cntVal = evaluate(loopExpr->count.get());
        int64_t n = cntVal.asInt();
        std::string accum = "";
        for (int64_t i = 0; i < n; ++i) {
            if (loopExpr->body) {
                for (const auto& s : loopExpr->body->statements) {
                    if (auto* p = dynamic_cast<PrintStmt*>(s.get())) {
                        for (const auto& a : p->arguments) {
                            accum += evaluate(a.get()).toString();
                        }
                    } else if (auto* es = dynamic_cast<ExprStmt*>(s.get())) {
                        accum += evaluate(es->expression.get()).toString();
                    } else {
                        execute(s.get());
                    }
                }
            }
        }
        return Value(accum);
    }

    return Value();
}
