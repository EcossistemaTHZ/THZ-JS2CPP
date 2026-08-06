#pragma once

#include <string>
#include <variant>
#include <cstdint>

namespace thz {

enum class Tok {
    End,                  // EOF

    // Literals
    Ident,                // identifier
    Number,               // numeric literal
    String,               // string literal
    Template,             // template literal (raw)

    // Keywords
    Let, Const, Var,
    Function, Return,
    If, Else, While, Do, For,
    Break, Continue,
    Switch, Case, Default,
    Class, New, Extends, Super,
    This, Static, Get, Set,
    Typeof, Instanceof, In, Of, Delete, Void,
    Async, Await,
    Try, Catch, Finally, Throw,
    True, False, Null, Undefined,

    // Punctuation / operators
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Comma, Semicolon, Colon, Dot, Question, Ellipsis,
    Arrow,                       // =>
    Plus, Minus, Star, Slash, Percent,
    PlusPlus, MinusMinus,
    Assign,                      // =
    PlusAssign, MinusAssign, StarAssign, SlashAssign, PercentAssign,
    Eq, Neq, StrictEq, StrictNeq,
    Lt, Gt, Le, Ge,
    AndAnd, OrOr, Bang, BitAnd, BitOr, BitXor, Tilde, Shl, Shr,
    ShlAssign, ShrAssign, BitAndAssign, BitOrAssign, BitXorAssign,
    QuestionDot, Nullish, NullishAssign,   // ??  ??=
    Backtick,
};

struct Token {
    Tok kind = Tok::End;
    std::string text;        // raw slice of source
    std::variant<std::monostate, double, std::string> value;
    std::size_t line = 0;
    std::size_t col = 0;

    bool is(Tok k) const { return kind == k; }
    bool isOneOf(std::initializer_list<Tok> ks) const {
        for (Tok k : ks) if (kind == k) return true;
        return false;
    }
};

const char* tokName(Tok k);

} // namespace thz
