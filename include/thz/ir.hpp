#pragma once

#include <string>
#include <vector>
#include <memory>

namespace thz {

// ---------------------------------------------------------------- Types
enum class T { Any, Num, Str, Bool, Null, Void, Array, Obj, Fn };

struct TypeInfo {
    T kind = T::Any;
    std::string ref;   // class name (Obj) or function name (Fn)
    TypeInfo() = default;
    TypeInfo(T k) : kind(k) {}
};

inline bool isNum(const TypeInfo& t) { return t.kind == T::Num; }
inline bool isStr(const TypeInfo& t) { return t.kind == T::Str; }

// ---------------------------------------------------------------- Operands & Instructions
struct IROperand {
    bool isConst = false;
    std::string name;
    double num = 0;
    std::string str;
    TypeInfo type;
};

enum class Op {
    Const, Load, Store, Alloca,
    Add, Sub, Mul, Div, Mod,
    Neg, Not, Plus, BitNot,
    Lt, Gt, Le, Ge, Eq, Ne, StrictEq, StrictNe,
    BitAnd, BitOr, BitXor, Shl, Shr,
    And, Or, Nullish,          // short-circuit
    Call, CallNew, CallMember, CallSuper,
    NewArray, ArraySpread, NewObject, ObjSpread,
    GetField, SetField, GetIndex, SetIndex,
    MakeFn,
    Return, Jump, JmpZ, JmpNZ, Label,
    Throw,
};

struct Ins {
    Op op = Op::Const;
    std::string dst;
    IROperand a, b;
    std::vector<IROperand> args;
    std::string member;
    TypeInfo type;
    size_t label = 0;
    size_t labelA = 0, labelB = 0;   // branch targets (then/else)
    bool namedCall = false;
    bool isSuper = false;
    std::string comment;
};

// ---------------------------------------------------------------- Functions / Classes / Module
struct IRFn {
    std::string name;
    std::vector<IROperand> params;
    TypeInfo retType;
    std::vector<Ins> code;
    std::vector<std::pair<std::string, TypeInfo>> locals;
    bool isMethod = false;
    bool isCtor = false;
    bool isStatic = false;
    bool isGetter = false;
    bool isSetter = false;
    bool isMain = false;
    std::string className;
};

struct IRField {
    std::string name;
    TypeInfo type;
    bool isStatic = false;
    bool hasInit = false;
    double initNum = 0;
    std::string initStr;
};

struct IRClass {
    std::string name;
    std::string base;
    std::vector<IRField> fields;
    std::vector<IRFn> methods;
    bool hasCtor = false;
};

struct IRModule {
    std::vector<IRFn> functions;
    std::vector<IRClass> classes;
    IRFn main;
    std::vector<std::pair<std::string, TypeInfo>> globals;
};

} // namespace thz