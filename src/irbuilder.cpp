#include "thz/irbuilder.hpp"

#include <sstream>
#include <cassert>

namespace thz {

static std::string esc(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\0': out += "\\0"; break;
        default: out += ch; break;
        }
    }
    return out;
}

std::string IRBuilder::cstring(const std::string& s) { return esc(s); }

IRBuilder::IRBuilder(std::unique_ptr<Program> prog) : prog_(std::move(prog)) {}

std::string IRBuilder::newTemp() { return "t" + std::to_string(++temp_); }
size_t IRBuilder::newLabel() { return ++label_; }

IROperand IRBuilder::constNum(double v, TypeInfo t) { IROperand o; o.isConst = true; o.num = v; o.type = t; return o; }
IROperand IRBuilder::constStr(const std::string& s, TypeInfo t) { IROperand o; o.isConst = true; o.str = s; o.type = t; return o; }
IROperand IRBuilder::tempOp(const std::string& n, TypeInfo t) { IROperand o; o.isConst = false; o.name = n; o.type = t; return o; }

void IRBuilder::emit(Ins i) { body().push_back(std::move(i)); }

TypeInfo IRBuilder::join(TypeInfo a, TypeInfo b) {
    if (a.kind == b.kind) return a;
    if (a.kind == T::Any) return b;
    if (b.kind == T::Any) return a;
    return TypeInfo(T::Any);
}

Op IRBuilder::binOpOp(BinOp op) {
    switch (op) {
    case BinOp::Add: return Op::Add;
    case BinOp::Sub: return Op::Sub;
    case BinOp::Mul: return Op::Mul;
    case BinOp::Div: return Op::Div;
    case BinOp::Mod: return Op::Mod;
    case BinOp::Eq: return Op::Eq;
    case BinOp::Neq: return Op::Ne;
    case BinOp::StrictEq: return Op::StrictEq;
    case BinOp::StrictNeq: return Op::StrictNe;
    case BinOp::Lt: return Op::Lt;
    case BinOp::Gt: return Op::Gt;
    case BinOp::Le: return Op::Le;
    case BinOp::Ge: return Op::Ge;
    case BinOp::BitAnd: return Op::BitAnd;
    case BinOp::BitOr: return Op::BitOr;
    case BinOp::BitXor: return Op::BitXor;
    case BinOp::Shl: return Op::Shl;
    case BinOp::Shr: return Op::Shr;
    case BinOp::LogicalAnd: return Op::And;
    case BinOp::LogicalOr: return Op::Or;
    case BinOp::LogicalNullish: return Op::Nullish;
    case BinOp::In: return Op::CallMember; // placeholder, handled specially
    case BinOp::Instanceof: return Op::CallMember;
    }
    return Op::Add;
}

// ---------------------------------------------------------------- Type inference
TypeInfo IRBuilder::infer(const Expr* e) {
    switch (e->kind) {
    case ExprKind::Number: return TypeInfo(T::Num);
    case ExprKind::String: return TypeInfo(T::Str);
    case ExprKind::Bool:   return TypeInfo(T::Bool);
    case ExprKind::Null:   return TypeInfo(T::Null);
    case ExprKind::Undefined: return TypeInfo(T::Any);
    case ExprKind::This: {
        TypeInfo t(T::Obj);
        t.ref = fn_ ? fn_->className : "";
        return t;
    }
    case ExprKind::Ident: {
        const auto* id = static_cast<const IdentExpr*>(e);
        auto it = localType_.find(id->name);
        if (it != localType_.end()) return it->second;
        auto g = globalType_.find(id->name);
        if (g != globalType_.end()) return g->second;
        TypeInfo t(T::Any);
        return t;
    }
    case ExprKind::Array:  return TypeInfo(T::Array);
    case ExprKind::Object: return TypeInfo(T::Obj);
    case ExprKind::ArrowExpr: { TypeInfo t(T::Fn); return t; }
    case ExprKind::Template: return TypeInfo(T::Str);
    case ExprKind::Sequence: {
        const auto* s = static_cast<const SequenceExpr*>(e);
        return s->exprs.empty() ? TypeInfo(T::Any) : infer(s->exprs.back().get());
    }
    case ExprKind::Unary: {
        const auto* u = static_cast<const UnaryExpr*>(e);
        switch (u->op) {
        case Tok::Bang: return TypeInfo(T::Bool);
        case Tok::Typeof: return TypeInfo(T::Str);
        case Tok::Delete: return TypeInfo(T::Bool);
        case Tok::Plus: case Tok::Minus: return TypeInfo(T::Num);
        default: return infer(u->operand.get());
        }
    }
    case ExprKind::Binary: {
        const auto* b = static_cast<const BinaryExpr*>(e);
        if (b->op == BinOp::LogicalAnd || b->op == BinOp::LogicalOr) return join(infer(b->left.get()), infer(b->right.get()));
        if (b->op == BinOp::LogicalNullish) return join(infer(b->left.get()), infer(b->right.get()));
        switch (b->op) {
        case BinOp::Eq: case BinOp::Neq: case BinOp::StrictEq: case BinOp::StrictNeq:
        case BinOp::Lt: case BinOp::Gt: case BinOp::Le: case BinOp::Ge:
        case BinOp::In: case BinOp::Instanceof:
            return TypeInfo(T::Bool);
        case BinOp::Add: {
            TypeInfo l = infer(b->left.get()), r = infer(b->right.get());
            if (l.kind == T::Str || r.kind == T::Str) return TypeInfo(T::Str);
            return TypeInfo(T::Num);
        }
        default: return TypeInfo(T::Num);
        }
    }
    case ExprKind::Cond: {
        const auto* c = static_cast<const CondExpr*>(e);
        return join(infer(c->thenBranch.get()), infer(c->elseBranch.get()));
    }
    case ExprKind::Assign: {
        const auto* a = static_cast<const AssignExpr*>(e);
        return infer(a->value.get());
    }
    case ExprKind::Call: {
        const auto* c = static_cast<const CallExpr*>(e);
        if (c->callee && c->callee->kind == ExprKind::Ident) {
            const std::string& n = static_cast<const IdentExpr*>(c->callee.get())->name;
            auto it = fnRetType_.find(n);
            if (it != fnRetType_.end()) return it->second;
        }
        return TypeInfo(T::Any);
    }
    case ExprKind::Member: return TypeInfo(T::Any);
    default: return TypeInfo(T::Any);
    }
}

