#pragma once

#include <string_view>
#include <vector>
#include <list>
#include <boost/intrusive/list.hpp>
#include "types.h"
#include <typeinfo>

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
        void operator()(lexeme* l) const {
            delete l;
        }
    };

    static void* operator new(size_t s);
    static void operator delete(void* p);
};

struct lex_err {
    explicit lex_err(std::string_view problem, std::string_view explanation, i32 line, i32 line_offset) :
        problem(problem), explanation(explanation), line(line), line_offset(line_offset)
    {}

    std::string_view problem;
    std::string_view explanation;
    i32 line;
    i32 line_offset;
};

class lexer {
public:
    lexer(std::string_view fp) : file_path(fp) {}

    struct result {
        boost::intrusive::list<lexeme> lexemes;
        std::vector<lex_err> errors;
    };
    result run(std::vector<char>& v) { return run(&*v.begin(), &*v.end()); }
    result run(char* begin, char* end);

private:
    std::string_view file_path;
};

struct token {
    std::vector<lexeme*> lexemes;

    token() = default;
    token(lexeme* l) : lexemes{{l}} {}
    token(lexeme* f, lexeme* l) : lexemes() {
        while (f != l) {
            lexemes.push_back(f);
            f = reinterpret_cast<lexeme*>(f->next_);
        }
    }

    virtual bool is(const std::type_info& t) const {
        return t == typeid(token);
    }
    virtual ~token() {}
};

#define UCPP_TOKEN_DEF(TokenName, ParentToken)                \
struct TokenName : ParentToken {                              \
    bool is(const std::type_info& t) const override {         \
        return t == typeid(TokenName) || ParentToken::is(t);  \
    }                                                         \
}                                                             \

template<typename T, typename P>
struct token_is : std::is_base_of<P, T> {};

template<typename T, typename P>
constexpr bool token_is_v = token_is<T, P>::value;

UCPP_TOKEN_DEF(t_keyword, token);
UCPP_TOKEN_DEF(t_class, t_keyword);
UCPP_TOKEN_DEF(t_extends, t_keyword);
UCPP_TOKEN_DEF(t_expands, t_keyword);
UCPP_TOKEN_DEF(t_native, t_keyword);
UCPP_TOKEN_DEF(t_nativereplication, t_keyword);
UCPP_TOKEN_DEF(t_abstract, t_keyword);
UCPP_TOKEN_DEF(t_safereplace, t_keyword);
UCPP_TOKEN_DEF(t_perobjectconfig, t_keyword);
UCPP_TOKEN_DEF(t_noexport, t_keyword);
UCPP_TOKEN_DEF(t_placeable, t_keyword);
UCPP_TOKEN_DEF(t_if, t_keyword);
UCPP_TOKEN_DEF(t_else, t_keyword);
UCPP_TOKEN_DEF(t_while, t_keyword);
UCPP_TOKEN_DEF(t_until, t_keyword);
UCPP_TOKEN_DEF(t_for, t_keyword);
UCPP_TOKEN_DEF(t_foreach, t_keyword);
UCPP_TOKEN_DEF(t_break, t_keyword);
UCPP_TOKEN_DEF(t_continue, t_keyword);
UCPP_TOKEN_DEF(t_return, t_keyword);
UCPP_TOKEN_DEF(t_local, t_keyword);
UCPP_TOKEN_DEF(t_function, t_keyword);
UCPP_TOKEN_DEF(t_event, t_keyword);
UCPP_TOKEN_DEF(t_operator, t_keyword);
UCPP_TOKEN_DEF(t_preoperator, t_keyword);
UCPP_TOKEN_DEF(t_postoperator, t_keyword);
UCPP_TOKEN_DEF(t_static, t_keyword);
UCPP_TOKEN_DEF(t_simulated, t_keyword);
UCPP_TOKEN_DEF(t_final, t_keyword);
UCPP_TOKEN_DEF(t_optional, t_keyword);
UCPP_TOKEN_DEF(t_coerce, t_keyword);
UCPP_TOKEN_DEF(t_out, t_keyword);
UCPP_TOKEN_DEF(t_skip, t_keyword);
UCPP_TOKEN_DEF(t_const, t_keyword);
UCPP_TOKEN_DEF(t_var, t_keyword);
UCPP_TOKEN_DEF(t_none, t_keyword);
UCPP_TOKEN_DEF(t_bool, t_keyword);
UCPP_TOKEN_DEF(t_byte, t_keyword);
UCPP_TOKEN_DEF(t_int, t_keyword);
UCPP_TOKEN_DEF(t_float, t_keyword);
UCPP_TOKEN_DEF(t_pointer, t_keyword);
UCPP_TOKEN_DEF(t_name, t_keyword);
UCPP_TOKEN_DEF(t_string, t_keyword);
UCPP_TOKEN_DEF(t_array, t_keyword);
UCPP_TOKEN_DEF(t_iterator, t_keyword);
UCPP_TOKEN_DEF(t_enum, t_keyword);
UCPP_TOKEN_DEF(t_struct, t_keyword);
UCPP_TOKEN_DEF(t_config, t_keyword);
UCPP_TOKEN_DEF(t_globalconfig, t_keyword);
UCPP_TOKEN_DEF(t_travel, t_keyword);
UCPP_TOKEN_DEF(t_localized, t_keyword);
UCPP_TOKEN_DEF(t_editconst, t_keyword);
UCPP_TOKEN_DEF(t_private, t_keyword);
UCPP_TOKEN_DEF(t_export, t_keyword);
UCPP_TOKEN_DEF(t_transient, t_keyword);
UCPP_TOKEN_DEF(t_latent, t_keyword);
UCPP_TOKEN_DEF(t_replication, t_keyword);
UCPP_TOKEN_DEF(t_reliable, t_keyword);
UCPP_TOKEN_DEF(t_unreliable, t_keyword);
UCPP_TOKEN_DEF(t_defaultproperties, t_keyword);
UCPP_TOKEN_DEF(t_cpptext, t_keyword);
UCPP_TOKEN_DEF(t_true, t_keyword);
UCPP_TOKEN_DEF(t_false, t_keyword);
UCPP_TOKEN_DEF(t_self, t_keyword);
UCPP_TOKEN_DEF(t_vect, t_keyword);
UCPP_TOKEN_DEF(t_rot, t_keyword);
UCPP_TOKEN_DEF(t_arraycount, t_keyword);
UCPP_TOKEN_DEF(t_enumcount, t_keyword);

struct tok_err {
    int error_code;
    lexeme* first;
    lexeme* last;
};

class tokenizer {
    struct result {
        std::vector<token*> tokens;
        std::vector<tok_err> errors;
    };
    result run(const boost::intrusive::list<lexeme>& lexemes);
};
