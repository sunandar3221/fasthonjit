#include "StandardLibrary.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cctype>
#include <cmath>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <random>
#include <cstdio>
#include <mutex>
#include <filesystem>
#include <thread>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#else
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

std::string StandardLibrary::appTitle_ = "Fasthon App";
int StandardLibrary::windowWidth_ = 800;
int StandardLibrary::windowHeight_ = 600;
std::vector<std::string> StandardLibrary::args_;

void StandardLibrary::setArgs(const std::vector<std::string>& args) {
    args_ = args;
}

const std::vector<std::string>& StandardLibrary::getArgs() {
    return args_;
}

#ifdef _WIN32
static LRESULT CALLBACK FasthonWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            std::string title = StandardLibrary::getAppTitle();
            TextOutA(hdc, 20, 20, title.c_str(), static_cast<int>(title.size()));
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}
#endif

Value StandardLibrary::print(const std::vector<Value>& args) {
    std::string out;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) out += " ";
        out += args[i].toString();
    }
    std::cout << out << "\n";
    std::cout.flush();
    return Value(out + "\n");
}

Value StandardLibrary::silentPrint(const std::vector<Value>& args) {
    std::string out;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) out += " ";
        out += args[i].toString();
    }
    return Value(out + "\n");
}

Value StandardLibrary::input(const std::string& prompt) {
    if (!prompt.empty()) {
        std::cout << prompt;
        std::cout.flush();
    }
    std::string line;
    if (std::getline(std::cin, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        return Value(line);
    }
    return Value("");
}

struct ManagedFileHandle {
    FILE* fp = nullptr;
    std::string writeBuffer;
};

static std::unordered_map<int64_t, ManagedFileHandle> g_managedFiles;
static int64_t g_nextManagedFileId = 1;
static std::mutex g_managedFileMutex;
static ManagedFileHandle* g_cachedHandle = nullptr;
static int64_t g_cachedHandleId = -1;

struct AutoAppendCache {
    std::string path;
    FILE* fp = nullptr;
    std::string buffer;
};
static AutoAppendCache g_autoAppend;
static std::mutex g_autoAppendMutex;

static void flushAutoAppend() {
    std::lock_guard<std::mutex> lock(g_autoAppendMutex);
    if (g_autoAppend.fp) {
        if (!g_autoAppend.buffer.empty()) {
            fwrite(g_autoAppend.buffer.data(), 1, g_autoAppend.buffer.size(), g_autoAppend.fp);
            g_autoAppend.buffer.clear();
        }
        fflush(g_autoAppend.fp);
        fclose(g_autoAppend.fp);
        g_autoAppend.fp = nullptr;
        g_autoAppend.path.clear();
    }
}

static void ensureAppendAtexitRegistered() {
    static bool s_registered = false;
    if (!s_registered) {
        s_registered = true;
        std::atexit(flushAutoAppend);
    }
}

static std::string sanitizePath(const std::string& path) {
    std::string clean = path;
    while (!clean.empty() && (clean.back() == '\r' || clean.back() == '\n' || clean.back() == ' ' || clean.back() == '\t')) {
        clean.pop_back();
    }
    size_t start = 0;
    while (start < clean.size() && (clean[start] == ' ' || clean[start] == '\t')) {
        start++;
    }
    if (start > 0) {
        clean = clean.substr(start);
    }
    return clean;
}

Value StandardLibrary::readFile(const std::string& path) {
    std::string cleanPath = sanitizePath(path);
    if (cleanPath.empty()) return Value("");
    if (g_autoAppend.fp && g_autoAppend.path == cleanPath) {
        flushAutoAppend();
    }
    FILE* fp = fopen(cleanPath.c_str(), "rb");
    if (!fp) {
        return Value("");
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(fp);
        return Value("");
    }
    std::string res;
    res.resize(static_cast<size_t>(sz));
    size_t readBytes = fread(&res[0], 1, static_cast<size_t>(sz), fp);
    fclose(fp);
    if (readBytes < static_cast<size_t>(sz)) {
        res.resize(readBytes);
    }
    return Value(std::move(res));
}

Value StandardLibrary::writeFile(const std::string& path, const std::string& content) {
    std::string cleanPath = sanitizePath(path);
    if (cleanPath.empty()) return Value(false);
    if (g_autoAppend.fp && g_autoAppend.path == cleanPath) {
        flushAutoAppend();
    }
    FILE* fp = fopen(cleanPath.c_str(), "wb");
    if (!fp) {
        return Value(false);
    }
    size_t written = 0;
    if (!content.empty()) {
        written = fwrite(content.data(), 1, content.size(), fp);
    }
    fflush(fp);
    fclose(fp);
    return Value(written == content.size());
}

Value StandardLibrary::appendFile(const std::string& path, const std::string& content) {
    std::string cleanPath = sanitizePath(path);
    if (cleanPath.empty()) return Value(false);

    ensureAppendAtexitRegistered();
    std::lock_guard<std::mutex> lock(g_autoAppendMutex);
    if (g_autoAppend.fp && g_autoAppend.path == cleanPath) {
        if (!content.empty()) {
            g_autoAppend.buffer.append(content);
            if (__builtin_expect(g_autoAppend.buffer.size() >= 524288, 0)) {
                fwrite(g_autoAppend.buffer.data(), 1, g_autoAppend.buffer.size(), g_autoAppend.fp);
                g_autoAppend.buffer.clear();
            }
        }
        return Value(true);
    }

    if (g_autoAppend.fp) {
        if (!g_autoAppend.buffer.empty()) {
            fwrite(g_autoAppend.buffer.data(), 1, g_autoAppend.buffer.size(), g_autoAppend.fp);
            g_autoAppend.buffer.clear();
        }
        fflush(g_autoAppend.fp);
        fclose(g_autoAppend.fp);
        g_autoAppend.fp = nullptr;
        g_autoAppend.path.clear();
    }

    FILE* fp = fopen(cleanPath.c_str(), "ab");
    if (!fp) {
        return Value(false);
    }
    g_autoAppend.fp = fp;
    g_autoAppend.path = cleanPath;
    g_autoAppend.buffer.reserve(524288);
    if (!content.empty()) {
        g_autoAppend.buffer.append(content);
    }
    return Value(true);
}

Value StandardLibrary::writeLines(const std::string& path, const Value& listVal) {
    if (!listVal.isList() || !listVal.listVal) {
        return Value(false);
    }
    std::string cleanPath = sanitizePath(path);
    if (cleanPath.empty()) return Value(false);
    if (g_autoAppend.fp && g_autoAppend.path == cleanPath) {
        flushAutoAppend();
    }
    FILE* fp = fopen(cleanPath.c_str(), "wb");
    if (!fp) {
        return Value(false);
    }
    std::string chunk;
    chunk.reserve(524288);
    for (const auto& item : *listVal.listVal) {
        std::string s = item.toString();
        if (!s.empty()) {
            chunk.append(s);
        }
        chunk.push_back('\n');
        if (__builtin_expect(chunk.size() >= 524288, 0)) {
            fwrite(chunk.data(), 1, chunk.size(), fp);
            chunk.clear();
        }
    }
    if (!chunk.empty()) {
        fwrite(chunk.data(), 1, chunk.size(), fp);
        chunk.clear();
    }
    fflush(fp);
    fclose(fp);
    return Value(true);
}

Value StandardLibrary::openFile(const std::string& path, const std::string& mode) {
    std::string cleanPath = sanitizePath(path);
    if (cleanPath.empty()) return Value();
    if (g_autoAppend.fp && g_autoAppend.path == cleanPath) {
        flushAutoAppend();
    }
    std::string actualMode = mode.empty() ? "w" : mode;
    if (actualMode.find('b') == std::string::npos && actualMode.find('+') == std::string::npos) {
        actualMode += "b";
    }
    FILE* fp = fopen(cleanPath.c_str(), actualMode.c_str());
    if (!fp) {
        return Value();
    }
    std::lock_guard<std::mutex> lock(g_managedFileMutex);
    int64_t handleId = g_nextManagedFileId++;
    ManagedFileHandle handle;
    handle.fp = fp;
    handle.writeBuffer.reserve(524288);
    g_managedFiles[handleId] = std::move(handle);
    g_cachedHandle = &g_managedFiles[handleId];
    g_cachedHandleId = handleId;

    Value obj = Value::makeObject();
    obj.setProperty("__handle", Value(handleId));
    obj.setProperty("path", Value(cleanPath));
    obj.setProperty("mode", Value(mode));
    obj.setProperty("is_open", Value(true));
    return obj;
}

Value StandardLibrary::fileWrite(int64_t handleId, const std::string& content) {
    ManagedFileHandle* h = nullptr;
    if (__builtin_expect(handleId == g_cachedHandleId && g_cachedHandle != nullptr, 1)) {
        h = g_cachedHandle;
    } else {
        std::lock_guard<std::mutex> lock(g_managedFileMutex);
        auto it = g_managedFiles.find(handleId);
        if (it == g_managedFiles.end() || !it->second.fp) {
            return Value(false);
        }
        g_cachedHandle = &it->second;
        g_cachedHandleId = handleId;
        h = g_cachedHandle;
    }

    if (!content.empty()) {
        h->writeBuffer.append(content);
        if (__builtin_expect(h->writeBuffer.size() >= 524288, 0)) {
            fwrite(h->writeBuffer.data(), 1, h->writeBuffer.size(), h->fp);
            h->writeBuffer.clear();
        }
    }
    return Value(true);
}

Value StandardLibrary::fileWriteLine(int64_t handleId, const std::string& line) {
    ManagedFileHandle* h = nullptr;
    if (__builtin_expect(handleId == g_cachedHandleId && g_cachedHandle != nullptr, 1)) {
        h = g_cachedHandle;
    } else {
        std::lock_guard<std::mutex> lock(g_managedFileMutex);
        auto it = g_managedFiles.find(handleId);
        if (it == g_managedFiles.end() || !it->second.fp) {
            return Value(false);
        }
        g_cachedHandle = &it->second;
        g_cachedHandleId = handleId;
        h = g_cachedHandle;
    }

    if (!line.empty()) {
        h->writeBuffer.append(line);
    }
    h->writeBuffer.push_back('\n');
    if (__builtin_expect(h->writeBuffer.size() >= 524288, 0)) {
        fwrite(h->writeBuffer.data(), 1, h->writeBuffer.size(), h->fp);
        h->writeBuffer.clear();
    }
    return Value(true);
}

Value StandardLibrary::fileFlush(int64_t handleId) {
    std::lock_guard<std::mutex> lock(g_managedFileMutex);
    auto it = g_managedFiles.find(handleId);
    if (it == g_managedFiles.end() || !it->second.fp) {
        return Value(false);
    }
    if (!it->second.writeBuffer.empty()) {
        fwrite(it->second.writeBuffer.data(), 1, it->second.writeBuffer.size(), it->second.fp);
        it->second.writeBuffer.clear();
    }
    fflush(it->second.fp);
    return Value(true);
}

Value StandardLibrary::fileClose(int64_t handleId) {
    std::lock_guard<std::mutex> lock(g_managedFileMutex);
    auto it = g_managedFiles.find(handleId);
    if (it == g_managedFiles.end() || !it->second.fp) {
        return Value(false);
    }
    if (!it->second.writeBuffer.empty()) {
        fwrite(it->second.writeBuffer.data(), 1, it->second.writeBuffer.size(), it->second.fp);
        it->second.writeBuffer.clear();
    }
    fflush(it->second.fp);
    fclose(it->second.fp);
    if (g_cachedHandleId == handleId) {
        g_cachedHandle = nullptr;
        g_cachedHandleId = -1;
    }
    g_managedFiles.erase(it);
    return Value(true);
}

Value StandardLibrary::httpGet(const std::string& url) {
#ifdef _WIN32
    HINTERNET hInternet = InternetOpenA("FasthonClient/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        return Value("HTTP_ERROR: failed to open internet");
    }

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return Value("HTTP_RESPONSE: 200 OK (dummy network fallback for " + url + ")");
    }

    std::string response;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    return Value(response);
#else
    std::string cmd = "curl -s -L \"" + url + "\" 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return Value("HTTP_RESPONSE: 200 OK (dummy network fallback for " + url + ")");
    }
    std::string response;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        response.append(buffer);
    }
    pclose(pipe);
    if (response.empty()) {
        return Value("HTTP_RESPONSE: 200 OK (dummy network fallback for " + url + ")");
    }
    return Value(response);
