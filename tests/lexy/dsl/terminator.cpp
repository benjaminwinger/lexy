// Copyright (C) 2020-2021 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include <lexy/dsl/terminator.hpp>

#include "verify.hpp"
#include <lexy/dsl/capture.hpp>
#include <lexy/dsl/list.hpp>
#include <lexy/dsl/option.hpp>
#include <lexy/dsl/position.hpp>

TEST_CASE("dsl::terminator()")
{
    constexpr auto term = dsl::terminator(LEXY_LIT("!!!") >> dsl::position);

    CHECK(equivalent_rules(term.terminator(), LEXY_LIT("!!!") >> dsl::position));
    CHECK(equivalent_rules(term.recovery_rule(), dsl::recover(term.terminator())));

    CHECK(equivalent_rules(term.limit(dsl::lit_c<';'>).recovery_rule(),
                           dsl::recover(term.terminator()).limit(dsl::lit_c<';'>)));
    CHECK(equivalent_rules(term.limit(dsl::lit_c<';'>).limit(dsl::lit_c<'.'>),
                           term.limit(dsl::lit_c<';'>, dsl::lit_c<'.'>)));

    SUBCASE("operator()")
    {
        constexpr auto rule = term(dsl::position);
        CHECK(lexy::is_rule<decltype(rule)>);
        CHECK(equivalent_rules(rule, dsl::position + term.terminator()));
    }

    SUBCASE(".try_()")
    {
        constexpr auto rule = term.limit(dsl::lit_c<';'>).try_(LEXY_LIT("abc") + dsl::position);
        CHECK(lexy::is_rule<decltype(rule)>);

        constexpr auto callback
            = lexy::callback<int>([](const char*, const char*) { return 0; },
                                  [](const char*, const char*, const char*) { return 1; });

        auto empty = LEXY_VERIFY("");
        CHECK(empty.status == test_result::fatal_error);
        CHECK(empty.trace
              == test_trace().expected_literal(0, "abc", 0).recovery().cancel().cancel());

        auto null = LEXY_VERIFY("!!!");
        CHECK(null.status == test_result::recovered_error);
        CHECK(null.value == 0);
        CHECK(null.trace
              == test_trace()
                     .expected_literal(0, "abc", 0)
                     .recovery()
                     .finish()
                     .token("!!!")
                     .position());

        auto abc = LEXY_VERIFY("abc!!!");
        CHECK(abc.status == test_result::success);
        CHECK(abc.value == 1);
        CHECK(abc.trace == test_trace().token("abc").position().token("!!!").position());

        auto ab = LEXY_VERIFY("ab!!!");
        CHECK(ab.status == test_result::recovered_error);
        CHECK(ab.value == 0);
        CHECK(ab.trace
              == test_trace()
                     .expected_literal(0, "abc", 2)
                     .error_token("ab")
                     .recovery()
                     .finish()
                     .token("!!!")
                     .position());

        auto unterminated = LEXY_VERIFY("abc");
        CHECK(unterminated.status == test_result::fatal_error);
        CHECK(unterminated.trace
              == test_trace().token("abc").position().expected_literal(3, "!!!", 0).cancel());
        auto partial_terminator = LEXY_VERIFY("abc!");
        CHECK(partial_terminator.status == test_result::fatal_error);
        CHECK(partial_terminator.trace
              == test_trace()
                     .token("abc")
                     .position()
                     .expected_literal(3, "!!!", 1)
                     .error_token("!")
                     .cancel());
        auto other_terminator = LEXY_VERIFY("abc???");
        CHECK(other_terminator.status == test_result::fatal_error);
        CHECK(other_terminator.trace
              == test_trace().token("abc").position().expected_literal(3, "!!!", 0).cancel());
        auto later_terminator = LEXY_VERIFY("abcdef!!!");
        CHECK(later_terminator.status == test_result::fatal_error);
        CHECK(later_terminator.trace
              == test_trace().token("abc").position().expected_literal(3, "!!!", 0).cancel());

        auto limited = LEXY_VERIFY("abde;abc!!!");
        CHECK(limited.status == test_result::fatal_error);
        CHECK(limited.trace
              == test_trace()
                     .expected_literal(0, "abc", 2)
                     .error_token("ab")
                     .recovery()
                     .error_token("de")
                     .cancel()
                     .cancel());
    }

    SUBCASE(".opt()")
    {
        constexpr auto rule = term.limit(dsl::lit_c<';'>).opt(dsl::capture(LEXY_LIT("abc")));
        CHECK(lexy::is_rule<decltype(rule)>);

        constexpr auto callback
            = lexy::callback<int>([](const char*, lexy::nullopt, const char*) { return 0; },
                                  [](const char* begin, lexy::string_lexeme<> lex, const char*) {
                                      CHECK(lex.begin() == begin);
                                      CHECK(lex.size() == 3);

                                      CHECK(lex[0] == 'a');
                                      CHECK(lex[1] == 'b');
                                      CHECK(lex[2] == 'c');

                                      return 1;
                                  },
                                  [](const char*, const char*) { return 2; });

        auto empty = LEXY_VERIFY("");
        CHECK(empty.status == test_result::fatal_error);
        CHECK(empty.trace
              == test_trace().expected_literal(0, "abc", 0).recovery().cancel().cancel());

        auto null = LEXY_VERIFY("!!!");
        CHECK(null.status == test_result::success);
        CHECK(null.value == 0);
        CHECK(null.trace == test_trace().token("!!!").position());

        auto abc = LEXY_VERIFY("abc!!!");
        CHECK(abc.status == test_result::success);
        CHECK(abc.value == 1);
        CHECK(abc.trace == test_trace().token("abc").token("!!!").position());

        auto ab = LEXY_VERIFY("ab!!!");
        CHECK(ab.status == test_result::recovered_error);
        CHECK(ab.value == 2);
        CHECK(ab.trace
              == test_trace()
                     .expected_literal(0, "abc", 2)
                     .error_token("ab")
                     .recovery()
                     .finish()
                     .token("!!!")
                     .position());

        auto unterminated = LEXY_VERIFY("abc");
        CHECK(unterminated.status == test_result::fatal_error);
        CHECK(unterminated.trace
              == test_trace().token("abc").expected_literal(3, "!!!", 0).cancel());

        auto limited = LEXY_VERIFY("abde;abc!!!");
        CHECK(limited.status == test_result::fatal_error);
        CHECK(limited.trace
              == test_trace()
                     .expected_literal(0, "abc", 2)
                     .error_token("ab")
                     .recovery()
                     .error_token("de")
                     .cancel()
                     .cancel());
    }

    SUBCASE(".list(branch)")
    {
        constexpr auto rule
            = term.limit(dsl::lit_c<';'>).list(dsl::capture(LEXY_LIT("ab") >> LEXY_LIT("c")));
        CHECK(lexy::is_rule<decltype(rule)>);

        constexpr auto callback
            = [](const char*, std::size_t count, const char*) { return int(count); };

        auto empty = LEXY_VERIFY("");
        CHECK(empty.status == test_result::fatal_error);
        CHECK(empty.trace
              == test_trace().expected_literal(0, "ab", 0).recovery().cancel().cancel());

        auto zero = LEXY_VERIFY("!!!");
        CHECK(zero.status == test_result::recovered_error);
        CHECK(zero.trace
              == test_trace()
                     .expected_literal(0, "ab", 0)
                     .recovery()
                     .finish()
                     .token("!!!")
                     .position());
        auto one = LEXY_VERIFY("abc!!!");
        CHECK(one.status == test_result::success);
        CHECK(one.value == 1);
        CHECK(one.trace == test_trace().token("ab").token("c").token("!!!").position());
        auto two = LEXY_VERIFY("abcabc!!!");
        CHECK(two.status == test_result::success);
        CHECK(two.value == 2);
        CHECK(
            two.trace
            == test_trace().token("ab").token("c").token("ab").token("c").token("!!!").position());
        auto three = LEXY_VERIFY("abcabcabc!!!");
        CHECK(three.status == test_result::success);
        CHECK(three.value == 3);
        CHECK(three.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .token("ab")
                     .token("c")
                     .token("ab")
                     .token("c")
                     .token("!!!")
                     .position());

        auto recover_item = LEXY_VERIFY("abcaabc!!!");
        CHECK(recover_item.status == test_result::recovered_error);
        CHECK(recover_item.value == 2);
        CHECK(recover_item.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .expected_literal(3, "ab", 1)
                     .error_token("a")
                     .recovery().finish()
                     .token("ab").token("c")
                     .token("!!!").position());
        // clang-format on
        auto recover_item_failed = LEXY_VERIFY("abcaababc!!!");
        CHECK(recover_item_failed.status == test_result::recovered_error);
        CHECK(recover_item_failed.value == 2);
        CHECK(recover_item_failed.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .expected_literal(3, "ab", 1)
                     .error_token("a")
                     .recovery().finish()
                     .token("ab").expected_literal(6, "c", 0)
                     .recovery().finish()
                     .token("ab").token("c")
                     .token("!!!").position());
        // clang-format on
        auto recover_term = LEXY_VERIFY("abcabd!!!");
        CHECK(recover_term.status == test_result::recovered_error);
        CHECK(recover_term.value == 1);
        CHECK(recover_term.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .token("ab").expected_literal(5, "c", 0)
                     .recovery()
                          .error_token("d")
                         .finish()
                     .token("!!!").position());
        // clang-format on
        auto recover_limit = LEXY_VERIFY("abcabd;abc!!!");
        CHECK(recover_limit.status == test_result::fatal_error);
        CHECK(recover_limit.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .token("ab").expected_literal(5, "c", 0)
                     .recovery()
                          .error_token("d")
                         .cancel()
                     .cancel());
        // clang-format on
    }
    SUBCASE(".list(rule)")
    {
        constexpr auto rule
            = term.limit(dsl::lit_c<';'>).list(dsl::capture(LEXY_LIT("ab") + LEXY_LIT("c")));
        CHECK(lexy::is_rule<decltype(rule)>);

        constexpr auto callback
            = [](const char*, std::size_t count, const char*) { return int(count); };

        auto empty = LEXY_VERIFY("");
        CHECK(empty.status == test_result::fatal_error);
        CHECK(empty.trace
              == test_trace().expected_literal(0, "ab", 0).recovery().cancel().cancel());

        auto zero = LEXY_VERIFY("!!!");
        CHECK(zero.status == test_result::recovered_error);
        CHECK(zero.trace
              == test_trace()
                     .expected_literal(0, "ab", 0)
                     .recovery()
                     .finish()
                     .token("!!!")
                     .position());
        auto one = LEXY_VERIFY("abc!!!");
        CHECK(one.status == test_result::success);
        CHECK(one.value == 1);
        CHECK(one.trace == test_trace().token("ab").token("c").token("!!!").position());
        auto two = LEXY_VERIFY("abcabc!!!");
        CHECK(two.status == test_result::success);
        CHECK(two.value == 2);
        CHECK(
            two.trace
            == test_trace().token("ab").token("c").token("ab").token("c").token("!!!").position());
        auto three = LEXY_VERIFY("abcabcabc!!!");
        CHECK(three.status == test_result::success);
        CHECK(three.value == 3);
        CHECK(three.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .token("ab")
                     .token("c")
                     .token("ab")
                     .token("c")
                     .token("!!!")
                     .position());

        auto recover_item = LEXY_VERIFY("abcaabc!!!"); // can't actually recover at the next item
        CHECK(recover_item.status == test_result::recovered_error);
        CHECK(recover_item.value == 1);
        CHECK(recover_item.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .expected_literal(3, "ab", 1)
                     .error_token("a")
                     .recovery()
                         .error_token("a")
                         .error_token("b")
                         .error_token("c")
                         .finish()
                     .token("!!!").position());
        // clang-format on
        auto recover_term = LEXY_VERIFY("abcabd!!!");
        CHECK(recover_term.status == test_result::recovered_error);
        CHECK(recover_term.value == 1);
        CHECK(recover_term.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .token("ab").expected_literal(5, "c", 0)
                     .recovery()
                        .error_token("d")
                        .finish()
                     .token("!!!").position());
        // clang-format on
        auto recover_limit = LEXY_VERIFY("abcabd;abc!!!");
        CHECK(recover_limit.status == test_result::fatal_error);
        CHECK(recover_limit.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .token("ab").expected_literal(5, "c", 0)
                     .recovery()
                          .error_token("d")
                         .cancel()
                     .cancel());
        // clang-format on
    }
    SUBCASE(".list(branch, sep)")
    {
        constexpr auto rule = term.limit(dsl::lit_c<';'>)
                                  .list(dsl::capture(LEXY_LIT("ab") >> LEXY_LIT("c")),
                                        dsl::sep((dsl::lit_c<','>) >> dsl::lit_c<','>));
        CHECK(lexy::is_rule<decltype(rule)>);

        constexpr auto callback
            = [](const char*, std::size_t count, const char*) { return int(count); };

        auto empty = LEXY_VERIFY("");
        CHECK(empty.status == test_result::fatal_error);
        CHECK(empty.trace
              == test_trace().expected_literal(0, "ab", 0).recovery().cancel().cancel());

        auto zero = LEXY_VERIFY("!!!");
        CHECK(zero.status == test_result::recovered_error);
        CHECK(zero.trace
              == test_trace()
                     .expected_literal(0, "ab", 0)
                     .recovery()
                     .finish()
                     .token("!!!")
                     .position());
        auto one = LEXY_VERIFY("abc!!!");
        CHECK(one.status == test_result::success);
        CHECK(one.value == 1);
        CHECK(one.trace == test_trace().token("ab").token("c").token("!!!").position());
        auto two = LEXY_VERIFY("abc,,abc!!!");
        CHECK(two.status == test_result::success);
        CHECK(two.value == 2);
        CHECK(two.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .token(",")
                     .token(",")
                     .token("ab")
                     .token("c")
                     .token("!!!")
                     .position());
        auto three = LEXY_VERIFY("abc,,abc,,abc!!!");
        CHECK(three.status == test_result::success);
        CHECK(three.value == 3);
        CHECK(three.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .token(",")
                     .token(",")
                     .token("ab")
                     .token("c")
                     .token(",")
                     .token(",")
                     .token("ab")
                     .token("c")
                     .token("!!!")
                     .position());

        auto trailing = LEXY_VERIFY("abc,,abc,,!!!");
        CHECK(trailing.status == test_result::recovered_error);
        CHECK(trailing.value == 2);
        CHECK(trailing.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .token(",")
                     .token(",")
                     .token("ab")
                     .token("c")
                     .token(",")
                     .token(",")
                     .error(8, 10, "unexpected trailing separator")
                     .token("!!!")
                     .position());

        auto no_sep = LEXY_VERIFY("abcabc!!!");
        CHECK(no_sep.status == test_result::recovered_error);
        CHECK(no_sep.value == 2);
        CHECK(no_sep.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .expected_literal(3, ",", 0)
                     .token("ab")
                     .token("c")
                     .token("!!!")
                     .position());
        auto no_sep_no_item = LEXY_VERIFY("abcd!!!");
        CHECK(no_sep_no_item.status == test_result::recovered_error);
        CHECK(no_sep_no_item.value == 1);
        CHECK(no_sep_no_item.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .expected_literal(3, ",", 0)
                     .recovery()
                     .error_token("d")
                     .finish()
                     .token("!!!")
                     .position());
        auto no_sep_partial_item = LEXY_VERIFY("abcab!!!");
        CHECK(no_sep_partial_item.status == test_result::recovered_error);
        CHECK(no_sep_partial_item.value == 1);
        CHECK(no_sep_partial_item.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .expected_literal(3, ",", 0)
                     .token("ab")
                     .expected_literal(5, "c", 0)
                     .recovery()
                     .finish()
                     .token("!!!")
                     .position());

        auto partial_sep = LEXY_VERIFY("abc,abc!!!");
        CHECK(partial_sep.status == test_result::recovered_error);
        CHECK(partial_sep.value == 1);
        CHECK(partial_sep.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .token(",")
                     .expected_literal(4, ",", 0)
                     .recovery()
                     .error_token("a")
                     .error_token("b")
                     .error_token("c")
                     .finish()
                     .token("!!!")
                     .position());

        auto recover_sep = LEXY_VERIFY("abc,,a,,abc!!!");
        CHECK(recover_sep.status == test_result::recovered_error);
        CHECK(recover_sep.value == 2);
        CHECK(recover_sep.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .token(",").token(",")
                     .expected_literal(5, "ab", 1)
                     .error_token("a")
                     .recovery().finish()
                     .token(",").token(",")
                     .token("ab").token("c")
                     .token("!!!").position());
        // clang-format on
        auto recover_sep_failed = LEXY_VERIFY("abc,,a,abc!!!");
        CHECK(recover_sep_failed.status == test_result::recovered_error);
        CHECK(recover_sep_failed.value == 1);
        CHECK(recover_sep_failed.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .token(",").token(",")
                     .expected_literal(5, "ab", 1)
                     .error_token("a")
                     .recovery().finish()
                     .token(",").expected_literal(7, ",", 0)
                     .recovery()
                         .error_token("a")
                         .error_token("b")
                         .error_token("c")
                         .finish()
                     .token("!!!").position());
        // clang-format on
        auto recover_term = LEXY_VERIFY("abc,,abd!!!");
        CHECK(recover_term.status == test_result::recovered_error);
        CHECK(recover_term.value == 1);
        CHECK(recover_term.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .token(",").token(",")
                     .token("ab").expected_literal(7, "c", 0)
                     .recovery()
                          .error_token("d")
                         .finish()
                     .token("!!!").position());
        // clang-format on
        auto recover_limit = LEXY_VERIFY("abc,,abd;abc!!!");
        CHECK(recover_limit.status == test_result::fatal_error);
        CHECK(recover_limit.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .token(",").token(",")
                     .token("ab").expected_literal(7, "c", 0)
                     .recovery()
                          .error_token("d")
                         .cancel()
                     .cancel());
        // clang-format on
    }
    SUBCASE(".list(rule, sep)")
    {
        constexpr auto rule = term.limit(dsl::lit_c<';'>)
                                  .list(dsl::capture(LEXY_LIT("ab") + LEXY_LIT("c")),
                                        dsl::sep((dsl::lit_c<','>) >> dsl::lit_c<','>));
        CHECK(lexy::is_rule<decltype(rule)>);

        constexpr auto callback
            = [](const char*, std::size_t count, const char*) { return int(count); };

        auto empty = LEXY_VERIFY("");
        CHECK(empty.status == test_result::fatal_error);
        CHECK(empty.trace
              == test_trace().expected_literal(0, "ab", 0).recovery().cancel().cancel());

        auto zero = LEXY_VERIFY("!!!");
        CHECK(zero.status == test_result::recovered_error);
        CHECK(zero.trace
              == test_trace()
                     .expected_literal(0, "ab", 0)
                     .recovery()
                     .finish()
                     .token("!!!")
                     .position());
        auto one = LEXY_VERIFY("abc!!!");
        CHECK(one.status == test_result::success);
        CHECK(one.value == 1);
        CHECK(one.trace == test_trace().token("ab").token("c").token("!!!").position());
        auto two = LEXY_VERIFY("abc,,abc!!!");
        CHECK(two.status == test_result::success);
        CHECK(two.value == 2);
        CHECK(two.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .token(",")
                     .token(",")
                     .token("ab")
                     .token("c")
                     .token("!!!")
                     .position());
        auto three = LEXY_VERIFY("abc,,abc,,abc!!!");
        CHECK(three.status == test_result::success);
        CHECK(three.value == 3);
        CHECK(three.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .token(",")
                     .token(",")
                     .token("ab")
                     .token("c")
                     .token(",")
                     .token(",")
                     .token("ab")
                     .token("c")
                     .token("!!!")
                     .position());

        auto trailing = LEXY_VERIFY("abc,,abc,,!!!");
        CHECK(trailing.status == test_result::recovered_error);
        CHECK(trailing.value == 2);
        CHECK(trailing.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .token(",")
                     .token(",")
                     .token("ab")
                     .token("c")
                     .token(",")
                     .token(",")
                     .error(8, 10, "unexpected trailing separator")
                     .token("!!!")
                     .position());

        auto no_sep = LEXY_VERIFY("abcabc!!!");
        CHECK(no_sep.status == test_result::recovered_error);
        CHECK(no_sep.value == 1);
        CHECK(no_sep.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .expected_literal(3, ",", 0)
                     .recovery()
                     .error_token("a")
                     .error_token("b")
                     .error_token("c")
                     .finish()
                     .token("!!!")
                     .position());
        auto no_sep_no_item = LEXY_VERIFY("abcd!!!");
        CHECK(no_sep_no_item.status == test_result::recovered_error);
        CHECK(no_sep_no_item.value == 1);
        CHECK(no_sep_no_item.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .expected_literal(3, ",", 0)
                     .recovery()
                     .error_token("d")
                     .finish()
                     .token("!!!")
                     .position());
        auto no_sep_partial_item = LEXY_VERIFY("abcab!!!");
        CHECK(no_sep_partial_item.status == test_result::recovered_error);
        CHECK(no_sep_partial_item.value == 1);
        CHECK(no_sep_partial_item.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .expected_literal(3, ",", 0)
                     .recovery()
                     .error_token("a")
                     .error_token("b")
                     .finish()
                     .token("!!!")
                     .position());

        auto partial_sep = LEXY_VERIFY("abc,abc!!!");
        CHECK(partial_sep.status == test_result::recovered_error);
        CHECK(partial_sep.value == 1);
        CHECK(partial_sep.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .token(",")
                     .expected_literal(4, ",", 0)
                     .recovery()
                     .error_token("a")
                     .error_token("b")
                     .error_token("c")
                     .finish()
                     .token("!!!")
                     .position());

        auto recover_sep = LEXY_VERIFY("abc,,a,,abc!!!");
        CHECK(recover_sep.status == test_result::recovered_error);
        CHECK(recover_sep.value == 2);
        CHECK(recover_sep.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .token(",").token(",")
                     .expected_literal(5, "ab", 1)
                     .error_token("a")
                     .recovery().finish()
                     .token(",").token(",")
                     .token("ab").token("c")
                     .token("!!!").position());
        // clang-format on
        auto recover_sep_failed = LEXY_VERIFY("abc,,a,abc!!!");
        CHECK(recover_sep_failed.status == test_result::recovered_error);
        CHECK(recover_sep_failed.value == 1);
        CHECK(recover_sep_failed.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .token(",").token(",")
                     .expected_literal(5, "ab", 1)
                     .error_token("a")
                     .recovery().finish()
                     .token(",").expected_literal(7, ",", 0)
                     .recovery()
                         .error_token("a")
                         .error_token("b")
                         .error_token("c")
                         .finish()
                     .token("!!!").position());
        // clang-format on
        auto recover_term = LEXY_VERIFY("abc,,abd!!!");
        CHECK(recover_term.status == test_result::recovered_error);
        CHECK(recover_term.value == 1);
        CHECK(recover_term.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .token(",").token(",")
                     .token("ab").expected_literal(7, "c", 0)
                     .recovery()
                          .error_token("d")
                         .finish()
                     .token("!!!").position());
        // clang-format on
        auto recover_limit = LEXY_VERIFY("abc,,abd;abc!!!");
        CHECK(recover_limit.status == test_result::fatal_error);
        CHECK(recover_limit.trace
              // clang-format off
              == test_trace()
                     .token("ab").token("c")
                     .token(",").token(",")
                     .token("ab").expected_literal(7, "c", 0)
                     .recovery()
                          .error_token("d")
                         .cancel()
                     .cancel());
        // clang-format on
    }
    SUBCASE(".list(branch, trailing_sep)")
    {
        constexpr auto rule = term.limit(dsl::lit_c<';'>)
                                  .list(dsl::capture(LEXY_LIT("ab") >> LEXY_LIT("c")),
                                        dsl::trailing_sep((dsl::lit_c<','>) >> dsl::lit_c<','>));
        CHECK(lexy::is_rule<decltype(rule)>);

        constexpr auto callback
            = [](const char*, std::size_t count, const char*) { return int(count); };

        auto empty = LEXY_VERIFY("");
        CHECK(empty.status == test_result::fatal_error);
        CHECK(empty.trace
              == test_trace().expected_literal(0, "ab", 0).recovery().cancel().cancel());

        auto trailing = LEXY_VERIFY("abc,,abc,,!!!");
        CHECK(trailing.status == test_result::success);
        CHECK(trailing.value == 2);
        CHECK(trailing.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .token(",")
                     .token(",")
                     .token("ab")
                     .token("c")
                     .token(",")
                     .token(",")
                     .token("!!!")
                     .position());
    }

    // Only simple checks necessary, it shares implementations between list and opt.
    SUBCASE(".opt_list(rule)")
    {
        constexpr auto rule
            = term.limit(dsl::lit_c<';'>).opt_list(dsl::capture(LEXY_LIT("ab") >> LEXY_LIT("c")));
        CHECK(lexy::is_rule<decltype(rule)>);

        constexpr auto callback
            = lexy::callback<int>([](const char*, lexy::nullopt, const char*) { return 0; },
                                  [](const char*, std::size_t count, const char*) {
                                      return int(count);
                                  });

        auto empty = LEXY_VERIFY("");
        CHECK(empty.status == test_result::fatal_error);
        CHECK(empty.trace
              == test_trace().expected_literal(0, "ab", 0).recovery().cancel().cancel());

        auto zero = LEXY_VERIFY("!!!");
        CHECK(zero.status == test_result::success);
        CHECK(zero.value == 0);
        CHECK(zero.trace == test_trace().token("!!!").position());
        auto one = LEXY_VERIFY("abc!!!");
        CHECK(one.status == test_result::success);
        CHECK(one.value == 1);
        CHECK(one.trace == test_trace().token("ab").token("c").token("!!!").position());
        auto three = LEXY_VERIFY("abcabcabc!!!");
        CHECK(three.status == test_result::success);
        CHECK(three.value == 3);
        CHECK(three.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .token("ab")
                     .token("c")
                     .token("ab")
                     .token("c")
                     .token("!!!")
                     .position());

        auto recover = LEXY_VERIFY("abd!!!");
        CHECK(recover.status == test_result::recovered_error);
        CHECK(recover.value == 0);
        CHECK(recover.trace
              == test_trace()
                     .token("ab")
                     .expected_literal(2, "c", 0)
                     .recovery()
                     .error_token("d")
                     .finish()
                     .token("!!!")
                     .position());
    }
    SUBCASE(".opt_list(rule, sep)")
    {
        constexpr auto rule = term.limit(dsl::lit_c<';'>)
                                  .opt_list(dsl::capture(LEXY_LIT("ab") >> LEXY_LIT("c")),
                                            dsl::sep(dsl::lit_c<','>));
        CHECK(lexy::is_rule<decltype(rule)>);

        constexpr auto callback
            = lexy::callback<int>([](const char*, lexy::nullopt, const char*) { return 0; },
                                  [](const char*, std::size_t count, const char*) {
                                      return int(count);
                                  });

        auto empty = LEXY_VERIFY("");
        CHECK(empty.status == test_result::fatal_error);
        CHECK(empty.trace
              == test_trace().expected_literal(0, "ab", 0).recovery().cancel().cancel());

        auto zero = LEXY_VERIFY("!!!");
        CHECK(zero.status == test_result::success);
        CHECK(zero.value == 0);
        CHECK(zero.trace == test_trace().token("!!!").position());
        auto one = LEXY_VERIFY("abc!!!");
        CHECK(one.status == test_result::success);
        CHECK(one.value == 1);
        CHECK(one.trace == test_trace().token("ab").token("c").token("!!!").position());
        auto three = LEXY_VERIFY("abc,abc,abc!!!");
        CHECK(three.status == test_result::success);
        CHECK(three.value == 3);
        CHECK(three.trace
              == test_trace()
                     .token("ab")
                     .token("c")
                     .token(",")
                     .token("ab")
                     .token("c")
                     .token(",")
                     .token("ab")
                     .token("c")
                     .token("!!!")
                     .position());

        auto recover = LEXY_VERIFY("abd!!!");
        CHECK(recover.status == test_result::recovered_error);
        CHECK(recover.value == 0);
        CHECK(recover.trace
              == test_trace()
                     .token("ab")
                     .expected_literal(2, "c", 0)
                     .recovery()
                     .error_token("d")
                     .finish()
                     .token("!!!")
                     .position());
    }
}

