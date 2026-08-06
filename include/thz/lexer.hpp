#pragma once

#include "thz/token.hpp"

#include <string>
#include <vector>
#include <stdexcept>

namespace thz {

struct LexError : std::runtime_error {
    explicit LexError(const std::string& m) : std::runtime_error(m) {}
};

class Lexer {
public:
    explicit Lexer(std::string src);

    std::vector<Token> tokenize();
    [[noreturn]] void error(const std::string& msg) const;

private:
    std::string src_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;
    std::size_t col_ = 1;

    bool atEnd() const { return pos_ >= src_.size(); }
    char peek(std::size_t ahead = 0) const {
        std::size_t p = pos_ + ahead;
        return p < src_.size() ? src_[p] : '\0';
    }
    char advance();
    bool match(char c);
    void skipWhitespaceAndComments();

    Token make(Tok kind, std::string text, std::variant<std::monostate, double, std::string> value = {}) const;

    void lexNumber(Token& t);
    void lexIdentifier(Token& t);
    void lexString(Token& t);
    void lexTemplate(Token& t);
    void lexOperator(Token& t);
};

} // namespace thz