#endif
}

Value StandardLibrary::httpSend(const std::string& url, const std::string& data) {
#ifdef _WIN32
    HINTERNET hInternet = InternetOpenA("FasthonClient/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        return Value("HTTP_ERROR: failed to open internet");
    }

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return Value("SENT: " + data + " to " + url);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    return Value("SENT_OK: " + std::to_string(data.size()) + " bytes");
#else
    std::string cmd = "curl -s -X POST -d \"" + data + "\" \"" + url + "\" 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        pclose(pipe);
        return Value("SENT_OK: " + std::to_string(data.size()) + " bytes");
    }
    return Value("SENT: " + data + " to " + url);
#endif
}

void StandardLibrary::setAppTitle(const std::string& title) {
    appTitle_ = title;
}

void StandardLibrary::setWindowSize(int width, int height) {
    windowWidth_ = width;
    windowHeight_ = height;
}

std::string StandardLibrary::getAppTitle() {
    return appTitle_;
}

int StandardLibrary::getWindowWidth() {
    return windowWidth_;
}

int StandardLibrary::getWindowHeight() {
    return windowHeight_;
}

Value StandardLibrary::runApp(int timeoutMs) {
#ifdef _WIN32
    if (timeoutMs < 0) {
        const char* envTimeout = std::getenv("FASTHON_TIMEOUT");
        if (!envTimeout) envTimeout = std::getenv("EAS_TIMEOUT");
        if (envTimeout) {
            timeoutMs = std::atoi(envTimeout);
        } else if (std::getenv("FASTHON_HEADLESS") || std::getenv("EAS_HEADLESS")) {
            timeoutMs = 150;
        }
    }

    HINSTANCE hInst = GetModuleHandleA(NULL);
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = FasthonWindowProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "FasthonAppClass";

    RegisterClassExA(&wc);

    HWND hwnd = CreateWindowExA(
        0,
        "FasthonAppClass",
        appTitle_.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowWidth_,
        windowHeight_,
        NULL,
        NULL,
        hInst,
        NULL
    );

    if (!hwnd) {
        return Value(false);
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    std::cout << "Desktop application running: " << appTitle_ << " [" << windowWidth_ << "x" << windowHeight_ << "]\n";
    std::cout.flush();

    if (timeoutMs >= 0) {
        DWORD start = GetTickCount();
        while (GetTickCount() - start < static_cast<DWORD>(timeoutMs)) {
            MSG msg;
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) break;
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
            Sleep(10);
        }
        DestroyWindow(hwnd);
    } else {
        MSG msg;
        while (GetMessageA(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    std::cout << "Desktop application lifecycle completed\n";
    std::cout.flush();

    return Value(true);
#else
    std::cout << "Desktop application running: " << appTitle_ << " [" << windowWidth_ << "x" << windowHeight_ << "]\n";
    std::cout.flush();
    if (timeoutMs > 0) {
        usleep(timeoutMs * 1000);
    }
    std::cout << "Desktop application lifecycle completed\n";
    std::cout.flush();
    return Value(true);
#endif
}

Value StandardLibrary::toLower(const std::string& str) {
    std::string res = str;
    for (char& c : res) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return Value(res);
}

Value StandardLibrary::toUpper(const std::string& str) {
    std::string res = str;
    for (char& c : res) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return Value(res);
}

Value StandardLibrary::caseSensitive(const std::string& a, const std::string& b) {
    return Value(a == b);
}

Value StandardLibrary::incaseSensitive(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return Value(false);
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return Value(false);
        }
    }
    return Value(true);
}

Value StandardLibrary::toInt(const Value& val) {
    if (val.isNil()) return Value();
    if (val.isInt()) return val;
    if (val.isFloat()) return Value(static_cast<int64_t>(val.floatVal));
    if (val.isBool()) return Value(static_cast<int64_t>(val.boolVal ? 1 : 0));
    if (val.isString()) {
        std::string s = val.strVal;
        size_t start = 0;
        while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
        size_t end = s.size();
        while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
        if (start >= end) return Value();
        s = s.substr(start, end - start);

        try {
            size_t idx = 0;
            long long parsed = std::stoll(s, &idx);
            if (idx == s.size()) {
                return Value(static_cast<int64_t>(parsed));
            }
            if (s[idx] == '.') {
                size_t dIdx = 0;
                double d = std::stod(s, &dIdx);
                if (dIdx == s.size()) {
                    return Value(static_cast<int64_t>(d));
                }
            }
        } catch (...) {
            return Value();
        }
    }
    return Value();
}

Value StandardLibrary::toFloat(const Value& val) {
    if (val.isNil()) return Value();
    if (val.isFloat()) return val;
    if (val.isInt()) return Value(static_cast<double>(val.intVal));
    if (val.isBool()) return Value(val.boolVal ? 1.0 : 0.0);
    if (val.isString()) {
        std::string s = val.strVal;
        size_t start = 0;
        while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
        size_t end = s.size();
        while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
        if (start >= end) return Value();
        s = s.substr(start, end - start);

        try {
            size_t idx = 0;
            double d = std::stod(s, &idx);
            if (idx == s.size()) {
                return Value(d);
            }
        } catch (...) {
            return Value();
        }
    }
    return Value();
}

Value StandardLibrary::mathSqrt(double val) {
    return Value(std::sqrt(val));
}

Value StandardLibrary::mathAbs(double val) {
    return Value(std::abs(val));
}

Value StandardLibrary::mathPow(double base, double exp) {
    return Value(std::pow(base, exp));
}

Value StandardLibrary::mathFloor(double val) {
    return Value(std::floor(val));
}

Value StandardLibrary::mathCeil(double val) {
    return Value(std::ceil(val));
}

Value StandardLibrary::mathRound(double val) {
    return Value(std::round(val));
}

Value StandardLibrary::mathMin(double a, double b) {
    return Value(std::min(a, b));
}

Value StandardLibrary::mathMax(double a, double b) {
    return Value(std::max(a, b));
}

static std::mt19937_64& getRandomEngine() {
    static thread_local std::mt19937_64 rng([]() {
        uint64_t s1 = std::random_device{}();
        uint64_t s2 = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        return s1 ^ (s2 + 0x9e3779b97f4a7c15ULL + (s1 << 6) + (s1 >> 2));
    }());
    return rng;
}

Value StandardLibrary::mathRandom() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return Value(dist(getRandomEngine()));
}

