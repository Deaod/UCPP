#include "preprocessor.h"
#include "lexer.h"
#include "scope_guard.h"

#include <cctype>

constexpr const auto lf = "\n";

constexpr std::string_view dir_include{ "include" };
constexpr std::string_view dir_define{ "define" };
constexpr std::string_view dir_undef{ "undef" };
constexpr std::string_view dir_if{ "if" };
constexpr std::string_view dir_elif{ "elif" };
constexpr std::string_view dir_else{ "else" };
constexpr std::string_view dir_endif{ "endif" };
constexpr std::string_view dir_ifdef{ "ifdef" };
constexpr std::string_view dir_ifndef{ "ifndef" };

static std::string_view trim_left(std::string_view sv) {
    size_t index = 0;
    while (index < sv.size() && std::isspace(sv[index]))
        index += 1;

    return sv.substr(index);
}

static std::string_view trim_right(std::string_view sv) {
    size_t index = sv.size();
    while (index > 0 && std::isspace(sv[index - 1]))
        index -= 1;

    return sv.substr(0, index);
}

static std::string_view trim(std::string_view sv) {
    return trim_right(trim_left(sv));
}

template<typename iterator>
iterator next_lexeme(iterator l, iterator end) {
    while (++l != end && (l->type == lexeme_type::WHITESPACE || l->type == lexeme_type::COMMENT));
    return l;
}

bool preprocessor::preprocess_file(fs::path in) {
    if (fs::exists(in) == false || fs::is_directory(in))
        return false;

    std::vector<std::vector<char>> content;
    
    fs::path path = in;
    auto l = _lexemes.begin();
    auto end = _lexemes.end();
    auto dir_start = l;
    auto dir_id = l;
    auto include_content = l;
    auto define_name = l;

file:
    {
        auto&& c = content.emplace_back(std::size_t(fs::file_size(path)));
        std::fstream f{path.c_str(), std::ios::in | std::ios::binary};
        f.read(c.data(), c.size());

        auto lex_result = lexer{}.run(c);
        if (lex_result.errors.size() > 0) {
            // print errors
            return false;
        }

        auto l2 = l++;
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
            do {
                auto it = l++;
                if (_erasing_depth > 0) {
                    _lexemes.erase_and_dispose(it, lexeme::disposer{});
                }
                replace_identifier(&*it);
            } while (l != end && l->type != lexeme_type::LINE_END);
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

        } else if (l->text == dir_endif) {
            goto endif_directive;
        } else if (_erasing_depth > 0) {
            while (l != end && l->type != lexeme_type::LINE_END) {
                ++l;
            }
            _lexemes.erase_and_dispose(dir_start, l, lexeme::disposer{});
            goto dispatch;
        } else if (l->text == dir_if) {

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
        replace_identifier(&*it);
        goto other;
    }

else_directive:
    if (_else_seen[_if_depth]) {
        // TODO error
        // second else
    } else {
        _else_seen[_if_depth] = true;
        if (_erasing_depth > 0) {
            _erasing_depth = 0;
        } else {
            _erasing_depth = _if_depth;
        }
        while (l != end && l->type == lexeme_type::LINE_END) {
            if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
                // TODO error
                // unexpected token
            }
            ++l;
        }
        _lexemes.erase_and_dispose(dir_start, l, lexeme::disposer{});
    }
    goto dispatch;

endif_directive:
    if (_if_depth > 0) {
        _else_seen[_if_depth] = false;
        if (_erasing_depth > 0) {
            _erasing_depth = 0;
        }
        _if_depth -= 1;
        while (l != end && l->type == lexeme_type::LINE_END) {
            if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
                // TODO error
                // unexpected token
            }
            ++l;
        }
        _lexemes.erase_and_dispose(dir_start, l, lexeme::disposer{});
    } else {
        // TODO error
        // spurious endif
    }
    goto dispatch;

ifdef_directive:
    l = next_lexeme(l, end);
    if (l == end) {
        // TODO error
        goto eof;
    } else if (l->type == lexeme_type::IDENTIFIER) {
        define_name = l;
        goto ifdef_define;
    } else {
        // TODO error
        goto other;
    }

ifdef_define:
    {
        while (l != end && l->type == lexeme_type::LINE_END) {
            if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
                // TODO error
            }
            ++l;
        }
        auto it = _defines2.find(define_name->text);
        _if_depth += 1;
        if (_if_depth >= _else_seen.size())
            _else_seen.push_back(false);
        if (it == _defines2.end() && _erasing_depth <= 0) {
            _erasing_depth = _if_depth;
        }

        _lexemes.erase_and_dispose(dir_start, l, lexeme::disposer{});
    }
    goto dispatch;

undef_directive:
    l = next_lexeme(l, end);
    if (l == end) {
        // TODO error
        // unexpected EOF
        goto eof;
    } else if (l->type == lexeme_type::IDENTIFIER) {
        define_name = l;
        goto undef_define;
    } else {
        // TODO error
        // unexpected token
        goto other;
    }

