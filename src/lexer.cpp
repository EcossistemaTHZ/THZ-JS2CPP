#include "thz/lexer.hpp"

#include <cctype>
#include <cstdlib>
#include <unordered_map>
#include <limits>

namespace thz {

const char* tokName(Tok k) {
    switch (k) {
    case Tok::End: return "End";
    case Tok::Ident: return "Ident";
    case Tok::Number: return "Number";
    case Tok::String: return "String";
    case Tok::Template: return "Template";
    case Tok::Let: return "let";
    case Tok::Const: return "const";
    case Tok::Var: return "var";
    case Tok::Function: return "function";
    case Tok::Return: return "return";
    case Tok::If: return "if";
    case Tok::Else: return "else";
    case Tok::While: return "while";
    case Tok::Do: return "do";
    case Tok::For: return "for";
    case Tok::Break: return "break";
    case Tok::Continue: return "continue";
    case Tok::Switch: return "switch";
    case Tok::Case: return "case";
    case Tok::Default: return "default";
    case Tok::Class: return "class";
    case Tok::New: return "new";
    case Tok::Extends: return "extends";
    case Tok::Super: return "super";
    case Tok::This: return "this";
    case Tok::Static: return "static";
    case Tok::Get: return "get";
    case Tok::Set: return "set";
    case Tok::Typeof: return "typeof";
    case Tok::Instanceof: return "instanceof";
    case Tok::In: return "in";
    case Tok::Of: return "of";
    case Tok::Delete: return "delete";
    case Tok::Void: return "void";
    case Tok::Async: return "async";
    case Tok::Await: return "await";
    case Tok::Try: return "try";
    case Tok::Catch: return "catch";
    case Tok::Finally: return "finally";
    case Tok::Throw: return "throw";
    case Tok::True: return "true";
    case Tok::False: return "false";
    case Tok::Null: return "null";
    case Tok::Undefined: return "undefined";
    case Tok::LParen: return "(";
    case Tok::RParen: return ")";
    case Tok::LBrace: return "{";
    case Tok::RBrace: return "}";
    case Tok::LBracket: return "[";
    case Tok::RBracket: return "]";
    case Tok::Comma: return ",";
    case Tok::Semicolon: return ";";
    case Tok::Colon: return ":";
    case Tok::Dot: return ".";
    case Tok::Question: return "?";
    case Tok::Ellipsis: return "...";
    case Tok::Arrow: return "=>";
    case Tok::Plus: return "+";
    case Tok::Minus: return "-";
    case Tok::Star: return "*";
    case Tok::Slash: return "/";
    case Tok::Percent: return "%";
    case Tok::PlusPlus: return "++";
    case Tok::MinusMinus: return "--";
    case Tok::Assign: return "=";
    case Tok::PlusAssign: return "+=";
    case Tok::MinusAssign: return "-=";
    case Tok::StarAssign: return "*=";
    case Tok::SlashAssign: return "/=";
    case Tok::PercentAssign: return "%=";
    case Tok::Eq: return "==";
    case Tok::Neq: return "!=";
    case Tok::StrictEq: return "===";
    case Tok::StrictNeq: return "!==";
    case Tok::Lt: return "<";
    case Tok::Gt: return ">";
    case Tok::Le: return "<=";
    case Tok::Ge: return ">=";
    case Tok::AndAnd: return "&&";
    case Tok::OrOr: return "||";
    case Tok::Bang: return "!";
    case Tok::BitAnd: return "&";
    case Tok::BitOr: return "|";
    case Tok::BitXor: return "^";
    case Tok::Tilde: return "~";
    case Tok::Shl: return "<<";
    case Tok::Shr: return ">>";
    case Tok::ShlAssign: return "<<=";
    case Tok::ShrAssign: return ">>=";
    case Tok::BitAndAssign: return "&=";
    case Tok::BitOrAssign: return "|=";
    case Tok::BitXorAssign: return "^=";
    case Tok::QuestionDot: return "?.";
    case Tok::Nullish: return "??";
    case Tok::NullishAssign: return "??" "=";
    case Tok::Backtick: return "`";
    }
    return "?";
}

static const std::unordered_map<std::string, Tok>& keywords() {
    static const std::unordered_map<std::string, Tok> m = {
        {"let", Tok::Let}, {"const", Tok::Const}, {"var", Tok::Var},
        {"function", Tok::Function}, {"return", Tok::Return},
        {"if", Tok::If}, {"else", Tok::Else}, {"while", Tok::While},
        {"do", Tok::Do}, {"for", Tok::For}, {"break", Tok::Break},
        {"continue", Tok::Continue}, {"switch", Tok::Switch},
        {"case", Tok::Case}, {"default", Tok::Default},
        {"class", Tok::Class}, {"new", Tok::New}, {"extends", Tok::Extends},
        {"super", Tok::Super}, {"this", Tok::This}, {"static", Tok::Static},
        {"get", Tok::Get}, {"set", Tok::Set}, {"typeof", Tok::Typeof},
        {"instanceof", Tok::Instanceof}, {"in", Tok::In}, {"of", Tok::Of},
        {"delete", Tok::Delete}, {"void", Tok::Void}, {"async", Tok::Async},
        {"await", Tok::Await}, {"try", Tok::Try}, {"catch", Tok::Catch},
        {"finally", Tok::Finally}, {"throw", Tok::Throw},
        {"true", Tok::True}, {"false", Tok::False}, {"null", Tok::Null},
        {"undefined", Tok::Undefined},
    };
    return m;
}

Lexer::Lexer(std::string src) : src_(std::move(src)) {}

void Lexer::skipWhitespaceAndComments() {
    for (;;) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
            advance();
            continue;
        }
        if (c == '/' && peek(1) == '/') {
            while (!atEnd() && peek() != '\n') advance();
            continue;
        }
        if (c == '/' && peek(1) == '*') {
            advance(); advance();
            while (!atEnd() && !(peek() == '*' && peek(1) == '/')) advance();
            if (!atEnd()) { advance(); advance(); }
            continue;
        }
        break;
    }
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') { ++line_; col_ = 1; } else { ++col_; }
    return c;
}

