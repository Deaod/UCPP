#include "preprocessor.h"
#include "lexer.h"
#include "scope_guard.h"

#include <cctype>
#include <format>
#include <charconv>

constexpr const auto lf = "\n";

constexpr std::string_view dir_include{"include"};
constexpr std::string_view dir_define{"define"};
constexpr std::string_view dir_undef{"undef"};
constexpr std::string_view dir_if{"if"};
constexpr std::string_view dir_elif{"elif"};
constexpr std::string_view dir_else{"else"};
constexpr std::string_view dir_endif{"endif"};
constexpr std::string_view dir_ifdef{"ifdef"};
constexpr std::string_view dir_ifndef{"ifndef"};
constexpr std::string_view sym_defined{"defined"};

constexpr std::string_view sym_zero{"0"};
constexpr std::string_view sym_one{"1"};

// returns the next useful lexeme
// skips over whitespace and comment lexemes
// line endings are not counted as whitespace
lex_iter next_lexeme(lex_iter l, lex_iter end) {
    while (++l != end && (l->type == lexeme_type::WHITESPACE || l->type == lexeme_type::COMMENT));
    return l;
}

lex_iter seek_line_end(lex_iter l, lex_iter end) {
    while (++l != end && l->type != lexeme_type::LINE_END);
    return l;
}

/*
Grammar

or_expr      = and_expr {"||" and_expr}
and_expr     = cmp_expr {"&&" cmp_expr}
cmp_expr     = bit_or_expr {("=="|"!="|">"|">="|"<"|"<=) bit_or_expr}
bit_or_expr  = bit_and_expr {("|"|"^") bit_and_expr}
bit_and_expr = shift_expr {"&" shift_expr}
shift_expr   = add_expr {("<<"|">>"|">>>") add_expr}
add_expr     = mul_expr {("+"|"-") mul_expr}
mul_expr     = pow_expr {("*"|"/"|"%") pow_expr}
pow_expr     = unary_expr {"**" unary_expr}
unary_expr   = [("+"|"-"|"~"|"!")] unary_expr
             | "defined" paren_expr
paren_expr   = Ident ["(" [or_expr {, or_expr}] ")"] -- not implemented currently
             | Number
             | "(" or_expr ")"
 */

struct value {
    explicit value(u32 val) : _value(val) {}
    u32 int_value() const {
        return _value;
    }
private:
    u32 _value;
};

enum class expression_type {
    NONE,
    OR,
    AND,
    EQ,
    NEQ,
    GT,
    GEQ,
    LT,
    LEQ,
    BIT_OR,
    BIT_XOR,
    BIT_AND,
    SHL,
    SHR,
    SHRU,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    POW,
    POS,
    NEG,
    NOT,
    BIT_NOT,
    DEF,
    LITERAL,
    NAME
};

struct expression {
    expression(expression_parser* parser) : _parser(parser) {}
    virtual ~expression() = default;
    virtual value evaluate() const = 0;
    virtual expression_type type() const = 0;

protected:
    expression_parser* _parser;
};

struct binary_expression : expression {
    explicit binary_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept :
        expression(parser), _lhs(lhs), _rhs(rhs) {}

protected:
    expression* _lhs;
    expression* _rhs;
};

struct or_expression : binary_expression {
    explicit or_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() || _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::OR; }
};

struct and_expression : binary_expression {
    explicit and_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() && _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::AND; }
};

struct eq_expression : binary_expression {
    explicit eq_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() == _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::EQ; }
};

struct neq_expression : binary_expression {
    explicit neq_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() != _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::NEQ; }
};

struct gt_expression : binary_expression {
    explicit gt_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() > _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::GT; }
};

struct geq_expression : binary_expression {
    explicit geq_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() >= _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::GEQ; }
};

struct lt_expression : binary_expression {
    explicit lt_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() < _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::LT; }
};

struct leq_expression : binary_expression {
    explicit leq_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() <= _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::LEQ; }
};

struct bit_or_expression : binary_expression {
    explicit bit_or_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() | _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::BIT_OR; }
};