// ---------------------------------------------------------------- Expression lowering
IROperand IRBuilder::lowerExpr(const Expr* e) {
    switch (e->kind) {
    case ExprKind::Number: {
        const auto* n = static_cast<const NumberExpr*>(e);
        return constNum(n->value, TypeInfo(T::Num));
    }
    case ExprKind::String: {
        const auto* s = static_cast<const StringExpr*>(e);
        return constStr(s->value, TypeInfo(T::Str));
    }
    case ExprKind::Bool: {
        const auto* b = static_cast<const BoolExpr*>(e);
        IROperand o = constNum(b->value ? 1.0 : 0.0, TypeInfo(T::Bool));
        return o;
    }
    case ExprKind::Null: {
        IROperand o = constNum(0, TypeInfo(T::Null));
        return o;
    }
    case ExprKind::Undefined: {
        IROperand o = constNum(0, TypeInfo(T::Null));
        return o;
    }
    case ExprKind::This: {
        TypeInfo t(T::Obj);
        t.ref = fn_ ? fn_->className : "";
        return tempOp("this", t);
    }
    case ExprKind::Ident: {
        const auto* id = static_cast<const IdentExpr*>(e);
        return tempOp(id->name, infer(e));
    }
    case ExprKind::Binary: {
        const auto* b = static_cast<const BinaryExpr*>(e);
        if (b->op == BinOp::LogicalAnd || b->op == BinOp::LogicalOr || b->op == BinOp::LogicalNullish) {
            return lowerLogical(e, b->op);
        }
        return lowerBin(b);
    }
    case ExprKind::Unary: return lowerUnary(static_cast<const UnaryExpr*>(e));
    case ExprKind::Cond: return lowerCond(static_cast<const CondExpr*>(e));
    case ExprKind::Assign: return lowerAssign(static_cast<const AssignExpr*>(e));
    case ExprKind::Call: return lowerCall(static_cast<const CallExpr*>(e));
    case ExprKind::Member: return lowerMember(static_cast<const MemberExpr*>(e));
    case ExprKind::Array: return lowerArray(static_cast<const ArrayExpr*>(e));
    case ExprKind::Object: return lowerObject(static_cast<const ObjectExpr*>(e));
    case ExprKind::ArrowExpr: return lowerMakeFn(static_cast<const ArrowExpr*>(e));
    case ExprKind::Template: {
        const auto* t = static_cast<const TemplateExpr*>(e);
        IROperand acc = constStr("", TypeInfo(T::Str));
        if (t->chunks.size() > 0) acc = constStr(t->chunks[0], TypeInfo(T::Str));
        for (size_t i = 0; i < t->substitutions.size(); ++i) {
            IROperand part = lowerExpr(t->substitutions[i].get());
            std::string d = newTemp();
            Ins in; in.op = Op::Add; in.dst = d; in.a = acc; in.b = part; in.type = TypeInfo(T::Str);
            emit(in);
            acc = tempOp(d, TypeInfo(T::Str));
            if (i + 1 < t->chunks.size()) {
                std::string d2 = newTemp();
                Ins in2; in2.op = Op::Add; in2.dst = d2; in2.a = acc; in2.b = constStr(t->chunks[i + 1], TypeInfo(T::Str)); in2.type = TypeInfo(T::Str);
                emit(in2);
                acc = tempOp(d2, TypeInfo(T::Str));
            }
        }
        return acc;
    }
    case ExprKind::Sequence: {
        const auto* s = static_cast<const SequenceExpr*>(e);
        IROperand last = constNum(0, TypeInfo(T::Any));
        for (auto& x : s->exprs) last = lowerExpr(x.get());
        return last;
    }
    case ExprKind::Super: {
        TypeInfo t(T::Obj);
        t.ref = fn_ ? fn_->className : "";
        return tempOp("super", t);
    }
    }
    return constNum(0, TypeInfo(T::Any));
}

IROperand IRBuilder::lowerBin(const BinaryExpr* b) {
    TypeInfo ret = infer(b);
    if (b->op == BinOp::In || b->op == BinOp::Instanceof) {
        // obj in x  /  x instanceof Y
        IROperand left = lowerExpr(b->left.get());
        IROperand right = lowerExpr(b->right.get());
        std::string d = newTemp();
        Ins in; in.op = (b->op == BinOp::In) ? Op::GetField : Op::CallMember;
        in.dst = d;
        in.a = right;
        in.member = "__instanceof_in_placeholder";
        in.args.push_back(left);
        in.type = ret;
        emit(in);
        return tempOp(d, ret);
    }
    IROperand left = lowerExpr(b->left.get());
    IROperand right = lowerExpr(b->right.get());
    std::string d = newTemp();
    Ins in;
    in.op = binOpOp(b->op);
    in.dst = d;
    in.a = left;
    in.b = right;
    in.type = ret;
    emit(in);
    return tempOp(d, ret);
}

