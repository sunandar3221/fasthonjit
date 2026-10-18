CXX ?= g++
CXXFLAGS ?= -std=c++20 -O3 -Iinclude
LDFLAGS ?=

UNAME_S := $(shell uname -s 2>/dev/null)

ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
    LDFLAGS += -lwininet -lgdi32 -luser32
    TARGET = fasthonjit.exe
    ALIAS1 = fasthon.exe
    ALIAS2 = eas.exe
else ifeq ($(findstring MSYS,$(UNAME_S)),MSYS)
    LDFLAGS += -lwininet -lgdi32 -luser32
    TARGET = fasthonjit.exe
    ALIAS1 = fasthon.exe
    ALIAS2 = eas.exe
else ifeq ($(OS),Windows_NT)
    LDFLAGS += -lwininet -lgdi32 -luser32
    TARGET = fasthonjit.exe
    ALIAS1 = fasthon.exe
    ALIAS2 = eas.exe
else
    TARGET = fasthonjit
    ALIAS1 = fasthon
    ALIAS2 = eas
endif

SRCS = src/AST.cpp src/AotGenerator.cpp src/Bytecode.cpp src/BytecodeCompiler.cpp \
       src/Diagnostic.cpp src/Environment.cpp src/Interpreter.cpp src/Jit.cpp \
       src/Lexer.cpp src/Main.cpp src/Parser.cpp src/Repl.cpp \
       src/StandardLibrary.cpp src/Token.cpp src/VM.cpp src/Value.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) $(LDFLAGS) -o $(TARGET)
	@cp $(TARGET) $(ALIAS1) 2>/dev/null || copy $(TARGET) $(ALIAS1) 2>nul || true
	@cp $(TARGET) $(ALIAS2) 2>/dev/null || copy $(TARGET) $(ALIAS2) 2>nul || true

clean:
	@rm -f $(TARGET) $(ALIAS1) $(ALIAS2) *.o 2>/dev/null || del /Q /F $(TARGET) $(ALIAS1) $(ALIAS2) *.o 2>nul || true

.PHONY: all clean