struct bit_xor_expression : binary_expression {
    explicit bit_xor_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() ^ _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::BIT_XOR; }
};

struct bit_and_expression : binary_expression {
    explicit bit_and_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() & _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::BIT_AND; }
};

struct shl_expression : binary_expression {
    explicit shl_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() << _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::SHL; }
};

struct shr_expression : binary_expression {
    explicit shr_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{u32(i32(_lhs->evaluate().int_value()) >> _rhs->evaluate().int_value())}; }
    expression_type type() const override { return expression_type::SHR; }
};

struct shru_expression : binary_expression {
    explicit shru_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() >> _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::SHRU; }
};

struct add_expression : binary_expression {
    explicit add_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() + _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::ADD; }
};

struct sub_expression : binary_expression {
    explicit sub_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() - _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::SUB; }
};

struct mul_expression : binary_expression {
    explicit mul_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() * _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::MUL; }
};

struct div_expression : binary_expression {
    explicit div_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() / _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::DIV; }
};

struct mod_expression : binary_expression {
    explicit mod_expression(expression_parser* parser, expression* lhs, expression* rhs) noexcept : binary_expression(parser, lhs, rhs) {}
    value evaluate() const override { return value{_lhs->evaluate().int_value() % _rhs->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::MOD; }
};

struct unary_expression : expression {
    explicit unary_expression(expression_parser* parser, expression* operand) noexcept : expression(parser), _operand(operand) {}

protected:
    expression* _operand;
};

struct pos_expression : unary_expression {
    explicit pos_expression(expression_parser* parser, expression* operand) noexcept : unary_expression(parser, operand) {}
    value evaluate() const override { return value{+_operand->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::POS; }
};

struct neg_expression : unary_expression {
    explicit neg_expression(expression_parser* parser, expression* operand) noexcept : unary_expression(parser, operand) {}
    value evaluate() const override { return value{u32(-i32(_operand->evaluate().int_value()))}; }
    expression_type type() const override { return expression_type::NEG; }
};

struct not_expression : unary_expression {
    explicit not_expression(expression_parser* parser, expression* operand) noexcept : unary_expression(parser, operand) {}
    value evaluate() const override { return value{!_operand->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::NOT; }
};

struct bit_not_expression : unary_expression {
    explicit bit_not_expression(expression_parser* parser, expression* operand) noexcept : unary_expression(parser, operand) {}
    value evaluate() const override { return value{~_operand->evaluate().int_value()}; }
    expression_type type() const override { return expression_type::BIT_NOT; }
};

//struct defined_expression : unary_expression {
//    explicit defined_expression(expression_parser* parser, expression* operand) noexcept : unary_expression(parser, operand) {}
//    value evaluate() const override {
//
//        // error 
//        return value{0};
//    }
//    expression_type type() const override { return expression_type::DEF; }
//};

struct literal_expression : expression {
    explicit literal_expression(expression_parser* parser, u32 value) noexcept : expression(parser), _value(value) {}
    value evaluate() const override { return _value; }
    expression_type type() const override { return expression_type::LITERAL; }

protected:
    value _value;
};

struct name_expression : expression {
    explicit name_expression(expression_parser* parser, lexeme* name) : expression(parser), _name(name) {}
    value evaluate() const override { return value{0}; }
    expression_type type() const override { return expression_type::NAME; }
    std::string_view name() const {
        return _name->text;
    }

protected:
    lexeme* _name;
};

struct expression_parser {
    expression_parser(preprocessor* p) : _preprocessor(p) {}
    ~expression_parser() {
        while (_chunks) {
            auto next = _chunks->next;
            delete _chunks;
            _chunks = next;
        }
    }

    void error(lexeme* l, const char* msg) {
        _preprocessor->error(l, msg);
    }

    void warn(lexeme* l, const char* msg) {
        _preprocessor->warn(l, msg);
    }

#define PARSE_ERR(LEX,MSG) error(LEX, MSG_DEBUG "error: "   MSG)
#define PARSE_WARN(LEX,MSG) warn(LEX, MSG_DEBUG "warning: " MSG)

