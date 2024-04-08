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

void preprocessor::remove_comments(char* buffer, size_t length) {
    comment_state s = _comment_state;

    for (size_t i = 0; i < length; i += 1) {
        switch (s) {
            case comment_state::START:
                if (buffer[i] == '/') 
                    s = comment_state::PRE_COMMENT;
                 else if (buffer[i] == '"')
                    s = comment_state::STRING;
                break;

            case comment_state::STRING:
                if (buffer[i] == '\\')
                    s = comment_state::ESCAPE;
                 else if (buffer[i] == '"')
                    s = comment_state::START;
                break;

            case comment_state::ESCAPE:
                s = comment_state::STRING;
                break;

            case comment_state::PRE_COMMENT:
                if (buffer[i] == '*') {
                    buffer[i - 1] = ' ';
                    buffer[i] = ' ';
                    s = comment_state::BLOCK_COMMENT;
                } else if (buffer[i] == '/') {
                    buffer[i - 1] = ' ';
                    buffer[i] = ' ';
                    s = comment_state::LINE_COMMENT;
                } else {
                    s = comment_state::START;
                }
                break;

            case comment_state::LINE_COMMENT:
                if (buffer[i] == '\n')
                    s = comment_state::START;

                if (std::isspace(buffer[i]) == false)
                    buffer[i] = ' ';
                break;

            case comment_state::BLOCK_COMMENT:
                if (buffer[i] == '*')
                    s = comment_state::BLOCK_COMMENT_END;

                if (std::isspace(buffer[i]) == false)
                    buffer[i] = ' ';
                break;

            case comment_state::BLOCK_COMMENT_END:
                if (buffer[i] == '/')
                    s = comment_state::START;

                if (std::isspace(buffer[i]) == false)
                    buffer[i] = ' ';
                break;
        }
    }

    _comment_state = s;
}

