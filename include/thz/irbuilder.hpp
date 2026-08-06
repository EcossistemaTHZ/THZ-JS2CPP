#pragma once

#include "thz/ast.hpp"
#include "thz/ir.hpp"

#include <map>
#include <string>
#include <vector>

namespace thz {

class IRBuilder {
public:
    explicit IRBuilder(std::unique_ptr<Program> prog);

    IRModule build();

private:
    std::unique_ptr<Program> prog_;

    IRModule mod_;
    IRFn* fn_ = nullptr;
    std::vector<Ins>& body() { return fn_->code; }

    // per-function state
    std::map<std::string, std::string> localMap_;      // source name -> ir name
    std::map<std::string, TypeInfo> localType_;
    std::vector<std::pair<std::string, TypeInfo>> localDecls_;
    int temp_ = 0, label_ = 0;

    // module-level symbol info
    std::map<std::string, TypeInfo> globalType_;
    std::vector<std::string> fnNames_;
    std::map<std::string, TypeInfo> fnRetType_;
    std::vector<std::string> classNames_;

    // control-flow stacks
    std::vector<std::size_t> breakStack_, continueStack_;
    int topLevel_ = 0;

    bool declaredFn(const std::string& n) const;
    TypeInfo scanRetType(const BlockStmt* body);

    std::string newTemp();
    size_t newLabel();
    IROperand constNum(double v, TypeInfo t);
    IROperand constStr(const std::string& s, TypeInfo t);
    IROperand tempOp(const std::string& n, TypeInfo t);
    void emit(Ins i);

    IROperand lowerExpr(const Expr* e);
    IROperand lowerBin(const BinaryExpr* e);
    IROperand lowerLogical(const Expr* e, BinOp op);
    IROperand lowerUnary(const UnaryExpr* e);
    IROperand lowerCall(const CallExpr* e);
    IROperand lowerMember(const MemberExpr* e);
    IROperand lowerAssign(const AssignExpr* e);
    IROperand lowerCond(const CondExpr* e);
    IROperand lowerArray(const ArrayExpr* e);
    IROperand lowerObject(const ObjectExpr* e);
    TypeInfo infer(const Expr* e);
    IROperand lowerMakeFn(const ArrowExpr* a);
    IROperand lowerFnDecl(const FunctionDecl* fd);

    void lowerStmt(const Stmt* s);
    void lowerBlock(const BlockStmt* b);
    void lowerVar(const VarStmt* s);
    void lowerIf(const IfStmt* s);
    void lowerWhile(const WhileStmt* s);
    void lowerDoWhile(const DoWhileStmt* s);
    void lowerFor(const ForStmt* s);
    void lowerForInOf(const ForInOfStmt* s);
    void lowerReturn(const ReturnStmt* s);
    void lowerThrow(const ThrowStmt* s);
    void lowerSwitch(const SwitchStmt* s);

    IRFn makeFunction(const std::string& name, const std::vector<Param>& params,
                      const BlockStmt* body, bool async, bool isMain = false);
    void buildClass(const ClassDef* c);
    void enterFn(IRFn& f, const std::vector<Param>& params);
    void leaveFn(IRFn* saved, std::map<std::string, std::string> savedMap,
                 std::map<std::string, TypeInfo> savedType,
                 std::vector<std::pair<std::string, TypeInfo>> savedDecls,
                 int savedTemp, int savedLabel);

    Op binOpOp(BinOp op);
    TypeInfo join(TypeInfo a, TypeInfo b);
    std::string cstring(const std::string& s);
};

} // namespace thz