bool Lexer::match(char c) {
    if (peek() != c) return false;
    advance();
    return true;
}

Token Lexer::make(Tok kind, std::string text, std::variant<std::monostate, double, std::string> value) const {
    Token t;
    t.kind = kind;
    t.text = std::move(text);
    t.value = std::move(value);
    t.line = line_;
    t.col = col_;
    return t;
}

void Lexer::lexNumber(Token& t) {
    size_t start = pos_;
    bool isFloat = false;
    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        advance(); advance();
        while (std::isxdigit(static_cast<unsigned char>(peek()))) advance();
        std::string text = src_.substr(start, pos_ - start);
        t.kind = Tok::Number;
        t.text = text;
        t.value = static_cast<double>(std::strtoull(text.c_str(), nullptr, 16));
        return;
    }
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        isFloat = true;
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
    } else {
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
            isFloat = true;
            advance();
            while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }
    }
    if (peek() == 'e' || peek() == 'E') {
        char nxt = peek(1);
        if (std::isdigit(static_cast<unsigned char>(nxt)) ||
            ((nxt == '+' || nxt == '-') && std::isdigit(static_cast<unsigned char>(peek(2))))) {
            isFloat = true;
            advance();
            if (peek() == '+' || peek() == '-') advance();
            while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }
    }
    std::string text = src_.substr(start, pos_ - start);
    t.kind = Tok::Number;
    t.text = text;
    if (isFloat) t.value = std::strtod(text.c_str(), nullptr);
    else t.value = static_cast<double>(std::strtoll(text.c_str(), nullptr, 10));
}