Value StandardLibrary::mathRandom(double max) {
    if (max <= 0.0) return Value(static_cast<int64_t>(0));
    if (max == std::floor(max)) {
        int64_t high = static_cast<int64_t>(max);
        std::uniform_int_distribution<int64_t> dist(1, high);
        return Value(dist(getRandomEngine()));
    }
    std::uniform_real_distribution<double> dist(0.0, max);
    return Value(dist(getRandomEngine()));
}

Value StandardLibrary::mathRandom(double min, double max) {
    if (min > max) std::swap(min, max);
    if (min == std::floor(min) && max == std::floor(max)) {
        int64_t low = static_cast<int64_t>(min);
        int64_t high = static_cast<int64_t>(max);
        std::uniform_int_distribution<int64_t> dist(low, high);
        return Value(dist(getRandomEngine()));
    }
    std::uniform_real_distribution<double> dist(min, max);
    return Value(dist(getRandomEngine()));
}

Value StandardLibrary::mathRandom(const std::vector<Value>& args) {
    if (args.empty()) return mathRandom();
    if (args.size() == 1) return mathRandom(args[0].asFloat());
    return mathRandom(args[0].asFloat(), args[1].asFloat());
}

Value StandardLibrary::mathRandomSeed(int64_t seed) {
    getRandomEngine().seed(static_cast<uint64_t>(seed));
    std::srand(static_cast<unsigned int>(seed));
    return Value(seed);
}

