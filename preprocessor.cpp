#include "preprocessor.h"
#include "lexer.h"
#include "scope_guard.h"

#include <cctype>
#include <format>

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

// returns the next useful lexeme
// skips over whitespace and comment lexemes
// line endings are not counted as whitespace
template<typename iterator>
iterator next_lexeme(iterator l, iterator end) {
    while (++l != end && (l->type == lexeme_type::WHITESPACE || l->type == lexeme_type::COMMENT));
    return l;
}

bool preprocessor::preprocess_file(fs::path in) {
    if (fs::exists(in) == false || fs::is_directory(in))
        return false;

    std::vector<std::string> files;
    std::vector<std::vector<char>> content;
    std::vector<std::string> errors;
    
    fs::path path = in;
    auto l = _lexemes.begin();
    auto end = _lexemes.end();
    auto dir_start = l;
    auto dir_id = l;
    auto include_content = l;
    auto define_name = l;
    bool first_file = true;

#define PP_ERR(MSG) \
    do { \
        errors.push_back(std::format("{}:{} {}\n", l->file_path, l->line, MSG));\
    } while(0)

file:
    {
        auto&& c = content.emplace_back(std::size_t(fs::file_size(path)));
        std::fstream f{path.c_str(), std::ios::in | std::ios::binary};
        f.read(c.data(), c.size());

        files.push_back(fs::absolute(path).string());
        auto lex_result = lexer{*(files.rbegin())}.run(&*c.begin(), (&*c.begin()) + c.size());
        if (lex_result.errors.size() > 0) {
            for (auto&& e : lex_result.errors) {
                errors.push_back(std::format("{}:{} {}\n", *(files.rbegin()), e.line, e.explanation));
            }
            return false;
        }

        auto l2 = l++;
        _lexemes.splice(l, lex_result.lexemes);
        if (first_file) {
            first_file = false;
            l = _lexemes.begin();
        } else {
            l = l2;
        }
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
        PP_ERR("second else");
    } else {
        _else_seen[_if_depth] = true;
        if (_erasing_depth > 0) {
            _erasing_depth = 0;
        } else {
            _erasing_depth = _if_depth;
        }
        while (l != end && l->type == lexeme_type::LINE_END) {
            if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
                PP_ERR("unexpected token");
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
                PP_ERR("unexpected token");
            }
            ++l;
        }
        _lexemes.erase_and_dispose(dir_start, l, lexeme::disposer{});
    } else {
        PP_ERR("spurious endif");
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
        while (l != end && l->type == lexeme_type::LINE_END) {
            if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
                PP_ERR("unexpected token");
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
        auto def = _defines2.find(define_name->text);
        if (def == _defines2.end()) {
            PP_ERR("macro not defined");
        }
        _defines2.erase(def);
    }
    while (l != end && l->type == lexeme_type::LINE_END) {
        if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
            PP_ERR("unexpected token");
        }
        ++l;
    }
    _lexemes.erase_and_dispose(dir_start, l, lexeme::disposer{});
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
        _defines2.emplace(define_name->text, define2{*define_name, std::move(c)});
        _lexemes.erase_and_dispose(dir_start, l, lexeme::disposer{});
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
        while (l != end && l->type == lexeme_type::LINE_END) {
            if (l->type != lexeme_type::WHITESPACE && l->type != lexeme_type::COMMENT) {
                PP_ERR("unexpected token");
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
    path = in.remove_filename() / include_content->text.substr(1, include_content->text.size() - 2);
    if (fs::exists(path) && fs::is_directory(path) == false) {
        _lexemes.erase_and_dispose(dir_start, l, lexeme::disposer{});
        goto file;
    }
    goto include_file;

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
                    auto l2 = _lexemes.insert(include_content, *new lexeme{
                        include_content->file_path,
                        lexeme_type::INCLUDE_STRING,
                        include_content->line,
                        include_content->line_offset,
                        i32(l->text.end() - include_content->text.begin()),
                        std::string_view{include_content->text.begin(), l->text.end()}
                    });
                    _lexemes.erase_and_dispose(include_content, ++l, lexeme::disposer{});
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
    PP_ERR("could not find included file");
    goto dispatch;

eof:

    if (errors.size() > 0) {
        for (auto&& s : errors) {
            std::cout << s;
        }

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
}

void preprocessor::replace_identifier(lexeme* ident) {
    if (ident->type != lexeme_type::IDENTIFIER)
        return;

    auto r = _defines2.find(std::string{ident->text});
    if (r != _defines2.end() && r->second.has_parameters == false && _used_defines.contains(r->second.name.text) == false) {
        auto&& [uit, success] = _used_defines.emplace(r->second.name.text);
        auto iter = _lexemes.iterator_to(*ident);
        for (auto&& c : r->second.content) {
            auto l = new lexeme{c};
            replace_identifier(&*_lexemes.insert(iter, *l)); // recurse
        }
        _lexemes.erase_and_dispose(iter, lexeme::disposer{});
        _used_defines.erase(uit);
    }
}