    expression* parse(lex_iter beg, lex_iter end) {
        if (beg == end)
            return nullptr;
        
        for (auto l = beg; l != end;) {
            if (l->type != lexeme_type::IDENTIFIER || l->text != sym_defined) {
                l = _preprocessor->replace_identifier(l);
                continue;
            }

            auto it = next_lexeme(l, end);
            if (it == end) {
                PARSE_ERR(&*l, "missing operand for operator \"defined\"");
                return nullptr;
            }
            bool paren_used = false;
            if (it->type == lexeme_type::OPEN_PAREN) {
                paren_used = true;
                it = next_lexeme(it, end);
                if (it == end) {
                    PARSE_ERR(&*l, "missing operand for operator \"defined\"");
                    return nullptr;
                }
            }
            if (it->type == lexeme_type::IDENTIFIER) {
                l = _preprocessor->insert(l, create_lexeme(
                    l->file_path,
                    lexeme_type::DECIMAL,
                    l->line,
                    l->line_offset,
                    l->src_length,
                    _preprocessor->is_defined(it->text) ? sym_one : sym_zero
                ));
            } else {
                PARSE_ERR(&*it, "expected identifier");
                return nullptr;
            }
            if (paren_used) {
                it = next_lexeme(it, end);
                if (it == end) {
                    PARSE_ERR(&*it, "missing closing parenthesis");
                    return nullptr;
                }
                if (it->type != lexeme_type::CLOSE_PAREN) {
                    PARSE_ERR(&*it, "expected closing parenthesis");
                    return nullptr;
                }
            }
            auto rem_it = l;
            _preprocessor->remove(++rem_it, ++it);
            l = it;
        }

        if (beg->type == lexeme_type::WHITESPACE || beg->type == lexeme_type::COMMENT)
            beg = next_lexeme(beg, end);
        return or_expr(beg, end);
    }

    expression* or_expr(lex_iter& l, lex_iter end) {
        auto result = and_expr(l, end);
        if (result == nullptr)
            return nullptr;
        while (l != end && l->type == lexeme_type::OR) {
            l = next_lexeme(l, end);
            auto rhs = and_expr(l, end);
            if (rhs == nullptr)
                return nullptr;
            result = create<or_expression>(result, rhs);
        }
        return result;
    }

    expression* and_expr(lex_iter& l, lex_iter end) {
        auto result = cmp_expr(l, end);
        if (result == nullptr)
            return nullptr;
        while (l != end && l->type == lexeme_type::AND) {
            l = next_lexeme(l, end);
            auto rhs = cmp_expr(l, end);
            if (rhs == nullptr)
                return nullptr;
            result = create<and_expression>(result, rhs);
        }
        return result;
    }

    expression* cmp_expr(lex_iter& l, lex_iter end) {
        auto result = bit_or_expr(l, end);
        if (result == nullptr)
            return nullptr;
        while (l != end) {
            if (l->type == lexeme_type::EQ_EQ) {
                l = next_lexeme(l, end);
                auto rhs = bit_or_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<eq_expression>(result, rhs);
            } else if (l->type == lexeme_type::NEQ) {
                l = next_lexeme(l, end);
                auto rhs = bit_or_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<neq_expression>(result, rhs);
            } else if (l->type == lexeme_type::GT) {
                l = next_lexeme(l, end);
                auto rhs = bit_or_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<gt_expression>(result, rhs);
            } else if (l->type == lexeme_type::GT_EQ) {
                l = next_lexeme(l, end);
                auto rhs = bit_or_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<geq_expression>(result, rhs);
            } else if (l->type == lexeme_type::LT) {
                l = next_lexeme(l, end);
                auto rhs = bit_or_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<lt_expression>(result, rhs);
            } else if (l->type == lexeme_type::LT_EQ) {
                l = next_lexeme(l, end);
                auto rhs = bit_or_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<leq_expression>(result, rhs);
            } else {
                break;
            }
        }
        return result;
    }