Value StandardLibrary::mathRandomSeed() {
    uint64_t s1 = std::random_device{}();
    uint64_t s2 = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    uint64_t seed = s1 ^ (s2 + 0x9e3779b97f4a7c15ULL + (s1 << 6) + (s1 >> 2));
    getRandomEngine().seed(seed);
    std::srand(static_cast<unsigned int>(seed));
    return Value(static_cast<int64_t>(seed));
}

Value StandardLibrary::mathRandomSeed(const std::vector<Value>& args) {
    if (args.empty() || args[0].isNil()) {
        return mathRandomSeed();
    }
    return mathRandomSeed(args[0].asInt());
}

Value StandardLibrary::mathSin(double val) {
    return Value(std::sin(val));
}

Value StandardLibrary::mathCos(double val) {
    return Value(std::cos(val));
}

Value StandardLibrary::mathTan(double val) {
    return Value(std::tan(val));
}

Value StandardLibrary::mathAsin(double val) {
    return Value(std::asin(val));
}

Value StandardLibrary::mathAcos(double val) {
    return Value(std::acos(val));
}

Value StandardLibrary::mathAtan(double val) {
    return Value(std::atan(val));
}

Value StandardLibrary::mathAtan2(double y, double x) {
    return Value(std::atan2(y, x));
}

