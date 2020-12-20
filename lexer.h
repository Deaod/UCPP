#pragma once

#include <string_view>
#include <vector>
#include "types.h"

enum class lexeme_type : char {
    LINE_COMMENT,
    BLOCK_COMMENT,
    IDENTIFIER,
    STRING,
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
    CONCAT,
    CONCAT_EQ,
    CONCAT_SPACE,
    CONCAT_SPACE_EQ,
    DOT,
    COMMA,
    COLON,
    SEMICOLON,
    LINE_END,
    LINE_CONTINUATION,
    OPEN_PAREN,
    CLOSE_PAREN,
    OPEN_BRACE,
    CLOSE_BRACE,
    OPEN_BRACKET,
    CLOSE_BRACKET,
};

struct lexeme {
    explicit lexeme(lexeme_type type, i32 line, std::string_view text) :
        type(type), line(line), text(text) {}

    lexeme_type type;
    i32 line;
    std::string_view text;
};

class lexer {
public:
    std::vector<lexeme> run(const std::vector<char>& content);
};