    expression* bit_or_expr(lex_iter& l, lex_iter end) {
        auto result = bit_and_expr(l, end);
        if (result == nullptr)
            return nullptr;
        while (l != end) {
            if (l->type == lexeme_type::BIT_OR) {
                l = next_lexeme(l, end);
                auto rhs = bit_and_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<bit_or_expression>(result, rhs);
            } else if (l->type == lexeme_type::BIT_XOR) {
                l = next_lexeme(l, end);
                auto rhs = bit_and_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<bit_xor_expression>(result, rhs);
            } else {
                break;
            }
        }
        return result;
    }

    expression* bit_and_expr(lex_iter& l, lex_iter end) {
        auto result = shift_expr(l, end);
        if (result == nullptr)
            return nullptr;
        while (l != end && l->type == lexeme_type::BIT_AND) {
            l = next_lexeme(l, end);
            auto rhs = shift_expr(l, end);
            if (rhs == nullptr)
                return nullptr;
            result = create<bit_and_expression>(result, rhs);
        }
        return result;
    }

    expression* shift_expr(lex_iter& l, lex_iter end) {
        auto result = add_expr(l, end);
        if (result == nullptr)
            return nullptr;
        while (l != end) {
            if (l->type == lexeme_type::SHL) {
                l = next_lexeme(l, end);
                auto rhs = add_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<shl_expression>(result, rhs);
            } else if (l->type == lexeme_type::SHR) {
                l = next_lexeme(l, end);
                auto rhs = add_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<shr_expression>(result, rhs);
            } else if (l->type == lexeme_type::SHR_UNSIGNED) {
                l = next_lexeme(l, end);
                auto rhs = add_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<shru_expression>(result, rhs);
            } else {
                break;
            }
        }
        return result;
    }

    expression* add_expr(lex_iter& l, lex_iter end) {
        auto result = mul_expr(l, end);
        if (result == nullptr)
            return nullptr;
        while (l != end) {
            if (l->type == lexeme_type::PLUS) {
                l = next_lexeme(l, end);
                auto rhs = mul_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<add_expression>(result, rhs);
            } else if (l->type == lexeme_type::MINUS) {
                l = next_lexeme(l, end);
                auto rhs = mul_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<sub_expression>(result, rhs);
            } else {
                break;
            }
        }
        return result;
    }

    expression* mul_expr(lex_iter& l, lex_iter end) {
        auto result = pow_expr(l, end);
        if (result == nullptr)
            return nullptr;
        while (l != end) {
            if (l->type == lexeme_type::MUL) {
                l = next_lexeme(l, end);
                auto rhs = pow_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<mul_expression>(result, rhs);
            } else if (l->type == lexeme_type::DIV) {
                l = next_lexeme(l, end);
                auto rhs = pow_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<div_expression>(result, rhs);
            } else if (l->type == lexeme_type::MOD) {
                l = next_lexeme(l, end);
                auto rhs = pow_expr(l, end);
                if (rhs == nullptr)
                    return nullptr;
                result = create<mod_expression>(result, rhs);
            } else {
                break;
            }
        }
        return result;
    }

    expression* pow_expr(lex_iter& l, lex_iter end) {
        auto result = unary_expr(l, end);
        // maybe in the future if we support floats
        return result;
    }

    expression* unary_expr(lex_iter& l, lex_iter end) {
        if (l->type == lexeme_type::PLUS) {
            l = next_lexeme(l, end);
            return create<pos_expression>(unary_expr(l, end));
        } else if (l->type == lexeme_type::MINUS) {
            l = next_lexeme(l, end);
            return create<neg_expression>(unary_expr(l, end));
        } else if (l->type == lexeme_type::NOT) {
            l = next_lexeme(l, end);
            return create<not_expression>(unary_expr(l, end));
        } else if (l->type == lexeme_type::BIT_NOT) {
            l = next_lexeme(l, end);
            return create<bit_not_expression>(unary_expr(l, end));
        //} else if (l->type == lexeme_type::IDENTIFIER && l->text == sym_defined) {
        //    l = next_lexeme(l, end);
        //    auto name = paren_expr(l, end);
        //    if (name->type() == expression_type::NAME) {
        //        bool defined = _preprocessor->is_defined(reinterpret_cast<name_expression*>(name)->name());
        //        return create<literal_expression>(i32(defined));
        //    }
        //    return create<bit_not_expression>(name);
        }
        return paren_expr(l, end);
    }

