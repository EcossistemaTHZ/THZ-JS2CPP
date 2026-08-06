#pragma once

#include "thz/token.hpp"

#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace thz {

// ---------------------------------------------------------------- Expressions

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct Param { std::string name; ExprPtr def; bool rest = false; };

enum class ExprKind {
    Number, String, Bool, Null,
    Ident, Array, Object,
    Unary, Binary, Cond, Assign, Call, Member,
    ArrowExpr, Template, Sequence, This, Super, Undefined,
};

struct Expr {
    ExprKind kind;
    size_t line = 0, col = 0;
    explicit Expr(ExprKind k) : kind(k) {}
    virtual ~Expr() = default;
};

struct NumberExpr : Expr { double value; NumberExpr(double v) : Expr(ExprKind::Number), value(v) {} };
struct StringExpr : Expr { std::string value; StringExpr(std::string v) : Expr(ExprKind::String), value(std::move(v)) {} };
struct BoolExpr   : Expr { bool value; BoolExpr(bool v) : Expr(ExprKind::Bool), value(v) {} };
struct NullExpr   : Expr { NullExpr() : Expr(ExprKind::Null) {} };
struct UndefinedExpr : Expr { UndefinedExpr() : Expr(ExprKind::Undefined) {} };
struct ThisExpr   : Expr { ThisExpr() : Expr(ExprKind::This) {} };
struct SuperExpr  : Expr { SuperExpr() : Expr(ExprKind::Super) {} };
struct IdentExpr  : Expr { std::string name; IdentExpr(std::string n) : Expr(ExprKind::Ident), name(std::move(n)) {} };

struct ArrayExpr : Expr {
    std::vector<ExprPtr> items;   // empty entry -> hole, null -> ...
    std::vector<bool> isSpread;
    ArrayExpr() : Expr(ExprKind::Array) {}
};

struct ObjectProp {
    std::string key;
    bool isComputed = false;
    ExprPtr computedKey;
    ExprPtr value;
    bool isSpread = false;
    ExprPtr spread;
};
struct ObjectExpr : Expr {
    std::vector<ObjectProp> props;
    ObjectExpr() : Expr(ExprKind::Object) {}
};

struct UnaryExpr : Expr {
    Tok op;
    ExprPtr operand;
    bool isPrefix = true;
    UnaryExpr(Tok o, ExprPtr opd, bool pre = true) : Expr(ExprKind::Unary), op(o), operand(std::move(opd)), isPrefix(pre) {}
};

enum class BinOp { Add, Sub, Mul, Div, Mod,
                   Eq, Neq, StrictEq, StrictNeq, Lt, Gt, Le, Ge,
                   BitAnd, BitOr, BitXor, Shl, Shr,
                   LogicalAnd, LogicalOr, LogicalNullish, In, Instanceof };
struct BinaryExpr : Expr {
    BinOp op;
    ExprPtr left, right;
    BinaryExpr(BinOp o, ExprPtr l, ExprPtr r) : Expr(ExprKind::Binary), op(o), left(std::move(l)), right(std::move(r)) {}
};

struct AssignExpr : Expr {
    Tok op = Tok::Assign;
    ExprPtr target, value;
    AssignExpr(Tok o, ExprPtr t, ExprPtr v) : Expr(ExprKind::Assign), op(o), target(std::move(t)), value(std::move(v)) {}
};

struct CondExpr : Expr {
    ExprPtr cond, thenBranch, elseBranch;
    CondExpr(ExprPtr c, ExprPtr t, ExprPtr e) : Expr(ExprKind::Cond), cond(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {}
};

struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> args;
    bool optional = false;
    bool isNew = false;
    CallExpr() : Expr(ExprKind::Call) {}
};

struct MemberExpr : Expr {
    ExprPtr object;
    std::optional<std::string> name;   // set for .name access
    ExprPtr index;                     // set for [expr] access
    bool optional = false;
    MemberExpr() : Expr(ExprKind::Member) {}
};

struct TemplateExpr : Expr {
    std::vector<std::string> chunks;
    std::vector<ExprPtr> substitutions;
    TemplateExpr() : Expr(ExprKind::Template) {}
};

struct SequenceExpr : Expr {
    std::vector<ExprPtr> exprs;
    SequenceExpr() : Expr(ExprKind::Sequence) {}
};