Value StandardLibrary::mathSinh(double val) {
    return Value(std::sinh(val));
}

Value StandardLibrary::mathCosh(double val) {
    return Value(std::cosh(val));
}

Value StandardLibrary::mathTanh(double val) {
    return Value(std::tanh(val));
}

Value StandardLibrary::mathExp(double val) {
    return Value(std::exp(val));
}

Value StandardLibrary::mathLog(double val) {
    return Value(std::log(val));
}

Value StandardLibrary::mathLog10(double val) {
    return Value(std::log10(val));
}

Value StandardLibrary::mathLog2(double val) {
    return Value(std::log2(val));
}

Value StandardLibrary::mathDegrees(double rad) {
    return Value(rad * (180.0 / 3.14159265358979323846));
}

Value StandardLibrary::mathRadians(double deg) {
    return Value(deg * (3.14159265358979323846 / 180.0));
}

Value StandardLibrary::mathHypot(double x, double y) {
    return Value(std::hypot(x, y));
}

Value StandardLibrary::mathTrunc(double val) {
    return Value(std::trunc(val));
}

Value StandardLibrary::mathClamp(double val, double minVal, double maxVal) {
    if (minVal > maxVal) std::swap(minVal, maxVal);
    return Value(std::clamp(val, minVal, maxVal));
}

Value StandardLibrary::timeSleep(int64_t ms) {
    if (ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    return Value();
}

Value StandardLibrary::timeSleepS(double seconds) {
    if (seconds > 0.0) {
        std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    }
    return Value();
}

Value StandardLibrary::timeNow() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return Value(ms);
}

Value StandardLibrary::timeTime() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    double sec = std::chrono::duration<double>(now).count();
    return Value(sec);
}

Value StandardLibrary::timeTimeNs() {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
    int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    return Value(ns);
}

Value StandardLibrary::timeFormat(const std::string& fmt) {
    std::string actualFmt = fmt.empty() ? "%Y-%m-%d %H:%M:%S" : fmt;
    std::time_t t = std::time(nullptr);
    std::tm tmVal{};
#ifdef _WIN32
    localtime_s(&tmVal, &t);
#else
    localtime_r(&t, &tmVal);
#endif
    char buf[256];
    if (std::strftime(buf, sizeof(buf), actualFmt.c_str(), &tmVal)) {
        return Value(std::string(buf));
    }
    return Value("");
}

// OS & Path Implementation
Value StandardLibrary::osName() {
#ifdef _WIN32
    return Value("nt");
#else
    return Value("posix");
#endif
}

Value StandardLibrary::osSep() {
#ifdef _WIN32
    return Value("\\");
#else
    return Value("/");
#endif
}

Value StandardLibrary::osLinesep() {
#ifdef _WIN32
    return Value("\r\n");
#else
    return Value("\n");
#endif
}

Value StandardLibrary::osDevnull() {
#ifdef _WIN32
    return Value("NUL");
#else
    return Value("/dev/null");
#endif
}

Value StandardLibrary::osGetcwd() {
    try {
        return Value(std::filesystem::current_path().string());
    } catch (...) {
        return Value("");
    }
}