IROperand IRBuilder::lowerLogical(const Expr* e, BinOp op) {
    TypeInfo ret = infer(e);
    IROperand res = tempOp(newTemp(), ret);
    Ins dec; dec.op = Op::Alloca; dec.dst = res.name; dec.type = ret; emit(dec);

    IROperand left;
    if (e->kind == ExprKind::Binary) {
        left = lowerExpr(static_cast<const BinaryExpr*>(e)->left.get());
    } else if (e->kind == ExprKind::Cond) {
        left = lowerExpr(static_cast<const CondExpr*>(e)->cond.get());
    } else {
        left = lowerExpr(e);
    }

    size_t Lend = newLabel();
    if (op == BinOp::LogicalAnd || op == BinOp::LogicalNullish) {
        size_t Lb = newLabel();
        Ins j; j.op = Op::JmpNZ; j.a = left; j.label = Lb; emit(j);
        Ins st1; st1.op = Op::Store; st1.dst = res.name; st1.a = left; st1.type = ret; emit(st1);
        Ins jmp; jmp.op = Op::Jump; jmp.label = Lend; emit(jmp);
        Ins lb; lb.op = Op::Label; lb.label = Lb; emit(lb);
        IROperand right = lowerExpr(static_cast<const BinaryExpr*>(e)->right.get());
        Ins st2; st2.op = Op::Store; st2.dst = res.name; st2.a = right; st2.type = ret; emit(st2);
    } else { // Or
        size_t Lb = newLabel();
        Ins j; j.op = Op::JmpZ; j.a = left; j.label = Lb; emit(j);
        Ins st1; st1.op = Op::Store; st1.dst = res.name; st1.a = left; st1.type = ret; emit(st1);
        Ins jmp; jmp.op = Op::Jump; jmp.label = Lend; emit(jmp);
        Ins lb; lb.op = Op::Label; lb.label = Lb; emit(lb);
        IROperand right = lowerExpr(static_cast<const BinaryExpr*>(e)->right.get());
        Ins st2; st2.op = Op::Store; st2.dst = res.name; st2.a = right; st2.type = ret; emit(st2);
    }
    Ins le; le.op = Op::Label; le.label = Lend; emit(le);
    return res;
}

IROperand IRBuilder::lowerUnary(const UnaryExpr* u) {
    switch (u->op) {
    case Tok::Bang: {
        IROperand v = lowerExpr(u->operand.get());
        std::string d = newTemp();
        Ins in; in.op = Op::Not; in.dst = d; in.a = v; in.type = TypeInfo(T::Bool); emit(in);
        return tempOp(d, TypeInfo(T::Bool));
    }
    case Tok::Typeof: {
        IROperand v = lowerExpr(u->operand.get());
        std::string d = newTemp();
        Ins in; in.op = Op::CallMember; in.dst = d; in.member = "__typeof";
        in.args.push_back(v); in.type = TypeInfo(T::Str); emit(in);
        return tempOp(d, TypeInfo(T::Str));
    }
    case Tok::Plus: {
        IROperand v = lowerExpr(u->operand.get());
        std::string d = newTemp();
        Ins in; in.op = Op::Plus; in.dst = d; in.a = v; in.type = TypeInfo(T::Num); emit(in);
        return tempOp(d, TypeInfo(T::Num));
    }
    case Tok::Minus: {
        IROperand v = lowerExpr(u->operand.get());
        std::string d = newTemp();
        Ins in; in.op = Op::Neg; in.dst = d; in.a = v; in.type = TypeInfo(T::Num); emit(in);
        return tempOp(d, TypeInfo(T::Num));
    }
    case Tok::Tilde: {
        IROperand v = lowerExpr(u->operand.get());
        std::string d = newTemp();
        Ins in; in.op = Op::BitNot; in.dst = d; in.a = v; in.type = TypeInfo(T::Num); emit(in);
        return tempOp(d, TypeInfo(T::Num));
    }
    case Tok::PlusPlus: case Tok::MinusMinus: {
        // x++ / ++x / x-- / --x : handled as x = x +- 1
        IROperand cur = lowerExpr(u->operand.get());
        IROperand one = constNum(1.0, TypeInfo(T::Num));
        std::string d = newTemp();
        Ins in; in.op = (u->op == Tok::PlusPlus) ? Op::Add : Op::Sub;
        in.dst = d; in.a = cur; in.b = one; in.type = infer(u->operand.get()); emit(in);
        if (u->operand->kind == ExprKind::Ident) {
            Ins st; st.op = Op::Store; st.dst = u->operand->kind == ExprKind::Ident ? static_cast<const IdentExpr*>(u->operand.get())->name : d;
            st.a = tempOp(d, infer(u->operand.get()));
            emit(st);
        } else if (u->operand->kind == ExprKind::Member) {
            const auto* m = static_cast<const MemberExpr*>(u->operand.get());
            IROperand obj = lowerExpr(m->object.get());
            Ins st; st.op = Op::SetField; st.a = obj; st.member = m->name ? *m->name : "";
            st.b = tempOp(d, infer(u->operand.get()));
            emit(st);
        }
        IROperand res = tempOp(d, infer(u->operand.get()));
        if (!u->isPrefix) {
            // postfix returns old value: old = operand (before increment)
            IROperand old = lowerExpr(u->operand.get());
            return old;
        }
        return res;
    }
    case Tok::Await:
    case Tok::Void:
    case Tok::Delete:
    default:
        return lowerExpr(u->operand.get());
    }
}

