#pragma once

#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>
#include <climits>
#include <bitset>

#include <filesystem>
namespace fs = std::filesystem;

#include <parallel_hashmap/phmap.h>

#define XXH_INLINE_ALL
#include <xxhash.h>

#include "lexer.h"

struct string_hash {
    std::size_t operator()(const std::string& s) const {
        if constexpr (sizeof(std::size_t) * CHAR_BIT == 32) {
            return std::size_t(XXH3_64bits(s.data(), s.size()));
        } else if constexpr (sizeof(std::size_t) * CHAR_BIT == 64) {
            return XXH3_64bits(s.data(), s.size());
        } else {
            throw std::exception("unsupported size of std::size_t");
        }
    }
    
    std::size_t operator()(const std::string_view& sv) const {
        if constexpr (sizeof(std::size_t) * CHAR_BIT == 32) {
            return std::size_t(XXH3_64bits(sv.data(), sv.size()));
        } else if constexpr (sizeof(std::size_t) * CHAR_BIT == 64) {
            return XXH3_64bits(sv.data(), sv.size());
        } else {
            throw std::exception("unsupported size of std::size_t");
        }
    }
};

struct define2 {
    lexeme* name;
    std::vector<lexeme*> content;
    bool has_parameters = false;
    std::vector<lexeme*> parameters;

    define2(lexeme* name, std::vector<lexeme*> content) :
        name(name), content(std::move(content)), has_parameters(false), parameters()
    {}

    define2(lexeme* name, std::vector<lexeme*> content, std::vector<lexeme*> parameters) :
        name(name),
        content(std::move(content)),
        has_parameters(true),
        parameters(std::move(parameters))
    {}
};

class preprocessor {
public:
    struct define {
        std::string name;
        std::string replacement;

        explicit define(std::string name) : name(name), replacement() {}
        explicit define(std::string name, std::string replacement) :
            name(name),
            replacement(replacement) {}
    };

    explicit preprocessor(
        std::ostream& out,
        std::vector<fs::path> include_dirs,
        std::vector<define> defines
    ) :
        _out(&out),
        _include_dirs(include_dirs),
        _defines(),
        _comment_state(comment_state::START),
        _if_depth(0),
        _erasing_depth(0),
        _else_seen()
    {
        for (auto&& def : defines) {
            _defines.emplace(def.name, def);
        }
        _else_seen.push_back(false);
    }

    bool preprocess_file(fs::path in);
    void replace_identifier(lexeme* ident);

    void remove_comments(char* buffer, size_t length);
    bool preprocess_file(std::istream& in);
    bool preprocess_line(std::string_view line);
    bool preprocess_directive(std::string_view directive, std::string_view args);

    bool preprocess_directive_include(std::string_view args);
    bool preprocess_directive_define(std::string_view args);
    bool preprocess_directive_undef(std::string_view args);
    bool preprocess_directive_ifdef(std::string_view args);
    bool preprocess_directive_ifndef(std::string_view args);
    bool preprocess_directive_else(std::string_view args);
    bool preprocess_directive_endif(std::string_view args);

    bool add_define(std::string_view name, std::string_view replacement);

    bool write_output(std::string_view line);
    bool replace_identifier(std::string_view ident);

private:
    std::ostream* _out;
    std::vector<fs::path> _include_dirs;
    
    boost::intrusive::list<lexeme> _lexemes;
    phmap::flat_hash_map<std::string_view, define2, string_hash> _defines2;
    phmap::flat_hash_set<std::string_view, string_hash> _used_defines;

    phmap::flat_hash_map<std::string, define, string_hash> _defines;
    enum class comment_state : int {
        START,
        STRING, ESCAPE,
        PRE_COMMENT,
        LINE_COMMENT,
        BLOCK_COMMENT,
        BLOCK_COMMENT_END
    } _comment_state;
    int _if_depth;
    int _erasing_depth;
    std::vector<bool> _else_seen;
};