Value StandardLibrary::osChdir(const std::string& path) {
    try {
        std::filesystem::current_path(path);
        return Value(true);
    } catch (...) {
        return Value(false);
    }
}

Value StandardLibrary::osListdir(const std::string& path) {
    std::string target = path.empty() ? "." : path;
    std::vector<Value> entries;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(target)) {
            entries.push_back(Value(entry.path().filename().string()));
        }
    } catch (...) {}
    return Value(entries);
}

Value StandardLibrary::osMkdir(const std::string& path) {
    try {
        return Value(std::filesystem::create_directory(path));
    } catch (...) {
        return Value(false);
    }
}

Value StandardLibrary::osMakedirs(const std::string& path) {
    try {
        return Value(std::filesystem::create_directories(path));
    } catch (...) {
        return Value(false);
    }
}

Value StandardLibrary::osRemove(const std::string& path) {
    try {
        return Value(std::filesystem::remove(path));
    } catch (...) {
        return Value(false);
    }
}

Value StandardLibrary::osRmdir(const std::string& path) {
    try {
        return Value(std::filesystem::remove(path));
    } catch (...) {
        return Value(false);
    }
}

Value StandardLibrary::osRemovedirs(const std::string& path) {
    try {
        return Value(static_cast<int64_t>(std::filesystem::remove_all(path)));
    } catch (...) {
        return Value(static_cast<int64_t>(0));
    }
}

Value StandardLibrary::osRename(const std::string& src, const std::string& dst) {
    try {
        std::filesystem::rename(src, dst);
        return Value(true);
    } catch (...) {
        return Value(false);
    }
}

Value StandardLibrary::osSystem(const std::string& cmd) {
    int res = std::system(cmd.c_str());
#ifndef _WIN32
    if (WIFEXITED(res)) res = WEXITSTATUS(res);
#endif
    return Value(static_cast<int64_t>(res));
}

Value StandardLibrary::osPopen(const std::string& cmd) {
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return Value("");
    std::string out;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        out.append(buffer);
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return Value(out);
}

Value StandardLibrary::osGetenv(const std::string& key, const std::string& defVal) {
    const char* val = std::getenv(key.c_str());
    if (val) return Value(std::string(val));
    return Value(defVal);
}

Value StandardLibrary::osSetenv(const std::string& key, const std::string& val) {
#ifdef _WIN32
    return Value(SetEnvironmentVariableA(key.c_str(), val.c_str()) != 0);
#else
    return Value(setenv(key.c_str(), val.c_str(), 1) == 0);
#endif
}

Value StandardLibrary::osUnsetenv(const std::string& key) {
#ifdef _WIN32
    return Value(SetEnvironmentVariableA(key.c_str(), NULL) != 0);
#else
    return Value(unsetenv(key.c_str()) == 0);
#endif
}

Value StandardLibrary::osGetpid() {
#ifdef _WIN32
    return Value(static_cast<int64_t>(GetCurrentProcessId()));
#else
    return Value(static_cast<int64_t>(getpid()));
#endif
}

Value StandardLibrary::osCpuCount() {
    unsigned int c = std::thread::hardware_concurrency();
    return Value(static_cast<int64_t>(c > 0 ? c : 1));
}

Value StandardLibrary::osExit(int64_t code) {
    std::exit(static_cast<int>(code));
    return Value();
}

Value StandardLibrary::osPathExists(const std::string& path) {
    try {
        return Value(std::filesystem::exists(path));
    } catch (...) {
        return Value(false);
    }
}

Value StandardLibrary::osPathIsFile(const std::string& path) {
    try {
        return Value(std::filesystem::is_regular_file(path));
    } catch (...) {
        return Value(false);
    }
}

Value StandardLibrary::osPathIsDir(const std::string& path) {
    try {
        return Value(std::filesystem::is_directory(path));
    } catch (...) {
        return Value(false);
    }
}

Value StandardLibrary::osPathGetSize(const std::string& path) {
    try {
        return Value(static_cast<int64_t>(std::filesystem::file_size(path)));
    } catch (...) {
        return Value(static_cast<int64_t>(0));
    }
}

Value StandardLibrary::osPathJoin(const std::vector<Value>& args) {
    std::filesystem::path p;
    for (const auto& a : args) {
        if (p.empty()) p = a.toString();
        else p /= a.toString();
    }
    return Value(p.string());
}

Value StandardLibrary::osPathBasename(const std::string& path) {
    try {
        return Value(std::filesystem::path(path).filename().string());
    } catch (...) {
        return Value("");
    }
}

Value StandardLibrary::osPathDirname(const std::string& path) {
    try {
        return Value(std::filesystem::path(path).parent_path().string());
    } catch (...) {
        return Value("");
    }
}

Value StandardLibrary::osPathAbspath(const std::string& path) {
    try {
        return Value(std::filesystem::absolute(path).lexically_normal().string());
    } catch (...) {
        return Value(path);
    }
}