IROperand IRBuilder::lowerCall(const CallExpr* c) {
    // super ctor call
    if (c->callee && c->callee->kind == ExprKind::Super && fn_ && fn_->isCtor) {
        std::vector<IROperand> args;
        for (auto& a : c->args) args.push_back(lowerExpr(a.get()));
        std::string d = newTemp();
        Ins in; in.op = Op::CallSuper; in.dst = d; in.args = std::move(args); in.type = TypeInfo(T::Any); emit(in);
        return tempOp(d, TypeInfo(T::Any));
    }

    // super.method(args)
    if (c->callee && c->callee->kind == ExprKind::Member) {
        const auto* m = static_cast<const MemberExpr*>(c->callee.get());
        if (m->object && m->object->kind == ExprKind::Super) {
            std::vector<IROperand> args;
            for (auto& a : c->args) args.push_back(lowerExpr(a.get()));
            std::string d = newTemp();
            Ins in; in.op = Op::CallMember; in.dst = d;
            in.a = tempOp("super", TypeInfo(T::Obj));
            in.member = *m->name;
            in.isSuper = true;
            in.args = std::move(args);
            in.type = TypeInfo(T::Any);
            emit(in);
            return tempOp(d, TypeInfo(T::Any));
        }
    }

    if (c->callee && c->callee->kind == ExprKind::Member) {
        const auto* m = static_cast<const MemberExpr*>(c->callee.get());
        IROperand obj = lowerExpr(m->object.get());
        std::vector<IROperand> args;
        for (auto& a : c->args) args.push_back(lowerExpr(a.get()));
        std::string d = newTemp();
        Ins in; in.op = Op::CallMember; in.dst = d; in.a = obj; in.member = m->name ? *m->name : ""; in.args = std::move(args); in.type = infer(c); emit(in);
        return tempOp(d, infer(c));
    }

    if (c->callee && c->callee->kind == ExprKind::Ident) {
        const std::string& name = static_cast<const IdentExpr*>(c->callee.get())->name;
        if (declaredFn(name)) {
            std::vector<IROperand> args;
            for (auto& a : c->args) args.push_back(lowerExpr(a.get()));
            std::string d = newTemp();
            Ins in; in.op = Op::Call; in.dst = d;
            in.a = tempOp(name, TypeInfo(T::Fn));
            in.namedCall = true;
            in.args = std::move(args);
            in.type = infer(c);
            emit(in);
            return tempOp(d, in.type);
        }
    }

    // dynamic call
    IROperand callee = lowerExpr(c->callee.get());
    std::vector<IROperand> args;
    for (auto& a : c->args) args.push_back(lowerExpr(a.get()));
    std::string d = newTemp();
    Ins in; in.op = Op::Call; in.dst = d; in.a = callee; in.args = std::move(args); in.type = TypeInfo(T::Any); emit(in);
    return tempOp(d, TypeInfo(T::Any));
}

IROperand IRBuilder::lowerMember(const MemberExpr* m) {
    IROperand obj = lowerExpr(m->object.get());
    std::string d = newTemp();
    Ins in; in.op = Op::GetField; in.dst = d; in.a = obj; in.type = TypeInfo(T::Any);
    if (m->name) {
        in.member = *m->name;
    } else {
        IROperand idx = lowerExpr(m->index.get());
        in.b = idx;
        in.op = Op::GetIndex;
    }
    emit(in);
    return tempOp(d, TypeInfo(T::Any));
}

IROperand IRBuilder::lowerAssign(const AssignExpr* a) {
    const Expr* tgt = a->target.get();
    if (a->op == Tok::Assign) {
        IROperand val = lowerExpr(a->value.get());
        if (tgt->kind == ExprKind::Ident) {
            const std::string& n = static_cast<const IdentExpr*>(tgt)->name;
            Ins st; st.op = Op::Store; st.dst = n; st.a = val; st.type = infer(a->value.get()); emit(st);
            return tempOp(n, infer(a->value.get()));
        }
        if (tgt->kind == ExprKind::Member) {
            const auto* m = static_cast<const MemberExpr*>(tgt);
            IROperand obj = lowerExpr(m->object.get());
            Ins st; st.op = Op::SetField; st.a = obj; st.b = val;
            if (m->name) { st.member = *m->name; }
            else { st.op = Op::SetIndex; st.b = lowerExpr(m->index.get()); Ins real; real.op = Op::SetIndex; real.a = obj; real.b = st.b; real.args.push_back(val); emit(real); return val; }
            emit(st);
            return val;
        }
        return val;
    }
    // compound: target = target op value
    IROperand val = lowerExpr(a->value.get());
    IROperand lhs;
    std::string lhsName;
    if (tgt->kind == ExprKind::Ident) {
        lhsName = static_cast<const IdentExpr*>(tgt)->name;
        lhs = tempOp(lhsName, infer(tgt));
    } else if (tgt->kind == ExprKind::Member) {
        lhs = lowerExpr(tgt); // reads field
    } else {
        lhs = lowerExpr(tgt);
    }
    BinOp bo;
    switch (a->op) {
    case Tok::PlusAssign: bo = BinOp::Add; break;
    case Tok::MinusAssign: bo = BinOp::Sub; break;
    case Tok::StarAssign: bo = BinOp::Mul; break;
    case Tok::SlashAssign: bo = BinOp::Div; break;
    case Tok::PercentAssign: bo = BinOp::Mod; break;
    case Tok::ShlAssign: bo = BinOp::Shl; break;
    case Tok::ShrAssign: bo = BinOp::Shr; break;
    case Tok::BitAndAssign: bo = BinOp::BitAnd; break;
    case Tok::BitOrAssign: bo = BinOp::BitOr; break;
    case Tok::BitXorAssign: bo = BinOp::BitXor; break;
    default: bo = BinOp::Add; break;
    }
    std::string d = newTemp();
    Ins in; in.op = binOpOp(bo); in.dst = d; in.a = lhs; in.b = val; in.type = infer(a->value.get()); emit(in);
    if (tgt->kind == ExprKind::Ident) {
        Ins st; st.op = Op::Store; st.dst = lhsName; st.a = tempOp(d, infer(a->value.get())); emit(st);
    } else if (tgt->kind == ExprKind::Member) {
        const auto* m = static_cast<const MemberExpr*>(tgt);
        IROperand obj = lowerExpr(m->object.get());
        Ins st; st.op = Op::SetField; st.a = obj; st.member = m->name ? *m->name : ""; st.b = tempOp(d, infer(a->value.get())); emit(st);
    }
    return tempOp(d, infer(a->value.get()));
}

