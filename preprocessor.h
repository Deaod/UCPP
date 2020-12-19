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
