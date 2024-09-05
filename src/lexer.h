#pragma once

#include <string_view>
#include <vector>
#include <boost/intrusive/list.hpp>
#include "types.h"

enum class lexeme_type : char {
    WHITESPACE,
    IDENTIFIER,
    STRING,
    INCLUDE_STRING,
    NAME,
    OCTAL,
    DECIMAL,
    HEXADECIMAL,
    FLOAT,
    LT,
    LT_EQ,
    SHL,
    GT,
    GT_EQ,
    SHR,
    SHR_UNSIGNED,
    EQ,
    EQ_EQ,
    NOT,
    BIT_NOT,
    ALMOST,
    NEQ,
    PLUS,
    ADD_EQ,
    INCREMENT,
    MINUS,
    SUB_EQ,
    DECREMENT,
    MUL,
    MUL_EQ,
    POW,
    DIV,
    DIV_EQ,
    MOD,
    MOD_EQ,
    BIT_AND,
    AND,
    BIT_OR,
    OR,
    BIT_XOR,
    XOR,
    HASH,
    TOKEN_CONCAT,
    BACKSLASH,
    CONCAT,
    CONCAT_EQ,
    CONCAT_SPACE,
    CONCAT_SPACE_EQ,
    DOT,
    ELLIPSIS,
    COMMA,
    COLON,
    SEMICOLON,
    LINE_END,
    OPEN_PAREN,
    CLOSE_PAREN,
    OPEN_BRACE,
    CLOSE_BRACE,
    OPEN_BRACKET,
    CLOSE_BRACKET,
    COMMENT,

    META_USED_DEFINE_POP,
};

struct lexeme : boost::intrusive::list_base_hook<> {
    explicit lexeme(std::string_view fp, lexeme_type type, i32 line, i32 line_offset, i32 src_length, std::string_view text) :
        file_path(fp), type(type), line(line), line_offset(line_offset), src_length(src_length), text(text)
    {}

    std::string_view file_path;
    lexeme_type type;
    i32 line;
    i32 line_offset;
    i32 src_length;
    std::string_view text;

    lexeme(lexeme&&) = default;
    lexeme(const lexeme&) = default;
    ~lexeme() = default;
    lexeme& operator=(lexeme&&) = default;
    lexeme& operator=(const lexeme&) = default;

    void write_to(std::ostream& os, const lexeme& next);
    void write_to(std::ostream& os);

    struct disposer {
        void operator()(lexeme*) const {}
    };
};

void* allocate_lexeme_space();

template<typename... Args>
lexeme* create_lexeme(Args&&... args) {
    return new(allocate_lexeme_space()) lexeme(std::forward<Args>(args)...);
}

struct lex_err {
    explicit lex_err(std::string_view problem, std::string_view explanation, i32 line, i32 line_offset) :
        problem(problem), explanation(explanation), line(line), line_offset(line_offset)
    {}

    std::string_view problem;
    std::string_view explanation;
    i32 line;
    i32 line_offset;
};

using lexeme_list = boost::intrusive::list<lexeme>;
using lex_iter = lexeme_list::iterator;

class lexer {
public:
    lexer(std::string_view fp) : file_path(fp) {}

    struct result {
        lexeme_list lexemes;
        std::vector<lex_err> errors;
    };
    result run(std::vector<char>& v) { 
        if (v.size() == 0)
            return run(nullptr, nullptr);
        else
            return run(&*v.begin(), &*v.begin() + v.size());
    }
    result run(char* begin, char* end);

private:
    std::string_view file_path;
};