IROperand IRBuilder::lowerCond(const CondExpr* c) {
    TypeInfo ret = infer(c);
    IROperand res = tempOp(newTemp(), ret);
    Ins dec; dec.op = Op::Alloca; dec.dst = res.name; dec.type = ret; emit(dec);

    IROperand cond = lowerExpr(c->cond.get());
    size_t Lthen = newLabel(), Lelse = newLabel(), Lend = newLabel();
    Ins j; j.op = Op::JmpNZ; j.a = cond; j.label = Lthen; emit(j);
    Ins jel; jel.op = Op::Jump; jel.label = Lelse; emit(jel);
    Ins lthen; lthen.op = Op::Label; lthen.label = Lthen; emit(lthen);
    IROperand tb = lowerExpr(c->thenBranch.get());
    Ins st1; st1.op = Op::Store; st1.dst = res.name; st1.a = tb; st1.type = ret; emit(st1);
    Ins jend1; jend1.op = Op::Jump; jend1.label = Lend; emit(jend1);
    Ins lelse; lelse.op = Op::Label; lelse.label = Lelse; emit(lelse);
    IROperand eb = lowerExpr(c->elseBranch.get());
    Ins st2; st2.op = Op::Store; st2.dst = res.name; st2.a = eb; st2.type = ret; emit(st2);
    Ins lend; lend.op = Op::Label; lend.label = Lend; emit(lend);
    return res;
}

IROperand IRBuilder::lowerArray(const ArrayExpr* a) {
    std::string d = newTemp();
    Ins in; in.op = Op::NewArray; in.dst = d; in.type = TypeInfo(T::Array); emit(in);
    IROperand arr = tempOp(d, TypeInfo(T::Array));
    size_t n = a->items.size();
    for (size_t i = 0; i < n; ++i) {
        if (!a->items[i]) {
            Ins hole; hole.op = Op::CallMember; hole.dst = newTemp(); hole.member = "__push_hole"; hole.a = arr; hole.type = TypeInfo(T::Any); emit(hole);
            continue;
        }
        if (a->isSpread[i]) {
            IROperand spread = lowerExpr(a->items[i].get());
            Ins sp; sp.op = Op::ArraySpread; sp.a = arr; sp.b = spread; sp.type = TypeInfo(T::Array); emit(sp);
            continue;
        }
        IROperand item = lowerExpr(a->items[i].get());
        Ins p; p.op = Op::CallMember; p.dst = newTemp(); p.a = arr; p.member = "__push"; p.args.push_back(item); p.type = TypeInfo(T::Any); emit(p);
    }
    return arr;
}

IROperand IRBuilder::lowerObject(const ObjectExpr* o) {
    std::string d = newTemp();
    Ins in; in.op = Op::NewObject; in.dst = d; in.type = TypeInfo(T::Obj); emit(in);
    IROperand obj = tempOp(d, TypeInfo(T::Obj));
    for (const auto& p : o->props) {
        if (p.isSpread) {
            IROperand spread = lowerExpr(p.spread.get());
            Ins sp; sp.op = Op::ObjSpread; sp.a = obj; sp.b = spread; sp.type = TypeInfo(T::Obj); emit(sp);
            continue;
        }
        IROperand val;
        std::string key = p.key;
        if (p.isComputed) {
            IROperand k = lowerExpr(p.computedKey.get());
            key = "__computed";
        }
        val = lowerExpr(p.value.get());
        Ins sf; sf.op = Op::SetField; sf.a = obj; sf.member = key; sf.b = val; sf.type = TypeInfo(T::Obj); emit(sf);
    }
    return obj;
}

IROperand IRBuilder::lowerMakeFn(const ArrowExpr* a) {
    static int fnCounter = 0;
    std::string name = "__fn" + std::to_string(++fnCounter);
    IRFn f = makeFunction(name, a->params, a->body.get(), a->async);
    mod_.functions.push_back(std::move(f));
    fnNames_.push_back(name);
    TypeInfo t(T::Fn);
    t.ref = name;
    std::string d = newTemp();
    Ins in; in.op = Op::MakeFn; in.dst = d; in.member = name; in.type = t; emit(in);
    return tempOp(d, t);
}

IROperand IRBuilder::lowerFnDecl(const FunctionDecl* fd) {
    static int fnCounter = 0;
    std::string name = "__fnexpr" + std::to_string(++fnCounter);
    IRFn f = makeFunction(name, fd->params, fd->body.get(), fd->async);
    mod_.functions.push_back(std::move(f));
    fnNames_.push_back(name);
    TypeInfo t(T::Fn);
    t.ref = name;
    std::string d = newTemp();
    Ins in; in.op = Op::MakeFn; in.dst = d; in.member = name; in.type = t; emit(in);
    return tempOp(d, t);
}

// ---------------------------------------------------------------- Statements
void IRBuilder::lowerStmt(const Stmt* s) {
    switch (s->kind) {
    case StmtKind::Block: lowerBlock(static_cast<const BlockStmt*>(s)); break;
    case StmtKind::VarDecl: lowerVar(static_cast<const VarStmt*>(s)); break;
    case StmtKind::ExprStmt: lowerExpr(static_cast<const ExprStmt*>(s)->expr.get()); break;
    case StmtKind::If: lowerIf(static_cast<const IfStmt*>(s)); break;
    case StmtKind::While: lowerWhile(static_cast<const WhileStmt*>(s)); break;
    case StmtKind::DoWhile: lowerDoWhile(static_cast<const DoWhileStmt*>(s)); break;
    case StmtKind::For: lowerFor(static_cast<const ForStmt*>(s)); break;
    case StmtKind::ForInOf: lowerForInOf(static_cast<const ForInOfStmt*>(s)); break;
    case StmtKind::Break: { Ins b; b.op = Op::Jump; b.label = breakStack_.empty() ? 0 : breakStack_.back(); b.comment = "break"; emit(b); break; }
    case StmtKind::Continue: { Ins c; c.op = Op::Jump; c.label = continueStack_.empty() ? 0 : continueStack_.back(); c.comment = "continue"; emit(c); break; }
    case StmtKind::Switch: lowerSwitch(static_cast<const SwitchStmt*>(s)); break;
    case StmtKind::Function: {
        const auto* fd = static_cast<const FunctionDecl*>(s);
        IRFn f = makeFunction(fd->name, fd->params, fd->body.get(), fd->async);
        mod_.functions.push_back(std::move(f));
        fnNames_.push_back(fd->name);
        break;
    }
    case StmtKind::ClassDef: buildClass(static_cast<const ClassDef*>(s)); break;
    case StmtKind::Return: lowerReturn(static_cast<const ReturnStmt*>(s)); break;
    case StmtKind::Throw: lowerThrow(static_cast<const ThrowStmt*>(s)); break;
    case StmtKind::Try: break; // try/catch not yet in IR (documented limitation)
    }
}

