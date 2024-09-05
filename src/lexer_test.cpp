#include "lexer.h"
#include <catch.hpp>

TEST_CASE("Empty content returns no lexemes") {
    std::vector<char> c(std::size_t(0));
    lexer l{"test"};
    auto result = l.run(c);

    REQUIRE(result.lexemes.size() == 0);
}

TEST_CASE("whitespace produces WHITESPACE lexemes") {
    std::vector<char> c{' ', '\t', '\v', '\f'};
    lexer l{"test"};
    auto result = l.run(c);

    REQUIRE(result.lexemes.size() == 1);
    REQUIRE(result.lexemes.begin()->type == lexeme_type::WHITESPACE);
}

TEST_CASE("line feed produces LINE_END lexeme") {
    std::vector<char> c{'\n'};
    lexer l{"test"};
    auto result = l.run(c);

    REQUIRE(result.lexemes.size() == 1);
    REQUIRE(result.lexemes.begin()->type == lexeme_type::LINE_END);
}

TEST_CASE("carriage return produces LINE_END lexeme") {
    std::vector<char> c{'\r'};
    lexer l{"test"};
    auto result = l.run(c);

    REQUIRE(result.lexemes.size() == 1);
    REQUIRE(result.lexemes.begin()->type == lexeme_type::LINE_END);
}

TEST_CASE("carriage return followed by line feed produces LINE_END lexeme") {
    std::vector<char> c{'\r', '\n'};
    lexer l{"test"};
    auto result = l.run(c);

    REQUIRE(result.lexemes.size() == 1);
    REQUIRE(result.lexemes.begin()->type == lexeme_type::LINE_END);
}

TEST_CASE("zero produces OCTAL lexeme") {
    std::vector<char> c{'0'};
    lexer l{"test"};
    auto result = l.run(c);

    REQUIRE(result.lexemes.size() == 1);
    REQUIRE(result.lexemes.begin()->type == lexeme_type::OCTAL);
}

TEST_CASE("zero followed by dot produces FLOAT lexeme") {
    std::vector<char> c{'0', '.'};
    lexer l{"test"};
    auto result = l.run(c);

    REQUIRE(result.lexemes.size() == 1);
    REQUIRE(result.lexemes.begin()->type == lexeme_type::FLOAT);
}

TEST_CASE("zero followed by dot f produces FLOAT lexeme") {
    std::vector<char> c{'0', '.', 'f'};
    lexer l{"test"};
    auto result = l.run(c);

    REQUIRE(result.lexemes.size() == 1);
    REQUIRE(result.lexemes.begin()->type == lexeme_type::FLOAT);
}