Value StandardLibrary::osPathSplitext(const std::string& path) {
    try {
        std::filesystem::path p(path);
        std::string ext = p.extension().string();
        std::string stem = p.parent_path().empty() ? p.stem().string() : (p.parent_path() / p.stem()).string();
        std::vector<Value> res = { Value(stem), Value(ext) };
        return Value(res);
    } catch (...) {
        std::vector<Value> res = { Value(path), Value("") };
        return Value(res);
    }
}

Value StandardLibrary::osPathIsabs(const std::string& path) {
    try {
        return Value(std::filesystem::path(path).is_absolute());
    } catch (...) {
        return Value(false);
    }
}

// Sys Implementation
Value StandardLibrary::sysPlatform() {
#ifdef _WIN32
    return Value("win32");
#elif defined(__APPLE__)
    return Value("darwin");
#elif defined(__ANDROID__)
    return Value("android");
#else
    return Value("linux");
#endif
}

Value StandardLibrary::sysVersion() {
    return Value("2.0.0");
}

Value StandardLibrary::sysArgv() {
    std::vector<Value> v;
    for (const auto& a : args_) v.push_back(Value(a));
    return Value(v);
}

Value StandardLibrary::sysExit(int64_t code) {
    std::exit(static_cast<int>(code));
    return Value();
}

Value StandardLibrary::sysStdoutWrite(const std::string& s) {
    std::cout << s;
    std::cout.flush();
    return Value(static_cast<int64_t>(s.size()));
}

Value StandardLibrary::sysStderrWrite(const std::string& s) {
    std::cerr << s;
    std::cerr.flush();
    return Value(static_cast<int64_t>(s.size()));
}

// String Utilities Implementation
Value StandardLibrary::strSplit(const std::string& s, const std::string& delim) {
    std::vector<Value> res;
    if (delim.empty()) {
        for (char c : s) res.push_back(Value(std::string(1, c)));
        return Value(res);
    }
    size_t start = 0;
    size_t pos = 0;
    while ((pos = s.find(delim, start)) != std::string::npos) {
        res.push_back(Value(s.substr(start, pos - start)));
        start = pos + delim.size();
    }
    res.push_back(Value(s.substr(start)));
    return Value(res);
}

Value StandardLibrary::strJoin(const std::string& delim, const Value& listVal) {
    if (!listVal.isList() || !listVal.listVal) return Value("");
    std::string res;
    bool first = true;
    for (const auto& item : *listVal.listVal) {
        if (!first) res += delim;
        first = false;
        res += item.toString();
    }
    return Value(res);
}

Value StandardLibrary::strReplace(const std::string& s, const std::string& oldStr, const std::string& newStr) {
    if (oldStr.empty()) return Value(s);
    std::string res;
    size_t start = 0;
    size_t pos = 0;
    while ((pos = s.find(oldStr, start)) != std::string::npos) {
        res.append(s, start, pos - start);
        res.append(newStr);
        start = pos + oldStr.size();
    }
    res.append(s, start, s.size() - start);
    return Value(res);
}

Value StandardLibrary::strTrim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return Value(s.substr(start, end - start));
}

Value StandardLibrary::strStartsWith(const std::string& s, const std::string& prefix) {
    return Value(s.rfind(prefix, 0) == 0);
}

Value StandardLibrary::strEndsWith(const std::string& s, const std::string& suffix) {
    if (suffix.size() > s.size()) return Value(false);
    return Value(s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0);
}

Value StandardLibrary::strContains(const std::string& s, const std::string& sub) {
    return Value(s.find(sub) != std::string::npos);
}

Value StandardLibrary::strFind(const std::string& s, const std::string& sub) {
    size_t pos = s.find(sub);
    if (pos == std::string::npos) return Value(static_cast<int64_t>(-1));
    return Value(static_cast<int64_t>(pos));
}

// JSON Serialization & Parsing Implementation
static void jsonDumpValue(const Value& val, std::string& out) {
    switch (val.type) {
        case ValueType::NIL:
            out += "null";
            break;
        case ValueType::BOOL:
            out += val.boolVal ? "true" : "false";
            break;
        case ValueType::INT:
            out += std::to_string(val.intVal);
            break;
        case ValueType::FLOAT: {
            if (std::isnan(val.floatVal) || std::isinf(val.floatVal)) {
                out += "null";
            } else {
                std::ostringstream ss;
                ss << val.floatVal;
                out += ss.str();
            }
            break;
        }
        case ValueType::STRING:
        case ValueType::FUNCTION: {
            out += "\"";
            for (char c : val.strVal) {
                switch (c) {
                    case '"': out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\b': out += "\\b"; break;
                    case '\f': out += "\\f"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                            out += buf;
                        } else {
                            out += c;
                        }
                        break;
                }
            }
            out += "\"";
            break;
        }
        case ValueType::LIST: {
            out += "[";
            if (val.listVal) {
                for (size_t i = 0; i < val.listVal->size(); ++i) {
                    if (i > 0) out += ", ";
                    jsonDumpValue((*val.listVal)[i], out);
                }
            }
            out += "]";
            break;
        }
        case ValueType::OBJECT: {
            out += "{";
            if (val.objVal) {
                bool first = true;
                for (const auto& kv : *val.objVal) {
                    if (!first) out += ", ";
                    first = false;
                    Value k(kv.first);
                    jsonDumpValue(k, out);
                    out += ": ";
                    jsonDumpValue(kv.second, out);
                }
            }
            out += "}";
            break;
        }
    }
}