void IRBuilder::lowerBlock(const BlockStmt* b) {
    for (auto& s : b->body) lowerStmt(s.get());
}

void IRBuilder::lowerVar(const VarStmt* s) {
    for (auto& d : s->decls) {
        bool isMainTop = fn_->isMain && topLevel_ == 0;
        TypeInfo t = d.init ? infer(d.init.get()) : TypeInfo(T::Any);
        std::string irName;
        if (isMainTop) {
            irName = d.name;
            globalType_[d.name] = t;
            mod_.globals.push_back({d.name, t});
        } else {
            irName = newTemp();
            localMap_[d.name] = irName;
            localType_[d.name] = t;
            localDecls_.push_back({irName, t});
            Ins all; all.op = Op::Alloca; all.dst = irName; all.type = t; emit(all);
        }
        if (d.init) {
            IROperand v = lowerExpr(d.init.get());
            Ins st; st.op = Op::Store; st.dst = irName; st.a = v; st.type = t; emit(st);
        }
    }
}

void IRBuilder::lowerIf(const IfStmt* s) {
    IROperand cond = lowerExpr(s->cond.get());
    size_t Lelse = newLabel(), Lend = newLabel();
    Ins j; j.op = Op::JmpNZ; j.a = cond; j.label = (s->elseBranch ? Lelse : Lend); emit(j);
    lowerStmt(s->thenBranch.get());
    if (s->elseBranch) {
        Ins jend; jend.op = Op::Jump; jend.label = Lend; emit(jend);
        Ins lel; lel.op = Op::Label; lel.label = Lelse; emit(lel);
        lowerStmt(s->elseBranch.get());
    }
    Ins le; le.op = Op::Label; le.label = Lend; emit(le);
}

void IRBuilder::lowerWhile(const WhileStmt* s) {
    size_t Lstart = newLabel(), Lend = newLabel();
    Ins ls; ls.op = Op::Label; ls.label = Lstart; emit(ls);
    IROperand cond = lowerExpr(s->cond.get());
    Ins j; j.op = Op::JmpZ; j.a = cond; j.label = Lend; emit(j);
    breakStack_.push_back(Lend);
    continueStack_.push_back(Lstart);
    lowerStmt(s->body.get());
    breakStack_.pop_back();
    continueStack_.pop_back();
    Ins jb; jb.op = Op::Jump; jb.label = Lstart; emit(jb);
    Ins le; le.op = Op::Label; le.label = Lend; emit(le);
}

void IRBuilder::lowerDoWhile(const DoWhileStmt* s) {
    size_t Lstart = newLabel(), Lcond = newLabel(), Lend = newLabel();
    Ins ls; ls.op = Op::Label; ls.label = Lstart; emit(ls);
    breakStack_.push_back(Lend);
    continueStack_.push_back(Lcond);
    lowerStmt(s->body.get());
    breakStack_.pop_back();
    continueStack_.pop_back();
    Ins lc; lc.op = Op::Label; lc.label = Lcond; emit(lc);
    IROperand cond = lowerExpr(s->cond.get());
    Ins jnz; jnz.op = Op::JmpNZ; jnz.a = cond; jnz.label = Lstart; emit(jnz);
    Ins le; le.op = Op::Label; le.label = Lend; emit(le);
}

void IRBuilder::lowerFor(const ForStmt* s) {
    if (s->init) lowerStmt(s->init.get());
    size_t Lcond = newLabel(), Lbody = newLabel(), Lend = newLabel();
    Ins lc; lc.op = Op::Label; lc.label = Lcond; emit(lc);
    if (s->cond) {
        IROperand cond = lowerExpr(s->cond.get());
        Ins jz; jz.op = Op::JmpZ; jz.a = cond; jz.label = Lend; emit(jz);
    }
    Ins lb; lb.op = Op::Label; lb.label = Lbody; emit(lb);
    breakStack_.push_back(Lend);
    continueStack_.push_back(Lcond);
    lowerStmt(s->body.get());
    breakStack_.pop_back();
    continueStack_.pop_back();
    if (s->update) lowerExpr(s->update.get());
    Ins jb; jb.op = Op::Jump; jb.label = Lcond; emit(jb);
    Ins le; le.op = Op::Label; le.label = Lend; emit(le);
}

