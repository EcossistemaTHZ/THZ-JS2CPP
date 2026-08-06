#include "thz/parser.hpp"
#include "thz/lexer.hpp"

#include <cmath>

namespace thz {

Parser::Parser(std::vector<Token> tokens) : toks_(std::move(tokens)) {}

void Parser::mark(Expr& e) {
    e.line = cur().line;
    e.col = cur().col;
}

void Parser::error(const std::string& msg) const {
    const auto& t = cur();
    throw ParseError(msg + " (linha " + std::to_string(t.line) + ", col " + std::to_string(t.col) + ")");
}

bool Parser::match(Tok k) {
    if (check(k)) { advance(); return true; }
    return false;
}

bool Parser::matchAny(std::initializer_list<Tok> ks) {
    for (Tok k : ks) if (check(k)) { advance(); return true; }
    return false;
}

const Token& Parser::expect(Tok k, const std::string& what) {
    if (!check(k)) error("esperado '" + what + "', mas encontrado '" + tokName(cur().kind) + "'");
    return advance();
}

// ---------------------------------------------------------------- Statements

StmtPtr Parser::parseStatement() {
    switch (cur().kind) {
    case Tok::LBrace:   return parseBlock();
    case Tok::Let:
    case Tok::Const:
    case Tok::Var:      return parseVarStmt(advance().kind);
    case Tok::If:       return parseIf();
    case Tok::While:    return parseWhile();
    case Tok::Do:       return parseDoWhile();
    case Tok::For:      return parseFor();
    case Tok::Switch:   return parseSwitch();
    case Tok::Function: return parseFunction(false, "");
    case Tok::Class:    return parseClass();
    case Tok::Return:   return parseReturn();
    case Tok::Break:    advance(); expect(Tok::Semicolon, ";"); return std::make_unique<BreakStmt>();
    case Tok::Continue: advance(); expect(Tok::Semicolon, ";"); return std::make_unique<ContinueStmt>();
    case Tok::Throw:    {
        advance();
        auto s = std::make_unique<ThrowStmt>(parseExpression());
        match(Tok::Semicolon);
        return s;
    }
    case Tok::Try:      return parseTry();
    case Tok::Semicolon: advance(); return parseStatement(); // skip empty
    default: {
        auto e = parseExpression();
        match(Tok::Semicolon);
        auto s = std::make_unique<ExprStmt>(std::move(e));
        return s;
    }
    }
}

StmtPtr Parser::parseBlock() {
    auto b = std::make_unique<BlockStmt>();
    expect(Tok::LBrace, "{");
    while (!check(Tok::RBrace) && !atEnd()) {
        b->body.push_back(parseStatement());
    }
    expect(Tok::RBrace, "}");
    return b;
}

StmtPtr Parser::parseVarStmt(Tok kind) {
    auto s = std::make_unique<VarStmt>();
    s->kind = kind;
    do {
        const auto& nameTok = expect(Tok::Ident, "identificador");
        VarDecl d;
        d.name = nameTok.text;
        d.isConst = (kind == Tok::Const);
        if (match(Tok::Assign)) d.init = parseAssignment();
        s->decls.push_back(std::move(d));
    } while (match(Tok::Comma));
    match(Tok::Semicolon);
    return s;
}

StmtPtr Parser::parseIf() {
    advance(); // if
    auto s = std::make_unique<IfStmt>();
    expect(Tok::LParen, "(");
    s->cond = parseExpression();
    expect(Tok::RParen, ")");
    s->thenBranch = parseStatement();
    if (match(Tok::Else)) s->elseBranch = parseStatement();
    return s;
}

StmtPtr Parser::parseWhile() {
    advance();
    auto s = std::make_unique<WhileStmt>();
    expect(Tok::LParen, "(");
    s->cond = parseExpression();
    expect(Tok::RParen, ")");
    s->body = parseStatement();
    return s;
}

StmtPtr Parser::parseDoWhile() {
    advance();
    auto s = std::make_unique<DoWhileStmt>();
    s->body = parseStatement();
    expect(Tok::While, "while");
    expect(Tok::LParen, "(");
    s->cond = parseExpression();
    expect(Tok::RParen, ")");
    match(Tok::Semicolon);
    return s;
}

StmtPtr Parser::parseFor() {
    advance(); // for
    expect(Tok::LParen, "(");
    auto s = std::make_unique<ForStmt>();

    // init
    if (check(Tok::Semicolon)) {
        advance();
    } else if (cur().isOneOf({Tok::Let, Tok::Const, Tok::Var})) {
        Tok kind = advance().kind;
        auto tmp = std::make_unique<VarStmt>();
        tmp->kind = kind;
        const auto& nt = expect(Tok::Ident, "identificador");
        VarDecl d; d.name = nt.text; d.isConst = (kind == Tok::Const);
        tmp->decls.push_back(std::move(d));
        // for-in/of with single var
        if (check(Tok::In)) {
            advance();
            auto sio = std::make_unique<ForInOfStmt>();
            sio->isOf = false;
            sio->var = nt.text;
            sio->iterable = parseExpression();
            expect(Tok::RParen, ")");
            sio->body = parseStatement();
            return sio;
        }
        if (match(Tok::Of)) {
            auto sio = std::make_unique<ForInOfStmt>();
            sio->isOf = true;
            sio->var = nt.text;
            sio->iterable = parseExpression();
            expect(Tok::RParen, ")");
            sio->body = parseStatement();
            return sio;
        }
        if (match(Tok::Assign)) {
            noIn_ = true;
            tmp->decls[0].init = parseAssignment();
            noIn_ = false;
        }
        while (match(Tok::Comma)) {
            VarDecl dd; dd.name = expect(Tok::Ident, "identificador").text;
            if (match(Tok::Assign)) { noIn_ = true; dd.init = parseAssignment(); noIn_ = false; }
            tmp->decls.push_back(std::move(dd));
        }
        expect(Tok::Semicolon, ";");
        s->init = std::move(tmp);
    } else {
        noIn_ = true;
        auto e = parseExpression();
        noIn_ = false;
        if (check(Tok::In)) {
            // `for (x in obj)` where x is an identifier expression
            if (e->kind == ExprKind::Ident) {
                advance();
                auto sio = std::make_unique<ForInOfStmt>();
                sio->isOf = false;
                sio->var = static_cast<IdentExpr*>(e.get())->name;
                sio->iterable = parseExpression();
                expect(Tok::RParen, ")");
                sio->body = parseStatement();
                return sio;
            }
        } else if (match(Tok::Of)) {
            if (e->kind == ExprKind::Ident) {
                auto sio = std::make_unique<ForInOfStmt>();
                sio->isOf = true;
                sio->var = static_cast<IdentExpr*>(e.get())->name;
                sio->iterable = parseExpression();
                expect(Tok::RParen, ")");
                sio->body = parseStatement();
                return sio;
            }
        }
        auto es = std::make_unique<ExprStmt>(std::move(e));
        expect(Tok::Semicolon, ";");
        s->init = std::move(es);
    }

    if (!check(Tok::RParen)) s->cond = parseExpression();
    expect(Tok::Semicolon, ";");
    if (!check(Tok::RParen)) s->update = parseExpression();
    expect(Tok::RParen, ")");
    s->body = parseStatement();
    return s;
}

StmtPtr Parser::parseSwitch() {
    advance();
    auto s = std::make_unique<SwitchStmt>();
    expect(Tok::LParen, "(");
    s->subject = parseExpression();
    expect(Tok::RParen, ")");
    expect(Tok::LBrace, "{");
    while (!check(Tok::RBrace) && !atEnd()) {
        SwitchCase c;
        if (match(Tok::Case)) {
            c.isDefault = false;
            c.test = parseExpression();
            expect(Tok::Colon, ":");
        } else if (match(Tok::Default)) {
            c.isDefault = true;
            expect(Tok::Colon, ":");
        } else {
            // continuation of previous case
            c = std::move(s->cases.back());
            s->cases.pop_back();
            while (!check(Tok::Case) && !check(Tok::Default) && !check(Tok::RBrace) && !atEnd()) {
                c.body.push_back(parseStatement());
            }
            s->cases.push_back(std::move(c));
            continue;
        }
        while (!check(Tok::Case) && !check(Tok::Default) && !check(Tok::RBrace) && !atEnd()) {
            c.body.push_back(parseStatement());
        }
        s->cases.push_back(std::move(c));
    }
    expect(Tok::RBrace, "}");
    return s;
}

std::unique_ptr<BlockStmt> Parser::parseFunctionBody() {
    auto body = std::make_unique<BlockStmt>();
    expect(Tok::LBrace, "{");
    while (!check(Tok::RBrace) && !atEnd()) {
        body->body.push_back(parseStatement());
    }
    expect(Tok::RBrace, "}");
    return body;
}

std::vector<Param> Parser::parseParams() {
    std::vector<Param> params;
    expect(Tok::LParen, "(");
    while (!check(Tok::RParen) && !atEnd()) {
        Param p;
        if (match(Tok::Ellipsis)) p.rest = true;
        p.name = expect(Tok::Ident, "identificador").text;
        if (match(Tok::Assign)) p.def = parseAssignment();
        params.push_back(std::move(p));
        if (!match(Tok::Comma)) break;
    }
    expect(Tok::RParen, ")");
    return params;
}

StmtPtr Parser::parseFunction(bool /*isExpr*/, const std::string& /*name*/) {
    advance(); // function
    // async handled by caller
    auto decl = std::make_unique<FunctionDecl>();
    if (match(Tok::Ident)) decl->name = toks_[pos_ - 1].text;
    decl->params = parseParams();
    decl->body = parseFunctionBody();
    return decl;
}

StmtPtr Parser::parseClass() {
    advance(); // class
    auto c = std::make_unique<ClassDef>();
    c->name = expect(Tok::Ident, "nome da classe").text;
    if (match(Tok::Extends)) {
        ExprPtr baseExpr = parseExpression();
        if (baseExpr && baseExpr->kind == ExprKind::Ident) {
            c->base = static_cast<IdentExpr*>(baseExpr.get())->name;
        }
    }
    expect(Tok::LBrace, "{");
    while (!check(Tok::RBrace) && !atEnd()) {
        bool isStatic = false, isGet = false, isSet = false, isAsync = false;
        if (match(Tok::Static)) isStatic = true;
        if (match(Tok::Async)) isAsync = true;
        if (match(Tok::Get)) isGet = true;
        if (match(Tok::Set)) isSet = true;

        if (check(Tok::Ident) || check(Tok::String) || check(Tok::Number)) {
            const auto& nameTok = *(&cur());
            std::string name = nameTok.text;
            advance();
            if (name == "constructor") {
                ClassMethod m;
                m.isCtor = true; m.isStatic = false; m.name = name;
                m.params = parseParams();
                m.body = parseFunctionBody();
                c->methods.push_back(std::move(m));
                continue;
            }
            if (check(Tok::LParen)) {
                ClassMethod m;
                m.name = name;
                m.isStatic = isStatic;
                m.isGetter = isGet;
                m.isSetter = isSet;
                m.async = isAsync;
                m.params = parseParams();
                m.body = parseFunctionBody();
                c->methods.push_back(std::move(m));
            } else {
                ClassField f;
                f.name = name;
                f.isStatic = isStatic;
                if (match(Tok::Assign)) f.init = parseAssignment();
                c->fields.push_back(std::move(f));
            }
        } else if (check(Tok::LBracket)) {
            error("nomes computados de métodos ainda não suportados");
        } else {
            error("esperado membro de classe");
        }
        match(Tok::Semicolon);
    }
    expect(Tok::RBrace, "}");
    return c;
}

StmtPtr Parser::parseReturn() {
    advance(); // return
    auto s = std::make_unique<ReturnStmt>();
    if (!check(Tok::Semicolon) && !check(Tok::RBrace) && !atEnd()) {
        s->value = parseExpression();
        s->hasValue = true;
    }
    match(Tok::Semicolon);
    return s;
}

StmtPtr Parser::parseTry() {
    advance(); // try
    auto s = std::make_unique<TryStmt>();
    s->tryBlock = parseBlockAsBlock();
    if (match(Tok::Catch)) {
        expect(Tok::LParen, "(");
        s->catchParam = expect(Tok::Ident, "identificador").text;
        expect(Tok::RParen, ")");
        s->catchBlock = parseBlockAsBlock();
    }
    if (match(Tok::Finally)) {
        s->finallyBlock = parseBlockAsBlock();
    }
    if (!s->catchBlock && !s->finallyBlock) {
        error("try deve ter catch ou finally");
    }
    return s;
}

std::unique_ptr<BlockStmt> Parser::parseBlockAsBlock() {
    expect(Tok::LBrace, "{");
    auto b = std::make_unique<BlockStmt>();
    while (!check(Tok::RBrace) && !atEnd()) b->body.push_back(parseStatement());
    expect(Tok::RBrace, "}");
    return b;
}

std::unique_ptr<Program> Parser::parseProgram() {
    auto prog = std::make_unique<Program>();
    while (!atEnd()) prog->body.push_back(parseStatement());
    return prog;
}

// ---------------------------------------------------------------- Expressions

int Parser::binPrec(Tok t) {
    switch (t) {
    case Tok::Nullish:       return 1;
    case Tok::OrOr:          return 2;
    case Tok::AndAnd:        return 3;
    case Tok::BitOr:         return 4;
    case Tok::BitXor:        return 5;
    case Tok::BitAnd:        return 6;
    case Tok::Eq: case Tok::Neq: case Tok::StrictEq: case Tok::StrictNeq: return 7;
    case Tok::Lt: case Tok::Gt: case Tok::Le: case Tok::Ge:
    case Tok::In: case Tok::Instanceof:               return 8;
    // >>> treated as >>
    case Tok::Shl: case Tok::Shr:                     return 9;
    case Tok::Plus: case Tok::Minus:                  return 10;
    case Tok::Star: case Tok::Slash: case Tok::Percent: return 11;
    default: return 0;
    }
}

BinOp Parser::binOp(Tok t) {
    switch (t) {
    case Tok::Plus:   return BinOp::Add;
    case Tok::Minus:  return BinOp::Sub;
    case Tok::Star:   return BinOp::Mul;
    case Tok::Slash:  return BinOp::Div;
    case Tok::Percent:return BinOp::Mod;
    case Tok::Eq:     return BinOp::Eq;
    case Tok::Neq:    return BinOp::Neq;
    case Tok::StrictEq:   return BinOp::StrictEq;
    case Tok::StrictNeq:  return BinOp::StrictNeq;
    case Tok::Lt:     return BinOp::Lt;
    case Tok::Gt:     return BinOp::Gt;
    case Tok::Le:     return BinOp::Le;
    case Tok::Ge:     return BinOp::Ge;
    case Tok::BitAnd: return BinOp::BitAnd;
    case Tok::BitOr:  return BinOp::BitOr;
    case Tok::BitXor: return BinOp::BitXor;
    case Tok::Shl:    return BinOp::Shl;
    case Tok::Shr:    return BinOp::Shr;
    case Tok::AndAnd: return BinOp::LogicalAnd;
    case Tok::OrOr:   return BinOp::LogicalOr;
    case Tok::Nullish:return BinOp::LogicalNullish;
    case Tok::In:     return BinOp::In;
    case Tok::Instanceof: return BinOp::Instanceof;
    default: return BinOp::Add;
    }
}

bool Parser::isAssignOp(Tok t) {
    switch (t) {
    case Tok::Assign: case Tok::PlusAssign: case Tok::MinusAssign:
    case Tok::StarAssign: case Tok::SlashAssign: case Tok::PercentAssign:
    case Tok::ShlAssign: case Tok::ShrAssign: case Tok::BitAndAssign:
    case Tok::BitOrAssign: case Tok::BitXorAssign: case Tok::NullishAssign:
        return true;
    default: return false;
    }
}

ExprPtr Parser::parseExpression() {
    ExprPtr left = parseAssignment();
    if (match(Tok::Comma)) {
        auto seq = std::make_unique<SequenceExpr>();
        seq->exprs.push_back(std::move(left));
        do {
            seq->exprs.push_back(parseAssignment());
        } while (match(Tok::Comma));
        mark(*seq);
        return seq;
    }
    return left;
}

ExprPtr Parser::parseAssignment() {
    ExprPtr left = parseTernary();
    if (isAssignOp(cur().kind)) {
        Tok op = advance().kind;
        ExprPtr right = parseAssignment();
        auto a = std::make_unique<AssignExpr>(op, std::move(left), std::move(right));
        mark(*a);
        return a;
    }
    return left;
}

ExprPtr Parser::parseTernary() {
    ExprPtr cond = parseBinary(0);
    if (match(Tok::Question)) {
        ExprPtr thenB = parseExpression();
        expect(Tok::Colon, ":");
        ExprPtr elseB = parseTernary();
        auto c = std::make_unique<CondExpr>(std::move(cond), std::move(thenB), std::move(elseB));
        mark(*c);
        return c;
    }
    return cond;
}

ExprPtr Parser::parseBinary(int minPrec) {
    ExprPtr left = parseUnary();
    for (;;) {
        Tok t = cur().kind;
        int prec = binPrec(t);
        if (t == Tok::In && noIn_) prec = 0;
        if (prec == 0 || prec < minPrec) break;
        advance(); // operator
        ExprPtr right = parseBinary(prec + 1);
        auto b = std::make_unique<BinaryExpr>(binOp(t), std::move(left), std::move(right));
        mark(*b);
        left = std::move(b);
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    switch (cur().kind) {
    case Tok::Bang:
    case Tok::Minus: case Tok::Plus: case Tok::Tilde:
    case Tok::Typeof: case Tok::Void: case Tok::Delete:
    case Tok::PlusPlus: case Tok::MinusMinus:
    case Tok::Await: {
        Tok op = advance().kind;
        ExprPtr opd = parseUnary();
        auto u = std::make_unique<UnaryExpr>(op, std::move(opd));
        mark(*u);
        return u;
    }
    case Tok::New: {
        advance();
        ExprPtr callee = parseCallMember();
        auto call = std::make_unique<CallExpr>();
        call->callee = std::move(callee);
        call->isNew = true;
        if (check(Tok::LParen)) {
            expect(Tok::LParen, "(");
            while (!check(Tok::RParen) && !atEnd()) {
                if (match(Tok::Ellipsis)) error("spread em call não suportado");
                call->args.push_back(parseAssignment());
                if (!match(Tok::Comma)) break;
            }
            expect(Tok::RParen, ")");
        }
        mark(*call);
        return call;
    }
    default:
        break;
    }
    // async arrow
    if (check(Tok::Async)) {
        Tok nxt = peekAhead(1).kind;
        if (nxt == Tok::Ident || nxt == Tok::LParen) {
            // attempt arrow: save pos
            size_t save = pos_;
            advance(); // async
            ExprPtr arrow = tryParseArrowAfterAsync();
            if (arrow) return arrow;
            pos_ = save;
        }
    }
    return parsePostfix();
}

ExprPtr Parser::tryParseArrowAfterAsync() {
    // cur is Ident or LParen
    if (check(Tok::Ident)) {
        std::vector<Param> params;
        Param p; p.name = advance().text;
        params.push_back(std::move(p));
        if (!match(Tok::Arrow)) return nullptr;
        auto a = std::make_unique<ArrowExpr>();
        a->async = true;
        a->params = std::move(params);
        a->body = parseArrowBody();
        mark(*a);
        return a;
    }
    if (check(Tok::LParen)) {
        // reuse paren scan
        ExprPtr inner = parseParenOrArrow();
        if (inner && inner->kind == ExprKind::ArrowExpr) {
            static_cast<ArrowExpr*>(inner.get())->async = true;
        }
        return inner;
    }
    return nullptr;
}

std::unique_ptr<BlockStmt> Parser::parseArrowBody() {
    if (match(Tok::LBrace)) {
        auto b = std::make_unique<BlockStmt>();
        while (!check(Tok::RBrace) && !atEnd()) b->body.push_back(parseStatement());
        expect(Tok::RBrace, "}");
        return b;
    }
    auto es = std::make_unique<BlockStmt>();
    auto ret = std::make_unique<ReturnStmt>();
    ret->value = parseAssignment();
    ret->hasValue = true;
    es->body.push_back(std::move(ret));
    return es;
}

ExprPtr Parser::parsePostfix() {
    ExprPtr expr = parseCallMember();
    if (check(Tok::PlusPlus) || check(Tok::MinusMinus)) {
        Tok op = advance().kind;
        auto u = std::make_unique<UnaryExpr>(op, std::move(expr), false);
        mark(*u);
        return u;
    }
    return expr;
}

ExprPtr Parser::parseCallMember() {
    ExprPtr expr = parsePrimary();
    for (;;) {
        if (check(Tok::Dot) || check(Tok::QuestionDot)) {
            bool optional = false;
            if (check(Tok::QuestionDot)) { optional = true; advance(); }
            expect(Tok::Dot, ".");
            auto m = std::make_unique<MemberExpr>();
            m->optional = optional;
            m->object = std::move(expr);
            const auto& nameTok = expect(Tok::Ident, "propriedade");
            m->name = nameTok.text;
            mark(*m);
            expr = std::move(m);
        } else if (match(Tok::LBracket)) {
            auto m = std::make_unique<MemberExpr>();
            m->object = std::move(expr);
            m->index = parseExpression();
            expect(Tok::RBracket, "]");
            mark(*m);
            expr = std::move(m);
        } else if (check(Tok::LParen)) {
            ExprPtr callee = std::move(expr);
            auto call = std::make_unique<CallExpr>();
            call->callee = std::move(callee);
            expect(Tok::LParen, "(");
            while (!check(Tok::RParen) && !atEnd()) {
                if (match(Tok::Ellipsis)) error("spread em args não suportado");
                call->args.push_back(parseAssignment());
                if (!match(Tok::Comma)) break;
            }
            expect(Tok::RParen, ")");
            mark(*call);
            expr = std::move(call);
        } else {
            break;
        }
    }
    return expr;
}

ExprPtr Parser::parsePrimary() {
    const Token& t = cur();
    switch (t.kind) {
    case Tok::Number: {
        advance();
        auto e = std::make_unique<NumberExpr>(std::get<double>(t.value));
        mark(*e); return e;
    }
    case Tok::String: {
        advance();
        auto e = std::make_unique<StringExpr>(std::get<std::string>(t.value));
        mark(*e); return e;
    }
    case Tok::True:  { advance(); auto tb = std::make_unique<BoolExpr>(true);  mark(*tb); return tb; }
    case Tok::False: { advance(); auto fb = std::make_unique<BoolExpr>(false); mark(*fb); return fb; }
    case Tok::Null:  { advance(); auto n  = std::make_unique<NullExpr>();      mark(*n);  return n; }
    case Tok::Undefined: { advance(); auto u = std::make_unique<UndefinedExpr>(); mark(*u); return u; }
    case Tok::This:  { advance(); auto th = std::make_unique<ThisExpr>();      mark(*th); return th; }
    case Tok::Super: { advance(); auto sp = std::make_unique<SuperExpr>();     mark(*sp); return sp; }
    case Tok::Ident: {
        if (peekAhead(1).kind == Tok::Arrow) {
            advance(); // id
            auto a = std::make_unique<ArrowExpr>();
            Param p; p.name = t.text;
            a->params.push_back(std::move(p));
            advance(); // =>
            a->body = parseArrowBody();
            mark(*a); return a;
        }
        advance();
        auto e = std::make_unique<IdentExpr>(t.text);
        mark(*e); return e;
    }
    case Tok::Template: {
        advance();
        return parseTemplateFromValue(t);
    }
    case Tok::LBracket: return parseArrayLit();
    case Tok::LBrace:   return parseObjectLit();
    case Tok::LParen:   return parseParenOrArrow();
    case Tok::Function: {
        advance();
        if (match(Tok::Ident)) { /* skip name */ }
        auto a = std::make_unique<ArrowExpr>();
        a->params = parseParams();
        a->body = parseFunctionBody();
        mark(*a); return a;
    }
    default:
        error("expressão inesperada: '" + std::string(tokName(t.kind)) + "'");
    }
}

ExprPtr Parser::parseArrayLit() {
    advance(); // [
    auto a = std::make_unique<ArrayExpr>();
    while (!check(Tok::RBracket) && !atEnd()) {
        if (match(Tok::Comma)) {
            a->items.push_back(nullptr);
            a->isSpread.push_back(false);
            continue;
        }
        bool spread = false;
        if (match(Tok::Ellipsis)) spread = true;
        auto e = parseAssignment();
        a->items.push_back(std::move(e));
        a->isSpread.push_back(spread);
        if (!match(Tok::Comma)) break;
    }
    if (check(Tok::Comma)) { advance(); a->items.push_back(nullptr); a->isSpread.push_back(false); }
    expect(Tok::RBracket, "]");
    mark(*a);
    return a;
}

ExprPtr Parser::parseObjectLit() {
    advance(); // {
    auto o = std::make_unique<ObjectExpr>();
    while (!check(Tok::RBrace) && !atEnd()) {
        ObjectProp prop;
        if (match(Tok::Ellipsis)) {
            prop.isSpread = true;
            prop.spread = parseAssignment();
            o->props.push_back(std::move(prop));
            if (!match(Tok::Comma)) break;
            continue;
        }
        if (match(Tok::LBracket)) {
            prop.isComputed = true;
            prop.computedKey = parseExpression();
            expect(Tok::RBracket, "]");
        } else if (cur().isOneOf({Tok::Ident, Tok::String, Tok::Number})) {
            const Token& k = advance();
            prop.key = k.text;
        } else {
            error("chave de propriedade esperada");
        }
        if (match(Tok::Colon)) {
            prop.value = parseAssignment();
        } else {
            // shorthand property
            auto id = std::make_unique<IdentExpr>(prop.key);
            prop.value = std::move(id);
        }
        o->props.push_back(std::move(prop));
        if (!match(Tok::Comma)) break;
    }
    expect(Tok::RBrace, "}");
    mark(*o);
    return o;
}

ExprPtr Parser::parseParenOrArrow() {
    // Look ahead to matching ')' followed by '=>'
    size_t save = pos_;
    expect(Tok::LParen, "(");
    size_t depth = 1;
    bool isArrow = false;
    while (!atEnd() && depth > 0) {
        Tok k = cur().kind;
        if (k == Tok::LParen) ++depth;
        else if (k == Tok::RParen) {
            --depth;
            if (depth == 0) {
                if (peekAhead(1).kind == Tok::Arrow) isArrow = true;
                break;
            }
        }
        advance();
    }
    pos_ = save;
    if (isArrow) {
        std::vector<Param> params = parseParams();
        expect(Tok::Arrow, "=>");
        auto a = std::make_unique<ArrowExpr>();
        a->params = std::move(params);
        a->body = parseArrowBody();
        mark(*a);
        return a;
    }
    expect(Tok::LParen, "(");
    ExprPtr inner = parseExpression();
    expect(Tok::RParen, ")");
    return inner;
}

ExprPtr Parser::parseTemplateFromValue(const Token& t) {
    auto raw = std::get<std::string>(t.value); // inner text
    auto te = std::make_unique<TemplateExpr>();
    size_t i = 0;
    std::string curChunk;
    while (i < raw.size()) {
        if (raw[i] == '$' && i + 1 < raw.size() && raw[i + 1] == '{') {
            te->chunks.push_back(curChunk); curChunk.clear();
            size_t j = i + 2;
            int depth = 1;
            std::string expr;
            while (j < raw.size() && depth > 0) {
                if (raw[j] == '{') ++depth;
                else if (raw[j] == '}') { --depth; if (depth == 0) break; }
                expr += raw[j];
                ++j;
            }
            Parser sub(Lexer(expr).tokenize());
            te->substitutions.push_back(sub.parseExpression());
            i = j + 1;
        } else {
            curChunk += raw[i];
            ++i;
        }
    }
    te->chunks.push_back(curChunk);
    mark(*te);
    return te;
}

} // namespace thz