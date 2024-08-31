#include "lexer.h"
#include <iostream>
#include <cctype>
#include <array>
#include <cstddef>

enum char_category : char {
    ERR, WS, LF, CR, NOT, DQ, HSH, DOL, PCT, AND, SQ, OP, CP, MUL, ADD, COM,
    SUB, DOT, SL, NUL, DIG, COL, SC, LT, EQ, GT, AT, ID, OBK, BSL, CBK, CIR,
    OB, OR, CB, TIL
};

// clang-format off
static constexpr char_category DispatchTable[256] = {
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, WS,  LF,  WS,  WS,  CR,  ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    WS,  NOT, DQ,  HSH, DOL, PCT, AND, SQ,  OP,  CP,  MUL, ADD, COM, SUB, DOT, SL,
    NUL, DIG, DIG, DIG, DIG, DIG, DIG, DIG, DIG, DIG, COL, SC,  LT,  EQ,  GT,  ERR,
    AT,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
    ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  OBK, BSL, CBK, CIR, ID,
    ERR, ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,
    ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  ID,  OB,  OR,  CB,  TIL, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR, ERR,
};
// clang-format on

lexer::result lexer::run(char* begin, char* end) {
    i32 line = 1;
    auto c = begin;
    auto line_start = c;
    auto token_start = c;
    i32 token_line = 1;
    i32 token_offset = 0;
    lexeme_list lexemes;
    std::vector<lex_err> errors;

#define PRODUCE(TOKEN)                                                 \
    do {                                                               \
        lexemes.push_back(*create_lexeme(                              \
            file_path,                                                 \
            lexeme_type::TOKEN,                                        \
            token_line,                                                \
            token_offset,                                              \
            i32(c - token_start),                                      \
            std::string_view{ &*token_start, size_t(c - token_start) } \
        ));                                                            \
    } while(0)                                                         \

#define LEX_ERR(MSG)                                                    \
    do {                                                                \
        errors.emplace_back(                                            \
            std::string_view{ &*token_start, size_t(c - token_start) }, \
            MSG,                                                        \
            token_line,                                                 \
            token_offset                                                \
        );                                                              \
    } while(0)                                                          \

#define GOTO(LABEL)                         \
    do {                                    \
        token_start = c;                    \
        token_line = line;                  \
        token_offset = i32(c - line_start); \
        goto LABEL;                         \
    } while (0)                             \

#define NEW_LINE() do { ++line; line_start = c; } while(0)

    if (end - c >= 3 && c[0] == 0xEF && c[1] == 0xBB && c[2] == 0xBF)
        c += 3;

dispatch:
    if (c == end) {
        goto eof;
    }
    switch (DispatchTable[std::size_t(*c)]) {
        default:
        case ERR:
            token_start = c;
            ++c;
            LEX_ERR("dropping unexpected symbol");
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
        PRODUCE(LINE_END); // \r
        NEW_LINE();
        goto eof;
    } else if (*c == '\n') {
        goto line_end; // \r\n
    } else {
        PRODUCE(LINE_END); // \r
        NEW_LINE();
        goto dispatch;
    }

line_end:
    ++c;
    PRODUCE(LINE_END); // \n
    NEW_LINE();
    goto dispatch;

whitespace:
    if (++c == end) {
        PRODUCE(WHITESPACE);
        goto eof;
    } else if (std::isspace(*c)) {
        goto whitespace;
    } else {
        PRODUCE(WHITESPACE);
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
    } else if (*c == '*') {
        ++c;
        PRODUCE(POW);
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
        LEX_ERR("unclosed string");
        goto eof;
    } else if (*c == '"') {
        ++c;
        PRODUCE(STRING);
        goto dispatch;
    } else if (*c == '\n' || *c == '\r') {
        LEX_ERR("unclosed string");
        goto dispatch;
    } else if (*c == '\\') {
        goto string_escape;
    } else {
        goto string;
    }

string_escape:
    if (++c == end) {
        LEX_ERR("unclosed string");
        goto eof;
    }
    goto string;

name:
    if (++c == end) {
        LEX_ERR("unclosed name");
        goto eof;
    } else if (*c == '\'') {
        ++c;
        PRODUCE(NAME);
        goto dispatch;
    } else if (*c == '\n' || *c == '\r') {
        LEX_ERR("unclosed name");
        goto dispatch;
    } else if (*c == '\\') {
        goto name_escape;
    } else {
        goto name;
    }

name_escape:
    if (++c == end) {
        LEX_ERR("unclosed name");
        goto eof;
    }
    goto name;

octal:
    if (++c == end) {
        PRODUCE(OCTAL);
        goto eof;
    } else if (*c == 'x' || *c == 'X') {
        goto hexadecimal_start;
    } else if (*c == '.') {
        goto float_literal;
    } else if (*c == '8' || *c == '9') {
        LEX_ERR("invalid octal literal");
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
        LEX_ERR("invalid hexadecimal literal");
        PRODUCE(HEXADECIMAL);
        goto eof;
    } else if (std::isxdigit(*c)) {
        goto hexadecimal;
    } else {
        LEX_ERR("invalid hexadecimal literal");
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
    } else if (*c == 'e' || *c == 'E') {
        goto float_exponent_sign;
    } else if (*c == 'f' || *c == 'F') {
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
        LEX_ERR("invalid float literal");
        goto eof;
    } else if (*c == '-' || *c == '+') {
        goto float_exponent_sign_after;
    } else if (std::isdigit(*c)) {
        goto float_exponent;
    } else {
        LEX_ERR("invalid float literal");
        goto eof;
    }

float_exponent_sign_after:
    if (++c == end) {
        LEX_ERR("invalid float literal");
        goto eof;
    } else if (std::isdigit(*c)) {
        goto float_exponent;
    } else {
        LEX_ERR("invalid float literal");
        goto eof;
    }

float_exponent:
    if (++c == end) {
        PRODUCE(FLOAT);
        goto eof;
    } else if (std::isdigit(*c)) {
        goto float_exponent;
    } else if (*c == 'f' || *c == 'F') {
        ++c;
        PRODUCE(FLOAT);
        goto dispatch;
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
        PRODUCE(BACKSLASH);
        goto eof;
    } else if (*c == '\r') {
        goto line_continuation_cr;
    } else if (*c == '\n') {
        ++c;
        NEW_LINE();
        goto dispatch;
    } else {
        PRODUCE(BACKSLASH);
        goto dispatch;
    }

line_continuation_cr:
    if (++c == end) {
        goto eof;
    } else if (*c == '\n') {
        ++c;
        NEW_LINE();
        goto dispatch;
    } else {
        NEW_LINE();
        goto dispatch;
    }

line_comment:
    if (++c == end) {
        PRODUCE(COMMENT);
        goto eof;
    } else if (*c == '\n') {
        PRODUCE(COMMENT);
        GOTO(line_end);
    } else if (*c == '\r') {
        PRODUCE(COMMENT);
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
        PRODUCE(COMMENT);
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
        NEW_LINE();
        goto block_comment;
    }

block_comment_line_end_cr:
    if (++c == end) {
        goto block_comment_error;
    } else if (*c == '\n') {
        goto block_comment_line_end;
    } else {
        NEW_LINE();
        goto block_comment;
    }

block_comment_error:
    LEX_ERR("unexpected EOF in comment");
    PRODUCE(COMMENT);
    goto eof;

dot:
    if (++c == end) {
        PRODUCE(DOT);
    } else if (*c == '.') {
        goto dot_dot;
    } else {
        PRODUCE(DOT);
    }
    goto dispatch;

dot_dot:
    if (++c == end) {
        LEX_ERR("unexpected second dot");
    } else if (*c == '.') {
        PRODUCE(ELLIPSIS);
    }
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
    return { std::move(lexemes), errors };

#undef PRODUCE
#undef LEX_ERR
#undef GOTO
#undef NEW_LINE
}