void IRBuilder::lowerForInOf(const ForInOfStmt* s) {
    // Simplified: treat as classic for over array-like values at runtime.
    IROperand it = lowerExpr(s->iterable.get());
    std::string arr = newTemp();
    Ins a1; a1.op = Op::Alloca; a1.dst = arr; a1.type = TypeInfo(T::Any); emit(a1);
    Ins st; st.op = Op::Store; st.dst = arr; st.a = it; st.type = TypeInfo(T::Any); emit(st);

    std::string i = newTemp();
    Ins ia; ia.op = Op::Alloca; ia.dst = i; ia.type = TypeInfo(T::Num); emit(ia);
    Ins is0; is0.op = Op::Store; is0.dst = i; is0.a = constNum(0, TypeInfo(T::Num)); is0.type = TypeInfo(T::Num); emit(is0);

    size_t Lcond = newLabel(), Lbody = newLabel(), Lend = newLabel();
    Ins lc; lc.op = Op::Label; lc.label = Lcond; emit(lc);
    std::string len = newTemp();
    Ins gl; gl.op = Op::CallMember; gl.dst = len; gl.member = "__length"; gl.a = tempOp(arr, TypeInfo(T::Any)); gl.type = TypeInfo(T::Num); emit(gl);
    Ins cmp; cmp.op = Op::Lt; cmp.dst = newTemp(); cmp.a = tempOp(i, TypeInfo(T::Num)); cmp.b = tempOp(len, TypeInfo(T::Num)); cmp.type = TypeInfo(T::Bool); emit(cmp);
    Ins jz; jz.op = Op::JmpZ; jz.a = tempOp(cmp.dst, TypeInfo(T::Bool)); jz.label = Lend; emit(jz);

    Ins lb; lb.op = Op::Label; lb.label = Lbody; emit(lb);
    // item = arr[i]
    std::string item = newTemp();
    Ins gi; gi.op = Op::GetIndex; gi.dst = item; gi.a = tempOp(arr, TypeInfo(T::Any)); gi.b = tempOp(i, TypeInfo(T::Num)); gi.type = TypeInfo(T::Any); emit(gi);
    // declare loop var
    std::string loopVar = newTemp();
    localMap_[s->var] = loopVar;
    localType_[s->var] = TypeInfo(T::Any);
    localDecls_.push_back({loopVar, TypeInfo(T::Any)});
    Ins lv; lv.op = Op::Alloca; lv.dst = loopVar; lv.type = TypeInfo(T::Any); emit(lv);
    Ins stv; stv.op = Op::Store; stv.dst = loopVar; stv.a = tempOp(item, TypeInfo(T::Any)); stv.type = TypeInfo(T::Any); emit(stv);

    breakStack_.push_back(Lend);
    continueStack_.push_back(Lcond);
    lowerStmt(s->body.get());
    breakStack_.pop_back();
    continueStack_.pop_back();

    // i = i + 1
    std::string ni = newTemp();
    Ins add; add.op = Op::Add; add.dst = ni; add.a = tempOp(i, TypeInfo(T::Num)); add.b = constNum(1, TypeInfo(T::Num)); add.type = TypeInfo(T::Num); emit(add);
    Ins sti; sti.op = Op::Store; sti.dst = i; sti.a = tempOp(ni, TypeInfo(T::Num)); sti.type = TypeInfo(T::Num); emit(sti);
    Ins jb; jb.op = Op::Jump; jb.label = Lcond; emit(jb);
    Ins le; le.op = Op::Label; le.label = Lend; emit(le);
}

void IRBuilder::lowerSwitch(const SwitchStmt* s) {
    IROperand subj = lowerExpr(s->subject.get());
    size_t Lend = newLabel();
    size_t Ldefault = Lend;
    for (auto& c : s->cases) if (c.isDefault) { Ldefault = newLabel(); break; }

    std::vector<std::pair<size_t, IROperand>> caseTests;
    for (auto& c : s->cases) {
        if (c.isDefault) { caseTests.push_back({Ldefault, IROperand{}}); continue; }
        size_t Lcase = newLabel();
        IROperand ct = lowerExpr(c.test.get());
        std::string eq = newTemp();
        Ins e; e.op = Op::StrictEq; e.dst = eq; e.a = subj; e.b = ct; e.type = TypeInfo(T::Bool); emit(e);
        Ins jnz; jnz.op = Op::JmpNZ; jnz.a = tempOp(eq, TypeInfo(T::Bool)); jnz.label = Lcase; emit(jnz);
        caseTests.push_back({Lcase, IROperand{}});
    }
    Ins jdef; jdef.op = Op::Jump; jdef.label = Ldefault; emit(jdef);

    for (auto& c : s->cases) {
        size_t L = c.isDefault ? Ldefault : newLabel();
        Ins l; l.op = Op::Label; l.label = L; emit(l);
        for (auto& st : c.body) lowerStmt(st.get());
    }
    Ins le; le.op = Op::Label; le.label = Lend; emit(le);
}

void IRBuilder::lowerReturn(const ReturnStmt* s) {
    Ins in; in.op = Op::Return;
    if (fn_->isMain) {
        in.a = constNum(0, TypeInfo(T::Any));
        emit(in);
        return;
    }
    if (s->hasValue && s->value) {
        in.a = lowerExpr(s->value.get());
    }
    emit(in);
}

void IRBuilder::lowerThrow(const ThrowStmt* s) {
    IROperand v = lowerExpr(s->value.get());
    Ins in; in.op = Op::Throw; in.a = v; emit(in);
}

// ---------------------------------------------------------------- Functions & classes
bool IRBuilder::declaredFn(const std::string& n) const {
    for (const auto& x : fnNames_) if (x == n) return true;
    return false;
}

TypeInfo IRBuilder::scanRetType(const BlockStmt* body) {
    TypeInfo ret(T::Void);
    for (const auto& st : body->body) {
        if (st->kind == StmtKind::Return) {
            const auto* r = static_cast<const ReturnStmt*>(st.get());
            if (r->hasValue && r->value) return infer(r->value.get());
        } else if (st->kind == StmtKind::Block) {
            TypeInfo t = scanRetType(static_cast<const BlockStmt*>(st.get()));
            if (t.kind != T::Void) ret = t;
        }
    }
    return ret;
}

void IRBuilder::enterFn(IRFn& f, const std::vector<Param>& params) {
    fn_ = &f;
    localMap_.clear();
    localType_.clear();
    localDecls_.clear();
    temp_ = 0;
    label_ = 0;
    for (const auto& p : params) {
        std::string n = newTemp();
        localMap_[p.name] = n;
        localType_[p.name] = p.def ? infer(p.def.get()) : TypeInfo(T::Any);
        localDecls_.push_back({n, localType_[p.name]});
    }
    if (f.isMethod || f.isCtor) {
        localType_["this"] = TypeInfo(T::Obj);
    }
}

