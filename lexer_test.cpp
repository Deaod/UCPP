#include "lexer.h"
#include <gtest/gtest.h>

TEST(lexer_tests, no_content_returns_no_tokens) {
    std::vector<char> c(std::size_t(0));
    lexer l;
    auto result = l.run(c);

    ASSERT_EQ(result.lexemes.size(), 0);
}

TEST(lexer_tests, whitespace_does_not_produce_tokens) {
    std::vector<char> c{ ' ', '\t', '\v', '\f' };
    lexer l;
    auto result = l.run(c);

    ASSERT_EQ(result.lexemes.size(), 0);
}

TEST(lexer_tests, line_feed_produces_LINE_END_token) {
    std::vector<char> c{ '\n' };
    lexer l;
    auto result = l.run(c);

    ASSERT_EQ(result.lexemes.size(), 1);
    ASSERT_EQ(result.lexemes[0].type, lexeme_type::LINE_END);
}

TEST(lexer_tests, carriage_return_produces_LINE_END_token) {
    std::vector<char> c{ '\r' };
    lexer l;
    auto result = l.run(c);

    ASSERT_EQ(result.lexemes.size(), 1);
    ASSERT_EQ(result.lexemes[0].type, lexeme_type::LINE_END);
}

TEST(lexer_tests, carriage_return_followed_by_line_feed_produces_LINE_END_token) {
    std::vector<char> c{ '\r', '\n' };
    lexer l;
    auto result = l.run(c);

    ASSERT_EQ(result.lexemes.size(), 1);
    ASSERT_EQ(result.lexemes[0].type, lexeme_type::LINE_END);
}

TEST(lexer_tests, zero_produces_OCTAL_token) {
    std::vector<char> c{ '0' };
    lexer l;
    auto result = l.run(c);

    ASSERT_EQ(result.lexemes.size(), 1);
    ASSERT_EQ(result.lexemes[0].type, lexeme_type::OCTAL);
}

TEST(lexer_tests, zero_followed_by_dot_produces_FLOAT_token) {
    std::vector<char> c{ '0', '.' };
    lexer l;
    auto result = l.run(c);

    ASSERT_EQ(result.lexemes.size(), 1);
    ASSERT_EQ(result.lexemes[0].type, lexeme_type::FLOAT);
}

TEST(lexer_tests, zero_followed_by_dot_f_produces_FLOAT_token) {
    std::vector<char> c{ '0', '.', 'f' };
    lexer l;
    auto result = l.run(c);

    ASSERT_EQ(result.lexemes.size(), 1);
    ASSERT_EQ(result.lexemes[0].type, lexeme_type::FLOAT);
}