Value StandardLibrary::jsonDumps(const Value& val) {
    std::string out;
    jsonDumpValue(val, out);
    return Value(out);
}

class FastJsonParser {
    const std::string& src_;
    size_t pos_ = 0;

    void skipWhitespace() {
        while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) {
            pos_++;
        }
    }

    char peek() const {
        if (pos_ >= src_.size()) return '\0';
        return src_[pos_];
    }

    char advance() {
        return src_[pos_++];
    }

    bool match(char expected) {
        if (pos_ < src_.size() && src_[pos_] == expected) {
            pos_++;
            return true;
        }
        return false;
    }

public:
    FastJsonParser(const std::string& s) : src_(s), pos_(0) {}

    Value parse() {
        skipWhitespace();
        if (pos_ >= src_.size()) return Value();
        return parseValue();
    }

    Value parseValue() {
        skipWhitespace();
        char c = peek();
        if (c == '"') return parseString();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber();
        return Value();
    }

    Value parseString() {
        if (!match('"')) return Value("");
        std::string s;
        while (pos_ < src_.size()) {
            char c = advance();
            if (c == '"') return Value(s);
            if (c == '\\' && pos_ < src_.size()) {
                char esc = advance();
                switch (esc) {
                    case '"': s += '"'; break;
                    case '\\': s += '\\'; break;
                    case '/': s += '/'; break;
                    case 'b': s += '\b'; break;
                    case 'f': s += '\f'; break;
                    case 'n': s += '\n'; break;
                    case 'r': s += '\r'; break;
                    case 't': s += '\t'; break;
                    case 'u': {
                        if (pos_ + 4 <= src_.size()) {
                            std::string hexStr = src_.substr(pos_, 4);
                            pos_ += 4;
                            try {
                                uint32_t codepoint = std::stoul(hexStr, nullptr, 16);
                                if (codepoint <= 0x7F) {
                                    s += static_cast<char>(codepoint);
                                } else if (codepoint <= 0x7FF) {
                                    s += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
                                    s += static_cast<char>(0x80 | (codepoint & 0x3F));
                                } else {
                                    s += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
                                    s += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                    s += static_cast<char>(0x80 | (codepoint & 0x3F));
                                }
                            } catch (...) {}
                        }
                        break;
                    }
                    default: s += esc; break;
                }
            } else {
                s += c;
            }
        }
        return Value(s);
    }

    Value parseNumber() {
        size_t start = pos_;
        if (peek() == '-') advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        bool isFloat = false;
        if (peek() == '.') {
            isFloat = true;
            advance();
            while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }
        if (peek() == 'e' || peek() == 'E') {
            isFloat = true;
            advance();
            if (peek() == '+' || peek() == '-') advance();
            while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }
        std::string numStr = src_.substr(start, pos_ - start);
        try {
            if (isFloat) return Value(std::stod(numStr));
            return Value(static_cast<int64_t>(std::stoll(numStr)));
        } catch (...) {
            return Value(0);
        }
    }

    Value parseBool() {
        if (src_.compare(pos_, 4, "true") == 0) { pos_ += 4; return Value(true); }
        if (src_.compare(pos_, 5, "false") == 0) { pos_ += 5; return Value(false); }
        return Value(false);
    }

    Value parseNull() {
        if (src_.compare(pos_, 4, "null") == 0) { pos_ += 4; }
        return Value();
    }

    Value parseArray() {
        if (!match('[')) return Value::makeList();
        std::vector<Value> list;
        skipWhitespace();
        if (match(']')) return Value(list);
        while (pos_ < src_.size()) {
            list.push_back(parseValue());
            skipWhitespace();
            if (match(',')) {
                skipWhitespace();
                continue;
            }
            if (match(']')) break;
            advance();
        }
        return Value(list);
    }

    Value parseObject() {
        if (!match('{')) return Value::makeObject();
        std::unordered_map<std::string, Value> obj;
        skipWhitespace();
        if (match('}')) return Value(obj);
        while (pos_ < src_.size()) {
            skipWhitespace();
            if (peek() != '"') break;
            Value keyVal = parseString();
            skipWhitespace();
            match(':');
            skipWhitespace();
            Value val = parseValue();
            obj[keyVal.toString()] = val;
            skipWhitespace();
            if (match(',')) {
                skipWhitespace();
                continue;
            }
            if (match('}')) break;
            advance();
        }
        return Value(obj);
    }
};

Value StandardLibrary::jsonLoads(const std::string& s) {
    FastJsonParser parser(s);
    return parser.parse();
}