void Lexer::lexIdentifier(Token& t) {
    size_t start = pos_;
    while (!atEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_' || peek() == '$')) advance();
    std::string text = src_.substr(start, pos_ - start);
    static const auto& kw = keywords();
    auto it = kw.find(text);
    t.text = text;
    if (it != kw.end()) t.kind = it->second;
    else t.kind = Tok::Ident;
}

void Lexer::lexString(Token& t) {
    char quote = advance(); // ' or "
    std::string raw = std::string(1, quote);
    std::string value;
    while (!atEnd() && peek() != quote) {
        char c = advance();
        raw += c;
        if (c == '\\') {
            if (atEnd()) break;
            char esc = advance();
            raw += esc;
            switch (esc) {
            case 'n': value += '\n'; break;
            case 't': value += '\t'; break;
            case 'r': value += '\r'; break;
            case '\\': value += '\\'; break;
            case '"': value += '"'; break;
            case '\'': value += '\''; break;
            case '0': value += '\0'; break;
            case '\n': value += '\n'; break;
            case 'b': value += '\b'; break;
            case 'f': value += '\f'; break;
            case 'v': value += '\v'; break;
            default: value += esc; break;
            }
        } else {
            value += c;
        }
    }
    if (!atEnd()) { raw += advance(); }
    t.kind = Tok::String;
    t.text = raw;
    t.value = value;
}

void Lexer::lexTemplate(Token& t) {
    // Consumes until closing backtick; substitutions not tokenized at lexer level.
    size_t start = pos_;
    advance(); // `
    while (!atEnd() && peek() != '`') {
        if (peek() == '\\') advance();
        advance();
    }
    if (!atEnd()) advance();
    t.kind = Tok::Template;
    t.text = src_.substr(start, pos_ - start);
    std::string inner = t.text.substr(1, t.text.size() - 2);
    t.value = inner;
}

void Lexer::lexOperator(Token& t) {
    size_t start = pos_;
    char c = advance();
    t.kind = [&]() -> Tok {
        switch (c) {
        case '(' : return Tok::LParen;
        case ')' : return Tok::RParen;
        case '{' : return Tok::LBrace;
        case '}' : return Tok::RBrace;
        case '[' : return Tok::LBracket;
        case ']' : return Tok::RBracket;
        case ',' : return Tok::Comma;
        case ';' : return Tok::Semicolon;
        case ':' : return Tok::Colon;
        case '~' : return Tok::Tilde;
        case '`' : return Tok::Backtick;
        case '?' : {
            if (peek() == '?') {
                if (peek(1) == '=') { advance(); advance(); return Tok::NullishAssign; }
                advance(); return Tok::Nullish;
            }
            if (peek() == '.') { advance(); return Tok::QuestionDot; }
            return Tok::Question;
        }
        case '.' :
            if (peek() == '.' && peek(1) == '.') { advance(); advance(); return Tok::Ellipsis; }
            return Tok::Dot;
        case '+' :
            if (peek() == '+') { advance(); return Tok::PlusPlus; }
            if (peek() == '=') { advance(); return Tok::PlusAssign; }
            return Tok::Plus;
        case '-' :
            if (peek() == '-' ) { advance(); return Tok::MinusMinus; }
            if (peek() == '>') { advance(); return Tok::Arrow; }
            if (peek() == '=') { advance(); return Tok::MinusAssign; }
            return Tok::Minus;
        case '*' :
            if (peek() == '=') { advance(); return Tok::StarAssign; }
            return Tok::Star;
        case '/' :
            if (peek() == '=') { advance(); return Tok::SlashAssign; }
            return Tok::Slash;
        case '%' :
            if (peek() == '=') { advance(); return Tok::PercentAssign; }
            return Tok::Percent;
        case '=' :
            if (peek() == '=') {
                if (peek(1) == '=') { advance(); advance(); return Tok::StrictEq; }
                advance(); return Tok::Eq;
            }
            if (peek() == '>') { advance(); return Tok::Arrow; }
            return Tok::Assign;
        case '!' :
            if (peek() == '=') {
                if (peek(1) == '=') { advance(); advance(); return Tok::StrictNeq; }
                advance(); return Tok::Neq;
            }
            return Tok::Bang;
        case '<' :
            if (peek() == '<') { advance(); return Tok::Shl; }
            if (peek() == '=') { advance(); return Tok::Le; }
            return Tok::Lt;
        case '>' :
            if (peek() == '>') { advance(); return Tok::Shr; }
            if (peek() == '=') { advance(); return Tok::Ge; }
            return Tok::Gt;
        case '&' :
            if (peek() == '&') { advance(); return Tok::AndAnd; }
            if (peek() == '=') { advance(); return Tok::BitAndAssign; }
            return Tok::BitAnd;
        case '|' :
            if (peek() == '|') { advance(); return Tok::OrOr; }
            if (peek() == '=') { advance(); return Tok::BitOrAssign; }
            return Tok::BitOr;
        case '^' :
            if (peek() == '=') { advance(); return Tok::BitXorAssign; }
            return Tok::BitXor;
        }
        error(std::string("caractere inesperado: '") + c + "'");
        return Tok::End;
    }();
    t.text = src_.substr(start, pos_ - start);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> out;
    for (;;) {
        skipWhitespaceAndComments();
        if (atEnd()) break;
        Token t;
        t.line = line_;
        t.col = col_;
        char c = peek();
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
            lexIdentifier(t);
        } else if (std::isdigit(static_cast<unsigned char>(c)) ||
                   (c == '.' && std::isdigit(static_cast<unsigned char>(peek(1))))) {
            lexNumber(t);
        } else if (c == '\'' || c == '"') {
            lexString(t);
        } else if (c == '`') {
            lexTemplate(t);
        } else {
            lexOperator(t);
        }
        out.push_back(t);
    }
    out.push_back(Token{}); // EOF
    return out;
}

void Lexer::error(const std::string& msg) const {
    throw LexError(msg + " (linha " + std::to_string(line_) + ", col " + std::to_string(col_) + ")");
}

} // namespace thz