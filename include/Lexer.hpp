#pragma once

#include "Token.hpp"
#include <vector>
#include <string>
#include <unordered_map>

class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> tokenize();
    bool hasErrors() const { return !errors_.empty(); }
    const std::vector<std::string>& getErrors() const { return errors_; }

private:
    std::string source_;
    size_t cursor_;
    int line_;
    int column_;
    std::vector<int> indentStack_;
    int bracketNesting_;
    bool atLineStart_;
    std::unordered_map<std::string, TokenType> keywords_;
    std::vector<std::string> errors_;

    void initKeywords();
    bool isAtEnd() const;
    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);
    void handleIndentation(std::vector<Token>& tokens);
    Token readString(char quoteChar = '"', bool isMultiline = false);
    Token readNumber();
    Token readIdentifier();
};
