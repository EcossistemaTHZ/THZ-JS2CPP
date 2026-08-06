#pragma once

#include "thz/ir.hpp"

#include <string>
#include <map>
#include <vector>
#include <set>

namespace thz {

class Codegen {
public:
    explicit Codegen(IRModule mod);

    std::string generate();

private:
    IRModule mod_;

    std::string out_;
    std::map<std::string, std::string> typeOf_;
    std::set<std::string> classNames_, fnNames_, globalNames_;
    int labelCounter_ = 0;

    std::string cppType(const TypeInfo& t);
    std::string operand(const IROperand& o);
    std::string argList(const std::vector<IROperand>& args);
    std::string cstr(const std::string& s);
    std::string tempDecls(const IRFn& f);
    void genFunction(const IRFn& f);
    void genMain(const IRFn& f);
    void genClass(const IRClass& c);
    void genIns(const Ins& in);
    void genExprIns(const Ins& in);
    void declareOp(const std::string& name, const TypeInfo& t);

    bool isClass(const std::string& n);
    bool isFn(const std::string& n);
    bool isGlobal(const std::string& n);
    bool isTemp(const std::string& n);
    static const char* runtimeHpp();
};

} // namespace thz