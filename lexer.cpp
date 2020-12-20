#include "lexer.h"
#include <iostream>
#include <cctype>

constexpr const auto lf = "\n";

enum char_category {
    ERR, WS, LF, CR, NOT, DQ, HSH, DOL, PCT, AND, SQ, OP, CP, MUL, ADD, COM,
    SUB, DOT, SL, NUL, DIG, COL, SC, LT, EQ, GT, AT, ID, OBK, BSL, CBK, CIR,
    OB, OR, CB, TIL
};

static constexpr char_category TokenDispatchTable[256] = {
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, WS, LF, WS, WS, CR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    WS, NOT, DQ, HSH, DOL, PCT, AND, SQ, OP, CP, MUL, ADD, COM, SUB, DOT, SL,
    NUL, DIG, DIG, DIG, DIG, DIG, DIG, DIG, DIG, DIG, COL, SC, LT, EQ, GT, ERR,
    AT, ID, ID, ID, ID, ID, ID, ID, ID, ID, ID, ID, ID, ID, ID, ID,
    ID, ID, ID, ID, ID, ID, ID, ID, ID, ID, ID, OBK, BSL, CBK, CIR, ID,
    ERR, ID, ID, ID, ID, ID, ID, ID, ID, ID, ID, ID, ID, ID, ID, ID,
    ID, ID, ID, ID, ID, ID, ID, ID, ID, ID, ID, OB, OR, CB, TIL, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
};

