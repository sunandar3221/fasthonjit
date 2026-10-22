#pragma once

#include "Value.hpp"
#include <string>
#include <vector>

class StandardLibrary {
public:
    static void setArgs(const std::vector<std::string>& args);
    static const std::vector<std::string>& getArgs();

    static Value print(const std::vector<Value>& args);
    static Value silentPrint(const std::vector<Value>& args);
    static Value input(const std::string& prompt = "");
    static Value readFile(const std::string& path);
    static Value writeFile(const std::string& path, const std::string& content);
    static Value appendFile(const std::string& path, const std::string& content);
    static Value writeLines(const std::string& path, const Value& listVal);
    static Value openFile(const std::string& path, const std::string& mode = "w");
    static Value fileWrite(int64_t handleId, const std::string& content);
    static Value fileWriteLine(int64_t handleId, const std::string& line);
    static Value fileFlush(int64_t handleId);
    static Value fileClose(int64_t handleId);
    static Value httpGet(const std::string& url);
    static Value httpSend(const std::string& url, const std::string& data);

    static void setAppTitle(const std::string& title);
    static void setWindowSize(int width, int height);
    static Value runApp(int timeoutMs = -1);

    static Value toLower(const std::string& str);
    static Value toUpper(const std::string& str);
    static Value caseSensitive(const std::string& a, const std::string& b);
    static Value incaseSensitive(const std::string& a, const std::string& b);
    static Value toInt(const Value& val);
    static Value toFloat(const Value& val);

    // Math functions
    static Value mathSqrt(double val);
    static Value mathAbs(double val);
    static Value mathPow(double base, double exp);
    static Value mathFloor(double val);
    static Value mathCeil(double val);
    static Value mathRound(double val);
    static Value mathMin(double a, double b);
    static Value mathMax(double a, double b);
    static Value mathRandom();
    static Value mathRandom(double max);
    static Value mathRandom(double min, double max);
    static Value mathRandom(const std::vector<Value>& args);
    static Value mathRandomSeed(int64_t seed);
    static Value mathRandomSeed();
    static Value mathRandomSeed(const std::vector<Value>& args);
    static Value mathSin(double val);
    static Value mathCos(double val);
    static Value mathTan(double val);
    static Value mathAsin(double val);
    static Value mathAcos(double val);
    static Value mathAtan(double val);
    static Value mathAtan2(double y, double x);
    static Value mathSinh(double val);
    static Value mathCosh(double val);
    static Value mathTanh(double val);
    static Value mathExp(double val);
    static Value mathLog(double val);
    static Value mathLog10(double val);
    static Value mathLog2(double val);
    static Value mathDegrees(double rad);
    static Value mathRadians(double deg);
    static Value mathHypot(double x, double y);
    static Value mathTrunc(double val);
    static Value mathClamp(double val, double minVal, double maxVal);

    // Time functions
    static Value timeSleep(int64_t ms);
    static Value timeSleepS(double seconds);
    static Value timeNow();
    static Value timeTime();
    static Value timeTimeNs();
    static Value timeFormat(const std::string& fmt = "");

    // OS & Path functions
    static Value osName();
    static Value osSep();
    static Value osLinesep();
    static Value osDevnull();
    static Value osGetcwd();
    static Value osChdir(const std::string& path);
    static Value osListdir(const std::string& path = ".");
    static Value osMkdir(const std::string& path);
    static Value osMakedirs(const std::string& path);
    static Value osRemove(const std::string& path);
    static Value osRmdir(const std::string& path);
    static Value osRemovedirs(const std::string& path);
    static Value osRename(const std::string& src, const std::string& dst);
    static Value osSystem(const std::string& cmd);
    static Value osPopen(const std::string& cmd);
    static Value osGetenv(const std::string& key, const std::string& defVal = "");
    static Value osSetenv(const std::string& key, const std::string& val);
    static Value osUnsetenv(const std::string& key);
    static Value osGetpid();
    static Value osCpuCount();
    static Value osExit(int64_t code = 0);

    static Value osPathExists(const std::string& path);
    static Value osPathIsFile(const std::string& path);
    static Value osPathIsDir(const std::string& path);
    static Value osPathGetSize(const std::string& path);
    static Value osPathJoin(const std::vector<Value>& args);
    static Value osPathBasename(const std::string& path);
    static Value osPathDirname(const std::string& path);
    static Value osPathAbspath(const std::string& path);
    static Value osPathSplitext(const std::string& path);
    static Value osPathIsabs(const std::string& path);

    // Sys functions
    static Value sysPlatform();
    static Value sysVersion();
    static Value sysArgv();
    static Value sysExit(int64_t code = 0);
    static Value sysStdoutWrite(const std::string& s);
    static Value sysStderrWrite(const std::string& s);

    // String utility functions
    static Value strSplit(const std::string& s, const std::string& delim = " ");
    static Value strJoin(const std::string& delim, const Value& listVal);
    static Value strReplace(const std::string& s, const std::string& oldStr, const std::string& newStr);
    static Value strTrim(const std::string& s);
    static Value strStartsWith(const std::string& s, const std::string& prefix);
    static Value strEndsWith(const std::string& s, const std::string& suffix);
    static Value strContains(const std::string& s, const std::string& sub);
    static Value strFind(const std::string& s, const std::string& sub);

    // JSON functions
    static Value jsonDumps(const Value& val);
    static Value jsonLoads(const std::string& s);

    static std::string getAppTitle();
    static int getWindowWidth();
    static int getWindowHeight();

private:
    static std::string appTitle_;
    static int windowWidth_;
    static int windowHeight_;
    static std::vector<std::string> args_;
};