void lexeme::write_to(std::ostream& os, const lexeme& next) {
    switch (type) {
        case lexeme_type::IDENTIFIER:
        case lexeme_type::OCTAL:
        case lexeme_type::DECIMAL:
        case lexeme_type::HEXADECIMAL:
        case lexeme_type::FLOAT:
            write_to(os);
            switch (next.type) {
                case lexeme_type::IDENTIFIER:
                case lexeme_type::OCTAL:
                case lexeme_type::DECIMAL:
                case lexeme_type::HEXADECIMAL:
                case lexeme_type::FLOAT:
                    os.put(' ');
                    break;
            }
            break;

        case lexeme_type::EQ:
        case lexeme_type::BIT_AND:
        case lexeme_type::BIT_OR:
        case lexeme_type::BIT_XOR:
        case lexeme_type::HASH:
            write_to(os);
            if (next.type == type)
                os.put(' ');
            break;

        case lexeme_type::LT:
        case lexeme_type::NOT:
        case lexeme_type::BIT_NOT:
        case lexeme_type::PLUS:
        case lexeme_type::MINUS:
        case lexeme_type::MUL:
        case lexeme_type::POW:
        case lexeme_type::DIV:
        case lexeme_type::MOD:
        case lexeme_type::CONCAT:
        case lexeme_type::CONCAT_SPACE:
            write_to(os);
            if (next.type == type || next.type == lexeme_type::EQ)
                os.put(' ');
            break;

        case lexeme_type::GT:
            write_to(os);
            if (next.type == type || next.type == lexeme_type::EQ || next.type == lexeme_type::SHR)
                os.put(' ');
            break;

        case lexeme_type::SHR:
            write_to(os);
            if (next.type == type || next.type == lexeme_type::EQ || next.type == lexeme_type::GT)
                os.put(' ');
            break;

        default:
            write_to(os);
            break;
    }
}

void lexeme::write_to(std::ostream& os) {
    os.write(text.data(), text.length());
}

struct chunk {
    std::array<std::byte, sizeof(lexeme) * 4096> data;
    std::byte* head;
    chunk* next;
};
static chunk* chunks_head;

void* allocate_lexeme_space() {
    static int _atexit_failed = std::atexit([]() {
        while (chunks_head) {
            auto c = chunks_head->next;
            delete chunks_head;
            chunks_head = c;
        }
    });

    if (chunks_head == nullptr || chunks_head->head == (void*) &chunks_head->head) {
        auto c = new chunk{};
        c->head = c->data.data();
        c->next = chunks_head;
        chunks_head = c;
    }

    auto that = chunks_head->head;
    chunks_head->head += sizeof(lexeme);

    return that;
}
