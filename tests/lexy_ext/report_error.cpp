// Copyright (C) 2020-2021 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include <lexy_ext/report_error.hpp>

#include <doctest/doctest.h>
#include <iterator>
#include <lexy/input/string_input.hpp>
#include <string>

namespace
{
struct production
{
    static constexpr auto name = "production";
};

struct error_tag
{
    static constexpr auto name = "error tag";
};
} // namespace

TEST_CASE("_detail::write_error")
{
    auto write = [](const auto& context, const auto& error) {
        std::string str;
        lexy_ext::_detail::write_error(std::back_insert_iterator(str), context, error, {});
        return str;
    };

    SUBCASE("simple cases")
    {
        auto input   = lexy::zstring_input("hello world");
        auto context = lexy::error_context(production{}, input, input.data());

        SUBCASE("generic error, position")
        {
            lexy::string_error<error_tag> error(input.data());
            CHECK(write(context, error) == R"*(error: while parsing production
     |
   1 | hello world
     | ^ error tag
)*");
        }
        SUBCASE("generic error, range")
        {
            lexy::string_error<error_tag> error(input.data() + 2, input.data() + 4);
            CHECK(write(context, error) == R"*(error: while parsing production
     |
   1 | hello world
     |   ^^ error tag
)*");
        }
        SUBCASE("expected literal")
        {
            lexy::string_error<lexy::expected_literal> error(input.data() + 2, "abc", 1);
            CHECK(write(context, error) == R"*(error: while parsing production
     |
   1 | hello world
     |   ^^ expected 'abc'
)*");
        }
        SUBCASE("expected keyword")
        {
            lexy::string_error<lexy::expected_keyword> error(input.data() + 2, input.data() + 7,
                                                             "abc");
            CHECK(write(context, error) == R"*(error: while parsing production
     |
   1 | hello world
     |   ^^^^^ expected keyword 'abc'
)*");
        }
        SUBCASE("expected char class")
        {
            lexy::string_error<lexy::expected_char_class> error(input.data() + 4, "class");
            CHECK(write(context, error) == R"*(error: while parsing production
     |
   1 | hello world
     |     ^ expected 'class' character
)*");
        }
    }

    SUBCASE("context annotation")
    {
        auto input = lexy::zstring_input("hello\nworld");

        auto context = lexy::error_context(production{}, input, input.data());
        lexy::string_error<error_tag> error(input.data() + 8);
        CHECK(write(context, error) == R"*(error: while parsing production
     |
   1 | hello
     | ~ beginning here
     |
   2 | world
     |   ^ error tag
)*");
    }
    SUBCASE("error at newline")
    {
        auto input = lexy::zstring_input("hello\nworld");

        auto context = lexy::error_context(production{}, input, input.data());
        lexy::string_error<error_tag> error(input.data() + 5);
        CHECK(write(context, error) == R"*(error: while parsing production
     |
   1 | hello\n
     |      ^^ error tag
)*");
    }
    SUBCASE("error at eof")
    {
        auto input = lexy::zstring_input("hello");

        auto context = lexy::error_context(production{}, input, input.data());
        lexy::string_error<error_tag> error(input.data() + 5);
        CHECK(write(context, error) == R"*(error: while parsing production
     |
   1 | hello
     |      ^ error tag
)*");
    }
    SUBCASE("escaped characters")
    {
        auto input = lexy::zstring_input<lexy::utf8_encoding>(u8"hel\u1234lo");

        auto context = lexy::error_context(production{}, input, input.data());
        lexy::string_error<error_tag, lexy::utf8_encoding> error(input.data(), input.data() + 6);
        CHECK(write(context, error) == R"*(error: while parsing production
     |
   1 | hel\u1234lo
     | ^^^^^^^^^ error tag
)*");
    }
    SUBCASE("multi-line range")
    {
        auto input = lexy::zstring_input("hello\nworld");

        auto context = lexy::error_context(production{}, input, input.data());
        lexy::string_error<error_tag> error(input.data(), input.data() + 8);
        CHECK(write(context, error) == R"*(error: while parsing production
     |
   1 | hello
     | ^^^^^ error tag
)*");
    }
}