    expression* paren_expr(lex_iter& l, lex_iter end) {
        if (l == end) {
            PARSE_ERR(&*l, "expected token, but found none");
            return nullptr;
        } else if (l->type == lexeme_type::IDENTIFIER) {
            auto result = create<name_expression>(&*l);
            PARSE_WARN(&*l, "undefined macro, substituting 0");
            l = next_lexeme(l, end);
            return result;
        } else if (l->type == lexeme_type::DECIMAL) {
            u32 val = 0;
            auto err = std::from_chars(l->text.data(), l->text.data() + l->text.size(), val, 10);
            if (err.ec != std::errc{} || err.ptr != l->text.data() + l->text.size()) {
                PARSE_ERR(&*l, "value too large");
                val = INT_MAX;
            }
            l = next_lexeme(l, end);
            return create<literal_expression>(val);
        } else if (l->type == lexeme_type::OCTAL) {
            u32 val = 0;
            auto err = std::from_chars(l->text.data(), l->text.data() + l->text.size(), val, 8);
            if (err.ec != std::errc{} || err.ptr != l->text.data() + l->text.size()) {
                PARSE_ERR(&*l, "value too large");
                val = INT_MAX;
            }
            l = next_lexeme(l, end);
            return create<literal_expression>(val);
        } else if (l->type == lexeme_type::HEXADECIMAL) {
            u32 val = 0;
            auto err = std::from_chars(l->text.data() + 2, l->text.data() + l->text.size(), val, 16); // skip 0x/0X
            if (err.ec != std::errc{} || err.ptr != l->text.data() + l->text.size()) {
                PARSE_ERR(&*l, "value too large");
                val = INT_MAX;
            }
            l = next_lexeme(l, end);
            return create<literal_expression>(val);
        } else if (l->type == lexeme_type::OPEN_PAREN) {
            l = next_lexeme(l, end);
            auto result = or_expr(l, end);
            if (l->type != lexeme_type::CLOSE_PAREN) {
                PARSE_ERR(&*l, "missing )");
                // probably fine to infer closing parentheses at the end
                return result;
            }
            l = next_lexeme(l, end);
            return result;
        }
        PARSE_ERR(&*l, "unexpected token");
        return nullptr;
    }

private:
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        if (_chunks == nullptr || (_chunks->data.data() + _chunks->data.size() - sizeof(T) < _chunks->head)) {
            auto c = new chunk{};
            c->head = c->data.data();
            c->next = _chunks;
            _chunks = c;
        }

        T* result = new(_chunks->head) T(this, std::forward<Args>(args)...);
        _chunks->head += sizeof(T);
        return result;
    }

