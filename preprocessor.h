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
    using is_transparent = std::true_type;

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

    std::size_t operator()(const lexeme& l) const {
        return operator()(l.text);
    }
};

struct define {
    lexeme name;
    std::vector<lexeme> content;
    bool has_parameters = false;
    std::vector<lexeme> parameters;

    define(lexeme name, std::vector<lexeme> content) :
        name(name), content(content), has_parameters(false), parameters()
    {}

    define(lexeme name, std::vector<lexeme> content, std::vector<lexeme> parameters) :
        name(name),
        content(content),
        has_parameters(true),
        parameters(parameters)
    {}
};

class preprocessor {
public:
    explicit preprocessor(
        std::ostream& out,
        std::vector<fs::path> include_dirs,
        std::vector<define> defines
    );
    ~preprocessor();

    bool preprocess_file(fs::path in);
    lex_iter replace_identifier(lex_iter id);
    lex_iter insert(lex_iter where, lexeme* l);
    void remove(lex_iter beg, lex_iter end);
    bool is_defined(std::string_view name);
    void error(lexeme* l, const char* msg);

private:
    std::ostream* _out;
    std::vector<fs::path> _include_dirs;
    std::unique_ptr<struct expression_parser> _expr_parser;
    
    lexeme_list _lexemes;
    phmap::flat_hash_map<std::string, define, string_hash> _defines;
    std::vector<define*> _used_defines;
    std::vector<std::string> _errors;

    int _if_depth;
    int _erasing_depth;
    std::vector<bool> _else_seen;
};
