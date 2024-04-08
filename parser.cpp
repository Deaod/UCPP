#include "types.h"
#include "scope_guard.h"

#include <expected>
#include <filesystem>
namespace fs = std::filesystem;

struct error {

};

template<typename T>
struct element {
    T elem;
    std::string_view source_name;
    i64 offset_in_source;
    i64 length_in_source;
    i64 line_number;
};

struct source_base {
    virtual bool has_next() const noexcept = 0;
};

template<typename T>
struct source : source_base {
    virtual std::expected<element<T>, error> next() noexcept = 0;
};

struct data_source : source<std::byte> {};
struct text_source : source<char32_t> {};