void IRBuilder::leaveFn(IRFn* saved, std::map<std::string, std::string> savedMap,
                        std::map<std::string, TypeInfo> savedType,
                        std::vector<std::pair<std::string, TypeInfo>> savedDecls,
                        int savedTemp, int savedLabel) {
    fn_ = saved;
    localMap_ = std::move(savedMap);
    localType_ = std::move(savedType);
    localDecls_ = std::move(savedDecls);
    temp_ = savedTemp;
    label_ = savedLabel;
}

IRFn IRBuilder::makeFunction(const std::string& name, const std::vector<Param>& params,
                             const BlockStmt* body, bool async, bool isMain) {
    IRFn* savedFn = fn_;
    auto savedMap = localMap_;
    auto savedType = localType_;
    auto savedDecls = localDecls_;
    int savedTemp = temp_, savedLabel = label_;

    IRFn f;
    f.name = name;
    f.isMain = isMain;
    if (isMain) f.name = "main";
    (void)async;

    enterFn(f, params);
    f.retType = isMain ? TypeInfo(T::Any) : scanRetType(body);

    for (auto& p : params) {
        IROperand op;
        op.name = localMap_[p.name];
        op.type = localType_[p.name];
        f.params.push_back(op);
    }

    lowerBlock(body);

    bool hasReturn = false;
    for (const auto& in : f.code) if (in.op == Op::Return) { hasReturn = true; break; }
    if (!hasReturn) {
        Ins r; r.op = Op::Return;
        if (!isMain && f.retType.kind != T::Void) r.a = constNum(0, TypeInfo(T::Null));
        emit(r);
    }

    f.locals = std::move(localDecls_);
    leaveFn(savedFn, std::move(savedMap), std::move(savedType), std::move(savedDecls), savedTemp, savedLabel);
    return f;
}

void IRBuilder::buildClass(const ClassDef* c) {
    IRClass cls;
    cls.name = c->name;
    if (c->base) cls.base = *c->base;
    classNames_.push_back(c->name);

    for (const auto& f : c->fields) {
        IRField fld;
        fld.name = f.name;
        fld.isStatic = f.isStatic;
        fld.type = f.init ? infer(f.init.get()) : TypeInfo(T::Any);
        fld.hasInit = f.init != nullptr;
        if (f.init && f.init->kind == ExprKind::Number) fld.initNum = static_cast<const NumberExpr*>(f.init.get())->value;
        if (f.init && f.init->kind == ExprKind::String) fld.initStr = static_cast<const StringExpr*>(f.init.get())->value;
        cls.fields.push_back(std::move(fld));
    }

    for (const auto& m : c->methods) {
        IRFn mf;
        mf.name = m.isCtor ? "__ctor__" : m.name;
        mf.isMethod = true;
        mf.isCtor = m.isCtor;
        mf.isStatic = m.isStatic;
        mf.className = c->name;
        mf.isGetter = m.isGetter;
        mf.isSetter = m.isSetter;

        IRFn* savedFn = fn_;
        auto savedMap = localMap_;
        auto savedType = localType_;
        auto savedDecls = localDecls_;
        int savedTemp = temp_, savedLabel = label_;

        enterFn(mf, m.params);
        mf.retType = m.isCtor ? TypeInfo(T::Void) : scanRetType(m.body.get());
        for (auto& p : m.params) {
            IROperand op;
            op.name = localMap_[p.name];
            op.type = localType_[p.name];
            mf.params.push_back(op);
        }
        lowerBlock(m.body.get());

        bool hasReturn = false;
        for (const auto& in : mf.code) if (in.op == Op::Return) { hasReturn = true; break; }
        if (!hasReturn) {
            Ins r; r.op = Op::Return; r.a = constNum(0, TypeInfo(T::Void));
            emit(r);
        }
        mf.locals = std::move(localDecls_);
        leaveFn(savedFn, std::move(savedMap), std::move(savedType), std::move(savedDecls), savedTemp, savedLabel);

        cls.methods.push_back(std::move(mf));
        if (m.isCtor) cls.hasCtor = true;
    }
    mod_.classes.push_back(std::move(cls));
}

IRModule IRBuilder::build() {
    // Pre-scan top-level function/class names so calls can be resolved.
    for (const auto& st : prog_->body) {
        if (st->kind == StmtKind::Function) {
            const auto* fd = static_cast<const FunctionDecl*>(st.get());
            fnNames_.push_back(fd->name);
            fnRetType_[fd->name] = scanRetType(fd->body.get());
        } else if (st->kind == StmtKind::ClassDef) {
            classNames_.push_back(static_cast<const ClassDef*>(st.get())->name);
        }
    }

    fn_ = &mod_.main;
    mod_.main.name = "main";
    mod_.main.isMain = true;
    localDecls_.clear();
    localMap_.clear();
    localType_.clear();
    topLevel_ = 0;
    temp_ = 0;
    label_ = 0;

    for (auto& st : prog_->body) {
        if (st->kind == StmtKind::Function || st->kind == StmtKind::ClassDef) {
            // hoisted: declared globally
            if (st->kind == StmtKind::Function) {
                const auto* fd = static_cast<const FunctionDecl*>(st.get());
                IRFn f = makeFunction(fd->name, fd->params, fd->body.get(), fd->async);
                mod_.functions.push_back(std::move(f));
            } else {
                buildClass(static_cast<const ClassDef*>(st.get()));
            }
            continue;
        }
        if (st->kind == StmtKind::VarDecl) {
            // top-level var -> global symbol registered in lowerVar
        }
        lowerStmt(st.get());
    }

    // main implicit return
    bool hasReturn = false;
    for (const auto& in : mod_.main.code) if (in.op == Op::Return) { hasReturn = true; break; }
    if (!hasReturn) {
        Ins r; r.op = Op::Return; r.a = constNum(0, TypeInfo(T::Any));
        emit(r);
    }
    mod_.main.locals = std::move(localDecls_);

    fn_ = nullptr;
    return std::move(mod_);
}

} // namespace thz