struct ArrowExpr : Expr {
    std::vector<Param> params;
    std::unique_ptr<struct BlockStmt> body;   // block body or implicit-return block
    bool async = false;
    ArrowExpr() : Expr(ExprKind::ArrowExpr) {}
};

// ---------------------------------------------------------------- Statements
struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;
using BlockPtr = std::unique_ptr<struct BlockStmt>;

enum class StmtKind { Block, ExprStmt, Return, VarDecl, If, While, DoWhile, For,
                       ForInOf, Break, Continue, Switch, ClassDef, Function,
                       Throw, Try };

struct Stmt {
    StmtKind kind;
    size_t line = 0, col = 0;
    explicit Stmt(StmtKind k) : kind(k) {}
    virtual ~Stmt() = default;
};

struct BlockStmt : Stmt { std::vector<StmtPtr> body; BlockStmt() : Stmt(StmtKind::Block) {} };
struct ExprStmt   : Stmt { ExprPtr expr; explicit ExprStmt(ExprPtr e) : Stmt(StmtKind::ExprStmt), expr(std::move(e)) {} };
struct ReturnStmt : Stmt { ExprPtr value; bool hasValue = false; ReturnStmt() : Stmt(StmtKind::Return) {} };

struct VarDecl { std::string name; ExprPtr init; bool isConst = false; };
struct VarStmt : Stmt {
    Tok kind = Tok::Let;   // Let / Const / Var
    std::vector<VarDecl> decls;
    VarStmt() : Stmt(StmtKind::VarDecl) {}
};

struct IfStmt : Stmt {
    ExprPtr cond; StmtPtr thenBranch, elseBranch;
    IfStmt() : Stmt(StmtKind::If) {}
};
struct WhileStmt : Stmt { ExprPtr cond; StmtPtr body; WhileStmt() : Stmt(StmtKind::While) {} };
struct DoWhileStmt : Stmt { ExprPtr cond; StmtPtr body; DoWhileStmt() : Stmt(StmtKind::DoWhile) {} };
struct ForStmt : Stmt {
    StmtPtr init; ExprPtr cond; ExprPtr update; StmtPtr body;
    ForStmt() : Stmt(StmtKind::For) {}
};
struct ForInOfStmt : Stmt {
    bool isOf = false;
    std::string var;
    ExprPtr iterable; StmtPtr body;
    ForInOfStmt() : Stmt(StmtKind::ForInOf) {}
};
struct BreakStmt : Stmt { BreakStmt() : Stmt(StmtKind::Break) {} };
struct ContinueStmt : Stmt { ContinueStmt() : Stmt(StmtKind::Continue) {} };

struct SwitchCase { ExprPtr test; std::vector<StmtPtr> body; bool isDefault = false; };
struct SwitchStmt : Stmt {
    ExprPtr subject;
    std::vector<SwitchCase> cases;
    SwitchStmt() : Stmt(StmtKind::Switch) {}
};

// ---------------------------------------------------------------- Functions & Classes
struct FunctionDecl : Stmt {
    std::string name;
    std::vector<Param> params;
    std::unique_ptr<BlockStmt> body;
    bool async = false;
    bool isMethod = false;
    bool isStatic = false;
    bool isCtor = false;
    std::string className;
    FunctionDecl() : Stmt(StmtKind::Function) {}
};

struct ClassField {
    std::string name;
    ExprPtr init;
    bool isStatic = false;
};
struct ClassMethod {
    std::string name;
    std::vector<Param> params;
    std::unique_ptr<BlockStmt> body;
    bool isStatic = false;
    bool isCtor = false;
    bool isGetter = false;
    bool isSetter = false;
    bool async = false;
};
struct ClassDef : Stmt {
    std::string name;
    std::optional<std::string> base;
    std::vector<ClassField> fields;
    std::vector<ClassMethod> methods;
    ClassDef() : Stmt(StmtKind::ClassDef) {}
};

struct ThrowStmt : Stmt { ExprPtr value; explicit ThrowStmt(ExprPtr v) : Stmt(StmtKind::Throw), value(std::move(v)) {} };
struct TryStmt : Stmt {
    std::unique_ptr<BlockStmt> tryBlock;
    std::optional<std::string> catchParam;
    std::unique_ptr<BlockStmt> catchBlock;
    std::unique_ptr<BlockStmt> finallyBlock;
    TryStmt() : Stmt(StmtKind::Try) {}
};

// ---------------------------------------------------------------- Program
struct Program {
    std::vector<StmtPtr> body;
};

} // namespace thz