    struct chunk {
        std::array<std::byte, 65536> data{};
        std::byte* head{};
        chunk* next{};
    };
    chunk* _chunks{};
    preprocessor* _preprocessor{};

#undef PARSE_ERR
};

preprocessor::preprocessor(
    std::ostream& out,
    file_service* fserv,
    std::vector<define> defines
) :
    _out(&out),
    _fserv(fserv),
    _expr_parser(std::make_unique<expression_parser>(this)),
    _if_depth(0),
    _erasing_depth(0),
    _else_seen() {
    for (auto&& def : defines) {
        _defines.emplace(def.name.text, def);
    }
    _else_seen.push_back(true);
}

// need this here to avoid having to define expression_parser in the header
preprocessor::~preprocessor() = default;

bool preprocessor::preprocess_file(std::string_view in, std::string_view cwd) {
    std::vector<std::string> files;

    auto fcont = _fserv->resolve_load(cwd, in);
    auto l = _lexemes.begin();
    auto end = _lexemes.end();
    auto dir_start = l;
    auto dir_id = l;
    auto include_content = l;
    auto define_name = l;

    if (fcont.begin == nullptr)
        return false;

#define PP_ERR(MSG) error(&*l, MSG_DEBUG "error: "   MSG)
#define PP_WARN(MSG) warn(&*l, MSG_DEBUG "warning: " MSG)

    file:
    {
        files.push_back(fcont.file);
        auto lex_result = lexer{*(files.rbegin())}.run(fcont.begin, fcont.end);
        if (lex_result.errors.size() > 0) {
            for (auto&& e : lex_result.errors) {
                _errors.push_back(std::format("{}({},{}): {}\n", *(files.rbegin()), e.line, e.line_offset, e.explanation));
            }
            return false;
        }

        auto l2 = lex_result.lexemes.begin();
        _lexemes.splice(l, lex_result.lexemes);
        l = l2;
    }

dispatch:
    if (l == end) {
        goto eof;
    }
    switch (l->type) {
        case lexeme_type::HASH:
            dir_start = l;
            goto directive;

        case lexeme_type::LINE_END:
        case lexeme_type::COMMENT:
        case lexeme_type::WHITESPACE:
            ++l;
            goto dispatch;

        default:
            while (l != end && l->type != lexeme_type::LINE_END) {
                if (_erasing_depth > 0) {
                    _lexemes.erase_and_dispose(l++, lexeme::disposer{});
                } else {
                    l = replace_identifier(l);
                }
            }
            goto dispatch;
    }

directive:
    l = next_lexeme(l, end);
    if (l == end) {
        goto eof;
    } else if (l->type == lexeme_type::IDENTIFIER) {
        dir_id = l;
        if (l->text == dir_else) {
            goto else_directive;
        } else if (l->text == dir_elif) {
            goto elif_directive;
        } else if (l->text == dir_endif) {
            goto endif_directive;
        } else if (_erasing_depth > 0) {
            while (l != end && l->type != lexeme_type::LINE_END) {
                ++l;
            }
            remove(dir_start, l);
            goto dispatch;
        } else if (l->text == dir_if) {
            goto if_directive;
        } else if (l->text == dir_ifdef) {
            goto ifdef_directive;
        } else if (l->text == dir_undef) {
            goto undef_directive;
        } else if (l->text == dir_define) {
            goto define_directive;
        } else if (l->text == dir_ifndef) {
            goto ifndef_directive;
        } else if (l->text == dir_include) {
            goto include_directive;
        }
        goto other;
    }
    goto dispatch;

other:
    if (l == end) {
        goto eof;
    } else if (l->type == lexeme_type::LINE_END) {
        ++l;
        goto dispatch;
    } else {
        auto it = l++;
        replace_identifier(it);
        goto other;
    }

else_directive:
    if (_if_depth == 0) {
        PP_ERR("spurious else");
    } else if (_else_seen[_if_depth]) {
        PP_ERR("second else");
    } else {
        _else_seen[_if_depth] = true;
        if (_erasing_depth > 0) {
            _erasing_depth = 0;
        } else {
            _erasing_depth = _if_depth;
        }
        while (++l != end && l->type != lexeme_type::LINE_END) {
            if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
                PP_ERR("unexpected token");
            }
        }
        remove(dir_start, l);
    }
    goto dispatch;

elif_directive:
    if (_if_depth == 0) {
        PP_ERR("spurious elif");
    } else if (_else_seen[_if_depth]) {
        PP_ERR("elif after else");
    } else {
        expression_parser parser{this};
        auto expr_begin = ++l;
        auto expr_end = seek_line_end(l, end);
        expression* expr = parser.parse(expr_begin, expr_end);
        if (expr) {
            value v = expr->evaluate();
            _erasing_depth = v.int_value() ? 0 : _if_depth;
        } else {
            PP_ERR("error parsing expression");
            _erasing_depth = _if_depth;
        }
        remove(dir_start, expr_end);
        l = expr_end;
    }
    goto dispatch;

endif_directive:
    if (_if_depth > 0) {
        _else_seen[_if_depth] = false;
        if (_erasing_depth > 0) {
            _erasing_depth = 0;
        }
        _if_depth -= 1;
        while (++l != end && l->type != lexeme_type::LINE_END) {
            if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
                PP_ERR("unexpected token");
            }
        }
        remove(dir_start, l);
    } else {
        PP_ERR("spurious endif");
    }
    goto dispatch;

if_directive:
    {
        expression_parser parser{this};
        auto expr_begin = ++l;
        auto expr_end = seek_line_end(l, end);
        expression* expr = parser.parse(expr_begin, expr_end);
        _if_depth += 1;
        if (_if_depth >= _else_seen.size())
            _else_seen.push_back(false);
        if (expr) {
            value v = expr->evaluate();
            _erasing_depth = v.int_value() ? 0 : _if_depth;
        } else {
            PP_ERR("error parsing expression");
            _erasing_depth = _if_depth;
        }
        remove(dir_start, expr_end);
        l = expr_end;
    }
    goto dispatch;

ifdef_directive:
    l = next_lexeme(l, end);
    if (l == end) {
        PP_ERR("missing define");
        goto eof;
    } else if (l->type == lexeme_type::IDENTIFIER) {
        define_name = l;
        goto ifdef_define;
    } else {
        PP_ERR("unexpected token");
        goto other;
    }

ifdef_define:
    {
        while (++l != end && l->type != lexeme_type::LINE_END) {
            if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
                PP_ERR("unexpected token");
            }
        }
        _if_depth += 1;
        if (_if_depth >= _else_seen.size())
            _else_seen.push_back(false);
        if (is_defined(define_name->text) && _erasing_depth <= 0) {
            _erasing_depth = _if_depth;
        }

