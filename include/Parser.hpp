#pragma once

#include "Token.hpp"
#include "AST.hpp"
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    std::unique_ptr<BlockStmt> parseProgram();
    std::unique_ptr<Stmt> parseStatement();
    bool hasErrors() const { return !errors_.empty(); }
    const std::vector<std::string>& getErrors() const { return errors_; }

private:
    std::vector<Token> tokens_;
    size_t cursor_;
    std::unordered_map<std::string, int> functionArity_;
    int anonFnCounter_;
    std::vector<std::unique_ptr<FnDeclStmt>> hoistedAnonFns_;
    std::vector<std::string> errors_;
    void reportError(const std::string& message, const Token& token, const std::string& customRecommendation = "", const std::string& customHint = "");

    bool isAtEnd() const;
    const Token& peek() const;
    const Token& peekNext() const;
    const Token& previous() const;
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    void skipNewlines();
    bool isIndexOrPropAssign() const;

    std::unique_ptr<BlockStmt> parseBlock(bool stopAtElse = false);
    std::unique_ptr<Stmt> parsePrint(bool silent = false);
    std::unique_ptr<Stmt> parseWrite();
    std::unique_ptr<Stmt> parseApp();
    std::unique_ptr<Stmt> parseWindow();
    std::unique_ptr<Stmt> parseRun();
    std::unique_ptr<Stmt> parseSet();
    std::unique_ptr<Stmt> parseUse();
    std::unique_ptr<Stmt> parseIf(bool isElif = false);
    std::unique_ptr<Stmt> parseLoop();
    std::unique_ptr<Stmt> parseWhile();
    std::unique_ptr<Stmt> parseFor();
    std::unique_ptr<Stmt> parseRepeat();
    std::unique_ptr<Stmt> parseFnDecl();
    std::unique_ptr<Stmt> parseReturn();
    std::unique_ptr<Stmt> parseBreak();
    std::unique_ptr<Stmt> parseContinue();
    std::unique_ptr<Stmt> parseAssignmentOrExpr();
    std::unique_ptr<IfExpr> parseIfExpr(bool isElif = false);

    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseLogicalOr();
    std::unique_ptr<Expr> parseLogicalAnd();
    std::unique_ptr<Expr> parseEquality();
    std::unique_ptr<Expr> parseRelational();
    std::unique_ptr<Expr> parseAdditive();
    std::unique_ptr<Expr> parseMultiplicative();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePrimary();
    std::unique_ptr<Expr> parseCallOrPrimary();
    std::unique_ptr<Expr> parsePostfix(std::unique_ptr<Expr> expr);
    std::unique_ptr<Expr> parseStringInterpolation(const Token& strTok);

public:
    void setFunctionArity(const std::unordered_map<std::string, int>& arity) { functionArity_ = arity; }
};