bool preprocessor::preprocess_file(std::istream& in) {
    char buffer[4096];

    while (in.good()) {
        auto old_pos = in.tellg();
        in.getline(buffer, sizeof(buffer), '\n');
        auto new_pos = [&]() {
            if (in.eof()) {
                return old_pos + std::streamoff(strlen(buffer));
            } else {
                return in.tellg();
            }
        }();
        auto count = new_pos - old_pos - 1;

        if (in.eof())
            count += 1;

        remove_comments(buffer, size_t(count));
        preprocess_line({ buffer, size_t(count) });

        if (in.eof() == false) {
            char lf = '\n';
            remove_comments(&lf, 1);
            _out->put(lf);
        }

        if (in.fail() && count == sizeof(buffer) - 1) {
            // not enough space
            in.clear(in.rdstate() & ~std::ios::failbit);
            in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cerr << "Line longer than 4095 characters" << lf;
        }
    }

    if (in.eof() == false) {
        std::cerr << "Finished processing file before EOF" << lf;
    }

    switch(_comment_state) {
        case comment_state::LINE_COMMENT:
        case comment_state::BLOCK_COMMENT:
        case comment_state::BLOCK_COMMENT_END:
            std::cerr << "EOF within comment" << lf;
            break;
    }

    return true;
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

bool preprocessor::preprocess_line(std::string_view line) {
    enum class state {
        START, PRE_DIRECTIVE, DIRECTIVE
    };

    state s = state::START;
    std::string_view::iterator dbegin;
    std::string_view::iterator dend;

    for (auto it = line.begin(), end = line.end(); it != end; ++it) {
        switch (s) {
            case state::START:
                if (std::isspace(*it)) {
                    // do nothing
                } else if (*it == '#') {
                    s = state::PRE_DIRECTIVE;
                } else {
                    goto output;
                }
                break;

            case state::PRE_DIRECTIVE:
                if (std::isspace(*it)) {
                    // do nothing;
                } else if (std::isalpha(*it) || *it == '_') {
                    dbegin = it;
                    s = state::DIRECTIVE;
                } else {
                    goto output;
                }
                break;

            case state::DIRECTIVE:
                if (std::isspace(*it)) {
                    dend = it;
                    auto directive = line.substr(dbegin - line.begin(), dend - dbegin);
                    auto args = line.substr(dend-line.begin(), end - dend);
                    if (preprocess_directive(directive, args) == false)
                        goto output;
                    else
                        return true;
                } else if (std::isalnum(*it) || *it == '_') {
                    // do nothing
                } else {
                    goto output;
                }
                break;
        }
    }

output:
    write_output(line);

    return false;
}

bool preprocessor::preprocess_directive(std::string_view directive, std::string_view args) {
    if (directive == dir_include) {
        return preprocess_directive_include(args);
    } else if (directive == dir_define) {
        return preprocess_directive_define(args);
    } else if (directive == dir_undef) {
        return preprocess_directive_undef(args);
    } else if (directive == dir_if) {
        
    } else if (directive == dir_elif) {

    } else if (directive == dir_else) {
        return preprocess_directive_else(args);
    } else if (directive == dir_endif) {
        return preprocess_directive_endif(args);
    } else if (directive == dir_ifdef) {
        return preprocess_directive_ifdef(args);
    } else if (directive == dir_ifndef) {
        return preprocess_directive_ifndef(args);
    }

    return false;
}

bool preprocessor::preprocess_directive_include(std::string_view args) {
    enum class state {
        START, REL_PATH, INCLDIR_PATH
    };

    state s = state::START;
    size_t offset = 0;
    size_t count = 0;

    for (auto it = args.begin(), end = args.end(); it != end; ++it) {
        switch (s) {
            case state::START:
                if (std::isspace(*it)) {
                    // do nothing
                } else if (*it == '"') {
                    offset = size_t(it - args.begin() + 1);
                    s = state::REL_PATH;
                } else if (*it == '<') {
                    offset = size_t(it - args.begin() + 1);
                    s = state::INCLDIR_PATH;
                } else {
                    return false;
                }
                break;

            case state::REL_PATH:
                if (*it == '"') {
                    goto include;
                } else {
                    count += 1;
                }
                break;

            case state::INCLDIR_PATH:
                if (*it == '>') {
                    goto include;
                } else {
                    count += 1;
                }
                break;
        }
    }
    return false;

include:
    fs::path p{ args.substr(offset, count) };
    if (p.is_relative()) {
        for (auto&& dir : _include_dirs) {
            fs::path p2 = dir / p;
            if (fs::exists(p2) && fs::is_directory(p2) == false) {
                std::ifstream file{ p2.c_str(), std::ios::in | std::ios::binary };
                return preprocess_file(file);
            }
        }
        std::cerr << "\"" << p << "\" could not be resolved" << lf;
    } else {
        if (fs::exists(p) && fs::is_directory(p) == false) {
            std::ifstream file{ p.c_str(), std::ios::in | std::ios::binary };
            return preprocess_file(file);
        } else {
            std::cerr << "\"" << p << "\" is not a file" << lf;
        }
    }

    return false;
}

bool preprocessor::preprocess_directive_define(std::string_view args) {
    enum class state {
        START, IDENT
    };

    state s = state::START;
    size_t offset = 0;
    size_t count = 0;

    for (auto it = args.begin(), end = args.end(); it != end; ++it) {
        switch (s) {
            case state::START:
                if (std::isspace(*it)) {
                    // do nothing
                } else if (std::isalpha(*it) || *it == '_') {
                    offset = size_t(it - args.begin());
                    count = 1;
                    s = state::IDENT;
                } else {
                    return false;
                }
                break;

            case state::IDENT:
                if (std::isalnum(*it) || *it == '_') {
                    count += 1;
                } else if (std::isspace(*it)) {
                    return add_define(args.substr(offset, count), args.substr(offset + count));
                } else {
                    return false;
                }
                break;
        }
    }
    return false;
}

bool preprocessor::preprocess_directive_undef(std::string_view args) {
    return (_defines.erase(std::string(trim(args))) != 0);
}

bool preprocessor::preprocess_directive_ifdef(std::string_view args) {
    auto it = _defines.find(std::string(trim(args)));
    _if_depth += 1;
    if (_if_depth >= _else_seen.size())
        _else_seen.push_back(false);
    if (it != _defines.end() && _erasing_depth <= 0) {
        _erasing_depth = _if_depth;
    }
    return true;
}

bool preprocessor::preprocess_directive_ifndef(std::string_view args) {
    auto it = _defines.find(std::string(trim(args)));
    _if_depth += 1;
    if (_if_depth >= _else_seen.size())
        _else_seen.push_back(false);
    if (it == _defines.end() && _erasing_depth <= 0) {
        _erasing_depth = _if_depth;
    }
    return true;
}

bool preprocessor::preprocess_directive_else(std::string_view args) {
    if (_if_depth <= 0 || _else_seen[_if_depth] == true) {
        std::cerr << "unexpected else directive" << lf;
        return false;
    }

    if (_if_depth == _erasing_depth) {
        _erasing_depth = 0;
    } else if (_erasing_depth == 0) {
        _erasing_depth = _if_depth;
    }
    _else_seen[_if_depth] = true;
    return true;
}

bool preprocessor::preprocess_directive_endif(std::string_view args) {
    if (_if_depth > 0) {
        if (_erasing_depth == _if_depth)
            _erasing_depth = 0;
        _else_seen[_if_depth] = false;
        _if_depth -= 1;
        return true;
    }
    std::cerr << "unexpected endif directive" << lf;
    return false;
}

bool preprocessor::add_define(std::string_view name, std::string_view replacement) {
    replacement = trim(replacement);
    define d{ std::string(name), std::string(replacement) };
    auto&& old = _defines.emplace(std::string(name), std::move(d));
    if (old.second) {
        std::cerr << old.first->first << " redefined" << lf;
        return false;
    }
    return true;
}

bool preprocessor::write_output(std::string_view line) {
    if (_erasing_depth > 0) return true;

    enum class state {
        START, STRING, STRING_ESCAPE, NAME, NAME_ESCAPE, IDENT
    };

    state s = state::START;
    size_t offset = 0;
    size_t count = 0;

    for (auto it = line.begin(), end = line.end(); it != end; ++it) {
        switch (s) {
            case state::START:
                if (std::isalpha(*it) || *it == '_') {
                    offset = size_t(it - line.begin());
                    count = 1;
                    s = state::IDENT;
                } else  {
                    if (*it == '"') {
                        s = state::STRING;
                    } else if (*it == '\'') {
                        s = state::NAME;
                    }
                    _out->put(*it);
                }
                break;

            case state::IDENT:
                if (std::isalnum(*it) || *it == '_') {
                    count += 1;
                } else {
                    s = state::START;
                    if (replace_identifier(line.substr(offset, count)) == false) {
                        _out->write(line.data() + offset, count);
                    }
                    _out->put(*it);
                }
                break;

            case state::STRING:
                if (*it == '\\')
                    s = state::STRING_ESCAPE;
                else if (*it == '"')
                    s = state::START;
                _out->put(*it);
                break;

            case state::STRING_ESCAPE:
                s = state::STRING;
                _out->put(*it);
                break;

            case state::NAME:
                if (*it == '\\')
                    s = state::NAME_ESCAPE;
                else if (*it == '\'')
                    s = state::START;
                _out->put(*it);
                break;

            case state::NAME_ESCAPE:
                s = state::NAME;
                _out->put(*it);
                break;
        }
    }
    return false;
}

bool preprocessor::replace_identifier(std::string_view ident) {
    auto def = _defines.find(std::string(ident));
    if (def == _defines.end())
        return false;

    _out->write(def->second.replacement.data(), def->second.replacement.size());
    return true;
}