        remove(dir_start, l);
    }
    goto dispatch;

undef_directive:
    l = next_lexeme(l, end);
    if (l == end) {
        PP_ERR("unexpected EOF");
        goto eof;
    } else if (l->type == lexeme_type::IDENTIFIER) {
        define_name = l;
        goto undef_define;
    } else {
        PP_ERR("unexpected token");
        goto other;
    }

undef_define:
    {
        auto def = _defines.find(define_name->text);
        if (def == _defines.end()) {
            PP_ERR("macro not defined");
        }
        _defines.erase(def);
    }
    while (++l != end && l->type != lexeme_type::LINE_END) {
        if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
            PP_ERR("unexpected token");
        }
    }
    remove(dir_start, l);
    goto dispatch;


define_directive:
    l = next_lexeme(l, end);
    if (l == end) {
        PP_ERR("unexpected EOF");
        goto eof;
    } else if (l->type == lexeme_type::IDENTIFIER) {
        define_name = l;
        goto define_parameters;
    } else {
        PP_ERR("expected name for define");
        goto other;
    }

define_parameters:
    if (++l != end && l->type == lexeme_type::OPEN_PAREN) {
        PP_ERR("parameterized not yet supported");
    } else {
        std::vector<lexeme> c;
        for (l = next_lexeme(define_name, end); l != end && l->type != lexeme_type::LINE_END; l = next_lexeme(l, end)) {
            c.push_back(*l);
        }
        _defines.emplace(define_name->text, define{*define_name, std::move(c)});
        remove(dir_start, l);
    }
    goto dispatch;

ifndef_directive:
    l = next_lexeme(l, end);
    if (l == end) {
        PP_ERR("expected define");
        goto eof;
    } else if (l->type == lexeme_type::IDENTIFIER) {
        define_name = l;
        goto ifndef_define;
    } else {
        PP_ERR("unexpected token");
        goto other;
    }

ifndef_define:
    {
        while (++l != end && l->type != lexeme_type::LINE_END) {
            if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
                PP_ERR("unexpected token");
            }
        }
        _if_depth += 1;
        if (_if_depth >= _else_seen.size())
            _else_seen.push_back(false);
        if (is_defined(define_name->text) && _erasing_depth <= 0) {
            _erasing_depth = _if_depth;
        }

        remove(dir_start, l);
    }
    goto dispatch;

include_directive:
    l = next_lexeme(l, end);
    if (l == end) {
        PP_ERR("unexpected EOF");
        goto eof;
    } else if (l->type == lexeme_type::STRING) {
        include_content = l;
        goto include_rel;
    } else if (l->type == lexeme_type::LT) {
        include_content = l;
        goto include_dir;
    } else {
        PP_ERR("unexpected token");
        goto other;
    }

