#include "Diagnostic.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

std::string Diagnostic::currentFilename_ = "script.fsn";
std::vector<std::string> Diagnostic::sourceLines_;

void Diagnostic::setSource(const std::string& filename, const std::string& source) {
    currentFilename_ = filename.empty() ? "script.fsn" : filename;
    sourceLines_.clear();

    std::string cur;
    for (size_t i = 0; i < source.size(); ++i) {
        char c = source[i];
        if (c == '\r') {
            if (i + 1 < source.size() && source[i + 1] == '\n') {
                i++;
            }
            sourceLines_.push_back(cur);
            cur.clear();
        } else if (c == '\n') {
            sourceLines_.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty() || sourceLines_.empty()) {
        sourceLines_.push_back(cur);
    }
}

const std::string& Diagnostic::getFilename() {
    return currentFilename_;
}

std::string Diagnostic::getLine(int line) {
    if (line >= 1 && line <= static_cast<int>(sourceLines_.size())) {
        return sourceLines_[line - 1];
    }
    return "";
}

int Diagnostic::getLineCount() {
    return static_cast<int>(sourceLines_.size());
}

bool Diagnostic::supportsColor() {
    const char* noColor = std::getenv("NO_COLOR");
    if (noColor && *noColor) return false;

#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE || hOut == NULL) return false;
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) return false;
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    return true;
#else
    return ISATTY(FILENO(stderr));
#endif
}

int Diagnostic::levenshteinDistance(const std::string& s1, const std::string& s2) {
    size_t len1 = s1.size(), len2 = s2.size();
    if (len1 == 0) return static_cast<int>(len2);
    if (len2 == 0) return static_cast<int>(len1);

    std::string a = s1, b = s2;
    for (char& c : a) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (char& c : b) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::vector<std::vector<int>> d(len1 + 1, std::vector<int>(len2 + 1, 0));
    for (size_t i = 0; i <= len1; ++i) d[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= len2; ++j) d[0][j] = static_cast<int>(j);

    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            d[i][j] = std::min({
                d[i - 1][j] + 1,
                d[i][j - 1] + 1,
                d[i - 1][j - 1] + cost
            });
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) {
                d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + cost);
            }
        }
    }
    return d[len1][len2];
}

std::string Diagnostic::suggestSimilar(const std::string& word, const std::vector<std::string>& candidates, int maxDist) {
    if (word.empty() || candidates.empty()) return "";

    std::string lowerWord = word;
    for (char& c : lowerWord) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (lowerWord == "elsif") {
        return "elseif' atau 'elif";
    }

    int bestDist = 999;
    std::string bestCandidate = "";

    if (word.size() <= 1) return "";

    int threshold = maxDist;
    if (word.size() <= 3) threshold = 1;
    else if (word.size() >= 6) threshold = std::max(threshold, 3);

    for (const auto& cand : candidates) {
        if (cand == word) continue;
        std::string lowerCand = cand;
        for (char& c : lowerCand) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lowerCand == lowerWord) return cand;

        int dist = levenshteinDistance(word, cand);
        if (dist <= threshold && dist < bestDist) {
            bestDist = dist;
            bestCandidate = cand;
        }
    }
    return bestCandidate;
}

std::string Diagnostic::format(
    const std::string& errorType,
    const std::string& message,
    int line,
    int column,
    int length,
    const std::string& recommendation,
    const std::string& hint) {

    bool color = supportsColor();
    std::string cReset = color ? "\033[0m" : "";
    std::string cRedBold = color ? "\033[1;31m" : "";
    std::string cCyan = color ? "\033[36m" : "";
    std::string cCyanBold = color ? "\033[1;36m" : "";
    std::string cYellowBold = color ? "\033[1;33m" : "";
    std::string cGray = color ? "\033[90m" : "";
    std::string cWhiteBold = color ? "\033[1;37m" : "";

    std::ostringstream ss;
    int safeLine = std::max(1, line);
    int safeCol = std::max(1, column);

    // Header: File "...", line X, col Y
    ss << cCyan << "File \"" << cCyanBold << currentFilename_ << cCyan << "\", line "
       << safeLine << ", col " << safeCol << cReset << "\n";

    // Source code snippet
    std::string lineStr = getLine(safeLine);
    if (!lineStr.empty() || safeLine <= getLineCount()) {
        std::string lineNumStr = std::to_string(safeLine);
        int gutterWidth = std::max(4, static_cast<int>(lineNumStr.size()) + 1);
        std::string gutterPad(gutterWidth - lineNumStr.size(), ' ');

        ss << cGray << gutterPad << lineNumStr << " | " << cReset << lineStr << "\n";

        // Caret pointer
        int visualCol = 0;
        for (int i = 0; i < safeCol - 1 && i < static_cast<int>(lineStr.size()); ++i) {
            if (lineStr[i] == '\t') visualCol += 4;
            else visualCol += 1;
        }

        std::string emptyGutter(gutterWidth, ' ');
        ss << cGray << emptyGutter << " | " << cReset << std::string(visualCol, ' ');

        int caretLen = std::max(1, length);
        if (visualCol + caretLen > static_cast<int>(lineStr.size()) + 1) {
            caretLen = std::max(1, static_cast<int>(lineStr.size()) + 1 - visualCol);
        }
        ss << cRedBold << std::string(caretLen, '^') << cReset << "\n";
    }

    // Error Class & Message
    ss << cRedBold << errorType << cReset << ": " << cWhiteBold << message << cReset << "\n";

    static const char* tipIcon =
#ifdef _WIN32
        "[*]";
#else
        ([]() {
            const char* lang = std::getenv("LANG");
            const char* lcAll = std::getenv("LC_ALL");
            bool isUtf8 = (lang && (std::string(lang).find("UTF-8") != std::string::npos || std::string(lang).find("utf8") != std::string::npos)) ||
                          (lcAll && (std::string(lcAll).find("UTF-8") != std::string::npos || std::string(lcAll).find("utf8") != std::string::npos));
            return isUtf8 ? "💡" : "[*]";
        })();
#endif

    // Recommendation (Did you mean...?)
    if (!recommendation.empty()) {
        ss << "  " << cYellowBold << tipIcon << " Rekomendasi:" << cReset << " " << recommendation << "\n";
    }

    // Hint / Actionable Solution
    if (!hint.empty()) {
        ss << "  " << cCyanBold << tipIcon << " Solusi:" << cReset << " " << hint << "\n";
    }

    return ss.str();
}

