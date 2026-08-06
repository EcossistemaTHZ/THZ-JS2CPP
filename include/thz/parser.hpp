#pragma once

#include "thz/ast.hpp"
#include "thz/token.hpp"

#include <string>
#include <vector>
#include <stdexcept>

namespace thz {

struct ParseError : std::runtime_error {
    explicit ParseError(const std::string& m) : std::runtime_error(m) {}
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    std::unique_ptr<Program> parseProgram();

private:
    std::vector<Token> toks_;
    size_t pos_ = 0;
    bool noIn_ = false;

    // token helpers
    const Token& cur() const { return toks_[pos_]; }
    const Token& peekAhead(size_t n) const {
        size_t p = pos_ + n;
        return p < toks_.size() ? toks_[p] : toks_.back();
    }
    const Token& advance() { return toks_[pos_ < toks_.size() - 1 ? pos_++ : pos_]; }
    bool atEnd() const { return cur().kind == Tok::End; }
    bool check(Tok k) const { return cur().kind == k; }
    bool checkAhead(Tok k, size_t n) const { return peekAhead(n).kind == k; }
    bool match(Tok k);
    bool matchAny(std::initializer_list<Tok> ks);
    const Token& expect(Tok k, const std::string& what);
    [[noreturn]] void error(const std::string& msg) const;
    void mark(Expr& e);

    // statements
    StmtPtr parseStatement();
    StmtPtr parseBlock();
    std::unique_ptr<BlockStmt> parseBlockAsBlock();
    StmtPtr parseVarStmt(Tok kind);
    StmtPtr parseIf();
    StmtPtr parseWhile();
    StmtPtr parseDoWhile();
    StmtPtr parseFor();
    StmtPtr parseForInOf();
    StmtPtr parseSwitch();
    StmtPtr parseFunction(bool isExpr, const std::string& name);
    StmtPtr parseClass();
    StmtPtr parseReturn();
    StmtPtr parseTry();
    std::unique_ptr<BlockStmt> parseFunctionBody();

    // expressions (Pratt)
    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseTernary();
    ExprPtr parseBinary(int minPrec);
    ExprPtr parseUnary();
    ExprPtr parsePostfix();
    ExprPtr parseCallMember();
    ExprPtr parsePrimary();
    ExprPtr parseArrayLit();
    ExprPtr parseObjectLit();
    ExprPtr parseParenOrArrow();
    ExprPtr tryParseArrowAfterAsync();
    std::unique_ptr<BlockStmt> parseArrowBody();
    ExprPtr parseTemplateFromValue(const Token& t);

    std::vector<Param> parseParams();
    ExprPtr finishCall(ExprPtr callee, bool optional);

    // helpers for arrow / binary
    int binPrec(Tok t);
    BinOp binOp(Tok t);
    bool isAssignOp(Tok t);
};

} // namespace thz