include_rel:
    include_content->type = lexeme_type::INCLUDE_STRING;
    while (++l != end && l->type != lexeme_type::LINE_END) {
        if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
            PP_ERR("unexpected token");
        }
    }
    fcont = _fserv->resolve_load(files[0], include_content->text.substr(1, include_content->text.size() - 2));
    if (fcont.begin) {
        remove(dir_start, l);
        goto file;
    }
    PP_ERR("could not find included file");
    goto dispatch;

include_dir:
    if (++l == end) {
        PP_ERR("unexpected EOF");
        goto eof;
    } else {
        switch (l->type) {
            case lexeme_type::LINE_END:
                PP_ERR("unclosed include path");
                goto other;

            case lexeme_type::GT:
            {
                auto l2 = _lexemes.insert(include_content, *create_lexeme(
                    include_content->file_path,
                    lexeme_type::INCLUDE_STRING,
                    include_content->line,
                    include_content->line_offset,
                    i32(l->text.end() - include_content->text.begin()),
                    std::string_view{include_content->text.begin(), l->text.end()}
                ));
                remove(include_content, ++l);
                include_content = l2;
            }

            while (l != end && l->type != lexeme_type::LINE_END) {
                if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
                    PP_ERR("unexpected tokens");
                }
                ++l;
            }
            goto include_file;

            default:
                goto include_dir;
        }
    }

include_file:
    fcont = _fserv->resolve_load("", include_content->text.substr(1, include_content->text.size() - 2));
    if (fcont.begin) {
        remove(dir_start, l);
        goto file;
    }
    PP_ERR("could not find included file");
    goto dispatch;

eof:

    if (_errors.size() > 0) {
        return false;
    } else {
        auto cur = _lexemes.begin();
        auto next = cur;
        next++;

        while (next != _lexemes.end()) {
            cur->write_to(*_out, *next);
            cur = next++;
        }
        cur->write_to(*_out);
        return true;
    }

#undef PP_WARN
#undef PP_ERR
}

lex_iter preprocessor::replace_identifier(lex_iter id_lex) {
    if (id_lex->type == lexeme_type::META_USED_DEFINE_POP) {
        _used_defines.pop_back();
        _lexemes.erase_and_dispose(id_lex++, lexeme::disposer{});
        return id_lex;
    }
    if (id_lex->type != lexeme_type::IDENTIFIER)
        return ++id_lex;

    auto r = _defines.find(id_lex->text);
    if (r != _defines.end() &&
        r->second.has_parameters == false &&
        std::find(_used_defines.begin(), _used_defines.end(), &r->second) == _used_defines.end()
    ) {
        auto ins_iter = id_lex;
        ++ins_iter;
        _used_defines.push_back(&r->second);
        for (auto&& c : r->second.content)
            _lexemes.insert(ins_iter, *create_lexeme(c));

        _lexemes.insert(ins_iter, *create_lexeme(
            id_lex->file_path,
            lexeme_type::META_USED_DEFINE_POP,
            id_lex->line,
            id_lex->line_offset,
            id_lex->src_length,
            ""
        ));

        auto rem_iter = id_lex++;
        _lexemes.erase_and_dispose(rem_iter, lexeme::disposer{});
        return id_lex;
    } else {
        return ++id_lex;
    }
}

lex_iter preprocessor::insert(lex_iter where, lexeme* l) {
    if (l == nullptr)
        return {};
    return _lexemes.insert(where, *l);
}

void preprocessor::remove(lex_iter beg, lex_iter end) {
    _lexemes.erase_and_dispose(beg, end, lexeme::disposer{});
}

bool preprocessor::is_defined(std::string_view name) {
    return _defines.find(name) != _defines.end();
}

void preprocessor::error(lexeme* l, const char* msg) {
    _errors.push_back(std::format("{}({},{}): {}\n", l->file_path, l->line, l->line_offset, msg));
}

void preprocessor::warn(lexeme* l, const char* msg) {
    _warns.push_back(std::format("{}({},{}): {}\n", l->file_path, l->line, l->line_offset, msg));
}