const std::vector<std::string>& Diagnostic::getKeywords() {
    static const std::vector<std::string> kws = {
        "while", "loop", "for", "in", "repeat", "until", "var", "let", "if", "elseif", "elif", "else", "print", "silent_print",
        "fn", "def", "func", "function", "return", "break", "continue",
        "use", "import", "include", "require", "new", "get", "set", "read", "write",
        "send", "app", "window", "run", "end", "then", "do", "and", "or", "not", "true", "false",
        "nil", "null"
    };
    return kws;
}

const std::vector<std::string>& Diagnostic::getStatementKeywords() {
    static const std::vector<std::string> stmtKws = {
        "while", "loop", "for", "repeat", "var", "let", "if", "elseif", "elif", "else", "print", "silent_print",
        "fn", "def", "func", "function", "return", "break", "continue",
        "use", "import", "read", "write", "app", "window", "run", "set"
    };
    return stmtKws;
}

const std::vector<std::string>& Diagnostic::getBuiltinFunctions() {
    static const std::vector<std::string> builtins = {
        "input", "ask", "len", "push", "pop", "str", "int", "float",
        "lower", "upper", "case_sensitive", "incase_sensitive"
    };
    return builtins;
}

const std::vector<std::string>& Diagnostic::getStandardLibraries() {
    static const std::vector<std::string> libs = {
        "io", "math", "time", "net", "http", "gui", "str", "os", "sys", "json", "path"
    };
    return libs;
}

std::vector<std::string> Diagnostic::getModuleMembers(const std::string& moduleName) {
    if (moduleName == "math") {
        return {
            "sqrt", "abs", "pow", "floor", "ceil", "round", "min", "max",
            "random", "random_seed", "seed", "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
            "sinh", "cosh", "tanh", "exp", "log", "log10", "log2", "degrees", "radians",
            "hypot", "trunc", "clamp", "pi", "e", "tau", "inf"
        };
    } else if (moduleName == "io") {
        return { "input", "ask", "read", "write", "append", "write_lines", "open", "print" };
    } else if (moduleName == "time") {
        return { "now", "sleep", "time", "time_ns", "sleep_s", "format" };
    } else if (moduleName == "net" || moduleName == "http") {
        return { "get", "send" };
    } else if (moduleName == "gui") {
        return { "app", "window", "run" };
    } else if (moduleName == "str" || moduleName == "string") {
        return { "lower", "upper", "case_sensitive", "incase_sensitive", "split", "join", "replace", "trim", "startswith", "endswith", "contains", "find" };
    } else if (moduleName == "os") {
        return {
            "name", "sep", "linesep", "devnull", "getcwd", "chdir", "listdir", "mkdir", "makedirs",
            "remove", "unlink", "rmdir", "removedirs", "rmtree", "rename", "system", "popen",
            "getenv", "setenv", "putenv", "unsetenv", "getpid", "cpu_count", "exit", "path",
            "exists", "isfile", "isdir", "getsize"
        };
    } else if (moduleName == "sys") {
        return { "platform", "version", "argv", "exit", "getpid", "stdout_write", "stderr_write" };
    } else if (moduleName == "json") {
        return { "dumps", "loads", "stringify", "parse" };
    } else if (moduleName == "path" || moduleName == "os.path") {
        return { "exists", "isfile", "isdir", "getsize", "join", "basename", "dirname", "abspath", "splitext", "isabs" };
    }
    return {};
}