std::vector<lexeme> lexer::run(const std::vector<char>& content) {
    i32 line = 1;
    auto c = content.begin();
    auto end = content.end();
    std::vector<lexeme> tokens;
    auto token_start = c;

#define PRODUCE(TOKEN)                                                 \
    do {                                                               \
        tokens.emplace_back(                                           \
            lexeme_type::TOKEN,                                        \
            line,                                                      \
            std::string_view{ &*token_start, size_t(c - token_start) } \
        );                                                             \
    } while(0)                                                         \

#define GOTO(LABEL)      \
    do {                 \
        token_start = c; \
        goto LABEL;      \
    } while (0)          \

dispatch:
    if (c == end) {
        goto eof;
    }
    switch (TokenDispatchTable[*c]) {
        default:
        case ERR:
            std::cerr << "dropping unexpected symbol: " << *c << lf;
            ++c;
            goto dispatch;
        case WS:
            GOTO(whitespace);
        case LF:
            GOTO(line_end);
        case CR:
            GOTO(line_end_cr);
        case NOT:
            GOTO(not_operator);
        case DQ:
            GOTO(string);
        case HSH:
            GOTO(hash);
        case DOL:
            GOTO(concat);
        case PCT:
            GOTO(modulo);
        case AND:
            GOTO(and_operator);
        case SQ:
            GOTO(name);
        case OP:
            GOTO(open_paren);
        case CP:
            GOTO(close_paren);
        case MUL:
            GOTO(asterisk);
        case ADD:
            GOTO(plus);
        case COM:
            GOTO(comma);
        case SUB:
            GOTO(minus);
        case DOT:
            GOTO(dot);
        case SL:
            GOTO(slash);
        case NUL:
            GOTO(octal);
        case DIG:
            GOTO(decimal);
        case COL:
            GOTO(colon);
        case SC:
            GOTO(semicolon);
        case LT:
            GOTO(less_than);
        case EQ:
            GOTO(equal);
        case GT:
            GOTO(greater_than);
        case AT:
            GOTO(concat_space);
        case ID:
            GOTO(identifier);
        case OBK:
            GOTO(open_bracket);
        case BSL:
            GOTO(line_continuation);
        case CBK:
            GOTO(close_bracket);
        case CIR:
            GOTO(xor_operator);
        case OB:
            GOTO(open_brace);
        case OR:
            GOTO(or_operator);
        case CB:
            GOTO(close_brace);
        case TIL:
            GOTO(tilde);
    }

line_end_cr:
    if (++c == end) {
        PRODUCE(LINE_END);
        ++line;
        goto eof;
    } else if (*c == '\n') {
        goto line_end;
    } else {
        PRODUCE(LINE_END);
        ++line;
        goto dispatch;
    }

line_end:
    ++c;
    PRODUCE(LINE_END);
    ++line;
    goto dispatch;

whitespace:
    if (++c == end) {
        goto eof;
    } else if (*c == '\r') {
        GOTO(line_end_cr);
    } else if (*c == '\n') {
        GOTO(line_end);
    } else if (std::isspace(*c)) {
        goto whitespace;
    } else {
        goto dispatch;
    }

identifier:
    if (++c == end) {
        PRODUCE(IDENTIFIER);
        goto eof;
    } else if (std::isalnum(*c) || *c == '_') {
        goto identifier;
    } else {
        PRODUCE(IDENTIFIER);
        goto dispatch;
    }

plus:
    if (++c == end) {
        PRODUCE(PLUS);
    } else if (*c == '+') {
        ++c;
        PRODUCE(INCREMENT);
    } else if (*c == '=') {
        ++c;
        PRODUCE(ADD_EQ);
    } else {
        PRODUCE(PLUS);
    }
    goto dispatch;

minus:
    if (++c == end) {
        PRODUCE(MINUS);
    } else if (*c == '-') {
        ++c;
        PRODUCE(DECREMENT);
    } else if (*c == '=') {
        ++c;
        PRODUCE(SUB_EQ);
    } else {
        PRODUCE(MINUS);
    }
    goto dispatch;

asterisk:
    if (++c == end) {
        PRODUCE(MUL);
    } else if (*c == '=') {
        ++c;
        PRODUCE(MUL_EQ);
    } else {
        PRODUCE(MUL);
    }
    goto dispatch;

slash:
    if (++c == end) {
        PRODUCE(DIV);
        goto eof;
    } else if (*c == '/') {
        goto line_comment;
    } else if (*c == '*') {
        goto block_comment;
    } else if (*c == '=') {
        ++c;
        PRODUCE(DIV_EQ);
        goto dispatch;
    } else {
        PRODUCE(DIV);
        goto dispatch;
    }

modulo:
    if (++c == end) {
        PRODUCE(MOD);
    } else if (*c == '=') {
        ++c;
        PRODUCE(MOD_EQ);
    } else {
        PRODUCE(MOD);
    }
    goto dispatch;

and_operator:
    if (++c == end) {
        PRODUCE(BIT_AND);
    } else if (*c == '&') {
        ++c;
        PRODUCE(AND);
    } else {
        PRODUCE(BIT_AND);
    }
    goto dispatch;

or_operator:
    if (++c == end) {
        PRODUCE(BIT_OR);
    } else if (*c == '|') {
        ++c;
        PRODUCE(OR);
    } else {
        PRODUCE(BIT_OR);
    }
    goto dispatch;

xor_operator:
    if (++c == end) {
        PRODUCE(BIT_XOR);
    } else if (*c == '^') {
        ++c;
        PRODUCE(XOR);
    } else {
        PRODUCE(BIT_XOR);
    }
    goto dispatch;

string:
    if (++c == end) {
        std::cerr << "unclosed string on line " << line << lf;
        goto eof;
    } else if (*c == '"') {
        ++c;
        PRODUCE(STRING);
        goto dispatch;
    } else if (*c == '\n' || *c == '\r') {
        std::cerr << "unclosed string on line " << line << lf;
        goto dispatch;
    } else if (*c == '\\') {
        goto string_escape;
    } else {
        goto string;
    }

string_escape:
    if (++c == end) {
        std::cerr << "unclosed string on line " << line << lf;
        goto eof;
    }
    goto string;

name:
    if (++c == end) {
        std::cerr << "unclosed name on line " << line << lf;
        goto eof;
    } else if (*c == '\'') {
        ++c;
        PRODUCE(NAME);
        goto dispatch;
    } else if (*c == '\n' || *c == '\r') {
        std::cerr << "unclosed name on line " << line << lf;
        goto dispatch;
    } else if (*c == '\\') {
        goto name_escape;
    } else {
        goto name;
    }

name_escape:
    if (++c == end) {
        std::cerr << "unclosed name on line " << line << lf;
        goto eof;
    }
    goto name;

octal:
    if (++c == end) {
        PRODUCE(OCTAL);
        goto eof;
    } else if (*c == 'x') {
        goto hexadecimal_start;
    } else if (*c == '.') {
        goto float_literal;
    } else if (*c == '8' || *c == '9') {
        std::cerr << "invalid octal literal on line " << line << lf;
        goto decimal;
    } else if (std::isdigit(*c)) {
        goto octal;
    } else {
        PRODUCE(OCTAL);
        goto dispatch;
    }

decimal:
    if (++c == end) {
        PRODUCE(DECIMAL);
        goto eof;
    } else if (*c == '.') {
        goto float_literal;
    } else if (std::isdigit(*c)) {
        goto decimal;
    } else {
        PRODUCE(DECIMAL);
        goto dispatch;
    }

hexadecimal_start:
    if (++c == end) {
        std::cerr << "invalid hexadecimal literal on line " << line << lf;
        PRODUCE(HEXADECIMAL);
        goto eof;
    } else if (std::isxdigit(*c)) {
        goto hexadecimal;
    } else {
        std::cerr << "invalid hexadecimal literal on line " << line << lf;
        PRODUCE(HEXADECIMAL);
        goto eof;
    }

hexadecimal:
    if (++c == end) {
        PRODUCE(HEXADECIMAL);
        goto eof;
    } else if (std::isxdigit(*c)) {
        goto hexadecimal;
    } else {
        PRODUCE(HEXADECIMAL);
        goto dispatch;
    }

float_literal:
    if (++c == end) {
        PRODUCE(FLOAT);
        goto eof;
    } else if (*c == 'e') {
        goto float_exponent_sign;
    } else if (*c == 'f') {
        ++c;
        PRODUCE(FLOAT);
        goto dispatch;
    } else if (std::isdigit(*c)) {
        goto float_literal;
    } else {
        PRODUCE(FLOAT);
        goto dispatch;
    }

float_exponent_sign:
    if (++c == end) {
        std::cerr << "unexpected EOF in float literal on line " << line << lf;
        goto eof;
    } else if (*c == '-' || *c == '+' || std::isdigit(*c)) {
        goto float_exponent;
    } else {
        std::cerr << "invalid exponent in float literal on line " << line << lf;
        goto eof;
    }

float_exponent:
    if (++c == end) {
        PRODUCE(FLOAT);
        goto eof;
    } else if (std::isdigit(*c)) {
        goto float_exponent;
    } else {
        PRODUCE(FLOAT);
        goto dispatch;
    }

hash:
    if (++c == end) {
        PRODUCE(HASH);
        goto eof;
    } else if (*c == '#') {
        ++c;
        PRODUCE(TOKEN_CONCAT);
        goto dispatch;
    } else {
        PRODUCE(HASH);
        goto dispatch;
    }

concat:
    if (++c == end) {
        PRODUCE(CONCAT);
        goto eof;
    } else if (*c == '=') {
        ++c;
        PRODUCE(CONCAT_EQ);
        goto dispatch;
    } else {
        PRODUCE(CONCAT);
        goto dispatch;
    }

concat_space:
    if (++c == end) {
        PRODUCE(CONCAT_SPACE);
        goto eof;
    } else if (*c == '=') {
        ++c;
        PRODUCE(CONCAT_SPACE_EQ);
        goto dispatch;
    } else {
        PRODUCE(CONCAT_SPACE);
        goto dispatch;
    }

equal:
    if (++c == end) {
        PRODUCE(EQ);
    } else if (*c == '=') {
        ++c;
        PRODUCE(EQ_EQ);
    } else {
        PRODUCE(EQ);
    }
    goto dispatch;

not_operator:
    if (++c == end) {
        PRODUCE(NOT);
    } else if (*c == '=') {
        ++c;
        PRODUCE(NEQ);
    } else {
        PRODUCE(NOT);
    }
    goto dispatch;

tilde:
    if (++c == end) {
        PRODUCE(BIT_NOT);
    } else if (*c == '=') {
        ++c;
        PRODUCE(ALMOST);
    } else {
        PRODUCE(BIT_NOT);
    }
    goto dispatch;

less_than:
    if (++c == end) {
        PRODUCE(LT);
    } else if (*c == '=') {
        ++c;
        PRODUCE(LT_EQ);
    } else if (*c == '<') {
        ++c;
        PRODUCE(SHL);
    } else {
        PRODUCE(LT);
    }
    goto dispatch;

greater_than:
    if (++c == end) {
        PRODUCE(GT);
    } else if (*c == '=') {
        ++c;
        PRODUCE(GT_EQ);
    } else if (*c == '>') {
        goto shift_right;
    } else {
        PRODUCE(GT);
    }
    goto dispatch;

shift_right:
    if (++c == end) {
        PRODUCE(SHR);
    } else if (*c == '>') {
        ++c;
        PRODUCE(SHR_UNSIGNED);
    } else {
        PRODUCE(SHR);
    }
    goto dispatch;

line_continuation:
    if (++c == end) {
        std::cerr << "unexpected EOF in line continuation on line " << line << lf;
        goto eof;
    } else if (*c == '\r') {
        goto line_continuation_cr;
    } else if (*c == '\n') {
        ++c;
        PRODUCE(LINE_CONTINUATION);
        ++line;
        goto dispatch;
    } else {
        std::cerr << "unexpected '\\' on line " << line << lf;
        goto dispatch;
    }

line_continuation_cr:
    if (++c == end) {
        PRODUCE(LINE_CONTINUATION);
        goto eof;
    } else if (*c == '\n') {
        ++c;
        PRODUCE(LINE_CONTINUATION);
        ++line;
        goto dispatch;
    } else {
        PRODUCE(LINE_CONTINUATION);
        ++line;
        goto dispatch;
    }

line_comment:
    if (++c == end) {
        PRODUCE(LINE_COMMENT);
        goto eof;
    } else if (*c == '\n') {
        PRODUCE(LINE_COMMENT);
        GOTO(line_end);
    } else if (*c == '\r') {
        PRODUCE(LINE_COMMENT);
        GOTO(line_end_cr);
    } else {
        goto line_comment;
    }

block_comment:
    if (++c == end) {
        goto block_comment_error;
    } else if (*c == '*') {
        goto block_comment_end;
    } else if (*c == '\r') {
        goto block_comment_line_end_cr;
    } else if (*c == '\n') {
        goto block_comment_line_end;
    } else {
        goto block_comment;
    }

block_comment_end:
    if (++c == end) {
        goto block_comment_error;
    } else if (*c == '/') {
        ++c;
        PRODUCE(BLOCK_COMMENT);
        goto dispatch;
    } else if (*c == '\r') {
        goto block_comment_line_end_cr;
    } else if (*c == '\n') {
        goto block_comment_line_end;
    } else {
        goto block_comment;
    }

block_comment_line_end:
    if (++c == end) {
        goto block_comment_error;
    } else {
        ++line;
        goto block_comment;
    }

block_comment_line_end_cr:
    if (++c == end) {
        goto block_comment_error;
    } else if (*c == '\n') {
        goto block_comment_line_end;
    } else {
        ++line;
        goto block_comment;
    }

block_comment_error:
    std::cerr << "unexpected EOF in comment" << lf;
    PRODUCE(BLOCK_COMMENT);
    goto eof;

dot:
    ++c;
    PRODUCE(DOT);
    goto dispatch;

comma:
    ++c;
    PRODUCE(COMMA);
    goto dispatch;

colon:
    ++c;
    PRODUCE(COLON);
    goto dispatch;

semicolon:
    ++c;
    PRODUCE(SEMICOLON);
    goto dispatch;

open_paren:
    ++c;
    PRODUCE(OPEN_PAREN);
    goto dispatch;

close_paren:
    ++c;
    PRODUCE(CLOSE_PAREN);
    goto dispatch;

open_brace:
    ++c;
    PRODUCE(OPEN_BRACE);
    goto dispatch;

close_brace:
    ++c;
    PRODUCE(CLOSE_BRACE);
    goto dispatch;

open_bracket:
    ++c;
    PRODUCE(OPEN_BRACKET);
    goto dispatch;

close_bracket:
    ++c;
    PRODUCE(CLOSE_BRACKET);
    goto dispatch;

eof:

    return tokens;

#undef PRODUCE
#undef GOTO
}