undef_define:
    {
        auto def = _defines2.find(define_name->text);
        if (def == _defines2.end()) {
            // TODO error
            // macro not defined
        }
        _defines2.erase(def);
    }
    while (l != end && l->type == lexeme_type::LINE_END) {
        if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
            // TODO error
            // unexpected token
        }
        ++l;
    }
    _lexemes.erase_and_dispose(dir_start, l, lexeme::disposer{});
    goto dispatch;


define_directive:
    l = next_lexeme(l, end);
    if (l == end) {
        // TODO error
        // unexpected EOF
        goto eof;
    } else if (l->type == lexeme_type::IDENTIFIER) {
        define_name = l;
        goto define_parameters;
    } else {
        // TODO error
        // expected name for define
        goto other;
    }

define_parameters:
    if (++l != end && l->type == lexeme_type::OPEN_PAREN) {
        // parameterized not yet supported

    } else {
        std::vector<lexeme*> c;
        for (l = next_lexeme(define_name, end); l != end && l->type != lexeme_type::LINE_END; l = next_lexeme(l, end)) {
            c.push_back(&*l);
        }
        _lexemes.erase_and_dispose(dir_start, l, lexeme::disposer{});
        _defines2.emplace(define_name->text, define2{&*define_name, std::move(c)});
    }
    goto dispatch;

ifndef_directive:
    l = next_lexeme(l, end);
    if (l == end) {
        // TODO error
        // unexpected EOF
        goto eof;
    } else if (l->type == lexeme_type::IDENTIFIER) {
        define_name = l;
        goto ifndef_define;
    } else {
        // TODO error
        goto other;
    }

ifndef_define:
    {
        while (l != end && l->type == lexeme_type::LINE_END) {
            if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
                // TODO error
            }
            ++l;
        }
        auto it = _defines2.find(define_name->text);
        _if_depth += 1;
        if (_if_depth >= _else_seen.size())
            _else_seen.push_back(false);
        if (it != _defines2.end() && _erasing_depth <= 0) {
            _erasing_depth = _if_depth;
        }

        _lexemes.erase_and_dispose(dir_start, l, lexeme::disposer{});
    }
    goto dispatch;

include_directive:
    l = next_lexeme(l, end);
    if (l == end) {
        // TODO error
        // unexpected EOF
        goto eof;
    } else if (l->type == lexeme_type::STRING) {
        include_content = l;
        goto include_rel;
    } else if (l->type == lexeme_type::LT) {
        include_content = _lexemes.insert(l, *new lexeme{
            lexeme_type::INCLUDE_STRING,
            l->line,
            l->line_offset,
            l->src_length,
            l->text
            });
        goto include_dir;
    } else {
        // TODO error
        goto other;
    }

include_rel:
    include_content->type = lexeme_type::INCLUDE_STRING;
    while (++l != end && l->type != lexeme_type::LINE_END) {
        if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
            // TODO error
        }
    }
    path = in.remove_filename() / include_content->text.substr(1, include_content->text.size() - 2);
    if (fs::exists(path) && fs::is_directory(path) == false) {
        _lexemes.erase_and_dispose(dir_start, l, lexeme::disposer{});
        goto file;
    }
    goto include_file;

include_dir:
    if (++l == end) {
        // TODO error
        // unexpected EOF
        goto eof;
    } else {
        switch (l->type) {
            case lexeme_type::LINE_END:
                // TODO error
                // unclosed include path
                goto other;

            case lexeme_type::COMMENT:
                include_content->src_length += l->src_length;
                include_content->text += " ";
                goto include_dir;

            case lexeme_type::GT:
                include_content->src_length += l->src_length;
                include_content->text += l->text;
                {
                    auto l2 = include_content;
                    _lexemes.erase_and_dispose(++l2, ++l, lexeme::disposer{});
                }

                while (l != end && l->type != lexeme_type::LINE_END) {
                    if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
                        // TODO error
                        // unexpected tokens
                    }
                    ++l;
                }
                goto include_file;

            default:
                include_content->src_length += l->src_length;
                include_content->text += l->text;
                goto include_dir;
        }
    }

include_file:
    for (auto&& dir : _include_dirs) {
        path = dir / include_content->text.substr(1, include_content->text.size() - 2);
        if (fs::exists(path) && fs::is_directory(path) == false) {
            _lexemes.erase_and_dispose(dir_start, l, lexeme::disposer{});
            break;
        }
    }
    if (fs::exists(path) && fs::is_directory(path) == false) {
        goto file;
    }
    // TODO error
    // could not find included file
    goto dispatch;

eof:

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

void preprocessor::replace_identifier(lexeme* ident) {
    if (ident->type != lexeme_type::IDENTIFIER)
        return;

    auto r = _defines2.find(ident->text);
    if (r != _defines2.end() && r->second.has_parameters == false && _used_defines.contains(r->second.name->text) == false) {
        auto&& [uit, success] = _used_defines.emplace(r->second.name->text);
        auto iter = _lexemes.iterator_to(*ident);
        for (auto&& c : r->second.content) {
            auto l = new lexeme{*c};
            replace_identifier(&*_lexemes.insert(iter, *l)); // recurse
        }
        _lexemes.erase_and_dispose(iter, lexeme::disposer{});
        _used_defines.erase(uit);
    }
}

