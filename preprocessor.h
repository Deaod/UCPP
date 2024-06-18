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

struct define2 {
    lexeme name;
    std::vector<lexeme> content;
    bool has_parameters = false;
    std::vector<lexeme> parameters;

    define2(lexeme name, std::vector<lexeme> content) :
        name(name), content(content), has_parameters(false), parameters()
    {}

    define2(lexeme name, std::vector<lexeme> content, std::vector<lexeme> parameters) :
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
        std::vector<define2> defines
    ) :
        _out(&out),
        _include_dirs(include_dirs),
        _if_depth(0),
        _erasing_depth(0),
        _else_seen()
    {
        for (auto&& def : defines) {
            _defines2.emplace(def.name.text, def);
        }
        _else_seen.push_back(false);
    }

    bool preprocess_file(fs::path in);
    void replace_identifier(lexeme* ident);

private:
    std::ostream* _out;
    std::vector<fs::path> _include_dirs;
    
    boost::intrusive::list<lexeme> _lexemes;
    phmap::flat_hash_map<std::string, define2, string_hash> _defines2;
    phmap::flat_hash_set<std::string, string_hash> _used_defines;

    int _if_depth;
    int _erasing_depth;
    std::vector<bool> _else_seen;
};
