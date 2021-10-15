// Copyright (C) 2020-2021 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#ifndef LEXY_DSL_LITERAL_HPP_INCLUDED
#define LEXY_DSL_LITERAL_HPP_INCLUDED

#include <lexy/_detail/iterator.hpp>
#include <lexy/_detail/nttp_string.hpp>
#include <lexy/dsl/base.hpp>
#include <lexy/dsl/token.hpp>

namespace lexyd
{
template <typename CharT, CharT... C>
struct _lit
: token_base<_lit<CharT, C...>,
             std::conditional_t<sizeof...(C) == 0, unconditional_branch_base, branch_base>>
{
    template <typename Reader>
    struct tp
    {
        typename Reader::iterator end;

        constexpr explicit tp(const Reader& reader) : end(reader.position()) {}

        constexpr auto try_parse(Reader reader)
        {
            if constexpr (sizeof...(C) == 0)
            {
                end = reader.position();
                return std::true_type{};
            }
            else
            {
                auto result
                    // Compare each code unit, bump on success, cancel on failure.
                    = ((reader.peek() == lexy::_char_to_int_type<typename Reader::encoding>(C)
                            ? (reader.bump(), true)
                            : false)
                       && ...);
                end = reader.position();
                return result;
            }
        }

        template <typename Context>
        constexpr void report_error(Context& context, const Reader& reader)
        {
            constexpr auto str = lexy::_detail::type_string<CharT, C...>::template c_str<
                typename Reader::encoding::char_type>;

            auto begin = reader.position();
            auto index = lexy::_detail::range_size(begin, this->end);
            auto err   = lexy::error<Reader, lexy::expected_literal>(begin, str, index);
            context.on(_ev::error{}, err);
        }
    };
};

template <auto C>
constexpr auto lit_c = _lit<std::decay_t<decltype(C)>, C>{};

template <unsigned char... C>
constexpr auto lit_b = _lit<unsigned char, C...>{};

#if LEXY_HAS_NTTP
/// Matches the literal string.
template <lexy::_detail::string_literal Str>
constexpr auto lit = lexy::_detail::to_type_string<_lit, Str>{};
#endif

#define LEXY_LIT(Str)                                                                              \
    LEXY_NTTP_STRING(::lexyd::_lit, Str) {}
} // namespace lexyd

#endif // LEXY_DSL_LITERAL_HPP_INCLUDED

