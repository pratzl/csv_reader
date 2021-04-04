// csv_reader.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "csv_reader.hpp"
#include <iostream>
#include <charconv>
#include <concepts>
#include <string_view>
#include <type_traits>
#include <limits>
#include <cassert>
#include "util.hpp"

using std::is_same_v;
using std::string;
using std::string_view;
using std::numeric_limits;
using std::forward_iterator;
using std::random_access_iterator;
using std::from_chars_result;
using std::from_chars;
using std::ranges::forward_range;
using std::ranges::random_access_range;
using std::ranges::begin;
using std::ranges::end;
using std::size;
using namespace std::literals;

/**
 * Types
 *  1.  Boolean:        true/false, yes/no, integer
 *  2.  Integer:        int8, int16, int32, int64, uint8, uint16, uint32, uint64
 *  3.  Floating Point: float32, float64, float80
 *  4.  String
 *
 * Features / Boundary Cases:
 *  1.  Empty lines
 *  2.  Lines with varying number of column values
 *  3.  0-n header row(s)
 *  4.  Caller override of column types
 * 
 * Design Notes
 *  1.  While it's possible to determine if a floating point value will fit in a float32, it
 *      doesn't have the same precision fidelity that float64 has and may not be the best
 *      choice. For this reason, type detection will only identify floating point as float64.
 *      The caller can override this to select float32 or float80.
 *  2.  Empty column values are igored when determining column types, and are assigned the
 *      default of their type when reading rows: 0 for numeric types, false for boolean,
 *      empty string for string.
 */


//csv_flags& operator&=(csv_flags& lhs, csv_flags rhs) {
    //static_cast<int8_t&>(lhs) &= static_cast<int8_t>(rhs);
//}
csv_flags operator&(csv_flags lhs, csv_flags rhs) {
    return static_cast<csv_flags>(static_cast<int8_t>(lhs) & static_cast<int8_t>(rhs));
}
csv_flags operator|(csv_flags lhs, csv_flags rhs) {
    return static_cast<csv_flags>(static_cast<int8_t>(lhs) | static_cast<int8_t>(rhs));
}

bool is_dec_digit(char ch) { return ch >= '0' && ch <= '9'; }
bool is_hex_digit(char ch) { return is_dec_digit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'); }

int_type eval_int_type(const char* first, const char* last) {
    // Leading + or -: decimal
    if (last - first >= 2 && (first[0] == '+' || first[0] == '-')) {
        for (++first; first != last && is_dec_digit(*first); ++first);
        if(first == last) // all decimal digits?
            return int_type::decimal;
    }
    // Leading 0x or 0X: hexidecimal
    else if (last - first >= 3 && (first[0] == '0' && (first[1] == 'x' || first[1] == 'X'))) {
        for (first += 2; first != last && is_hex_digit(*first); ++first);
        if(first == last)
            int_type::hexidecimal;
    }
    // No leading character hints; examine chars for decimal & hexidecimal digits
    else if (last - first >= 1) {
        for (; first != last && is_dec_digit(*first); ++first);
        if (first == last) // all decimal digits?
            return int_type::decimal;
        for (; first != last && is_hex_digit(*first); ++first);
        if (first == last) // all hexidecimal digits?
            return int_type::hexidecimal;
    }
    return int_type::undefined;
}

struct csv_value_type { 
    csv_type type = csv_type::unknown; 
    int radix = 10;
    csv_value value;
};

csv_type smallest_int_type(const char* first, const char* last) {
    int64_t val;
    from_chars_result result = from_chars(first, last, val);
    if (result.ptr != last)
        return csv_type::unknown;

    if (val >= numeric_limits<int8_t>::min() && val <= numeric_limits<int8_t>::max())
        return csv_type::int8;
    else if (val >= numeric_limits<int16_t>::min() && val <= numeric_limits<int16_t>::max())
        return csv_type::int16;
    else if (val >= numeric_limits<int32_t>::min() && val <= numeric_limits<int32_t>::max())
        return csv_type::int32;
    else
        return csv_type::int64;
}
csv_type smallest_uint_type(const char* first, const char* last, int base) {
    uint64_t val;
    from_chars_result result = from_chars(first, last, val, base);
    if (result.ptr != last)
        return csv_type::unknown;

    if (val <= numeric_limits<uint8_t>::max())
        return csv_type::uint8;
    else if (val <= numeric_limits<uint16_t>::max())
        return csv_type::uint16;
    else if (val <= numeric_limits<uint32_t>::max())
        return csv_type::uint32;
    else
        return csv_type::uint64;
}

csv_type smallest_float_type(const char* first, const char* last) {
    double val;
    from_chars_result result = from_chars(first, last, val);
    if (result.ptr != last)
        return csv_type::unknown;
    return csv_type::float64;
}

template<char_range Rng1, char_range Rng2>
bool match_symbol(Rng1 const& line, Rng2 const& symbol) {
    auto ln = begin(line);
    auto sy = begin(symbol);
    for (; ln != end(line) && sy != end(symbol) && *ln == *sy; ++ln, ++sy);
    return ln == end(line) || sy == end(symbol);
}

template<typename CharIter>
bool match_any_char(CharIter it, string const& charset) {
    for (auto ch : charset)
        if (*it == ch)
            return true;
    return false;
}

csv_type_vector eval_line_types( string_view const& rng
                               , string const& sep_charset
                               , string const& quote_lead_symbol
                               , string const& quote_trail_symbol
                               , string const& whitesp_charset
                               , csv_flags flags) {
    csv_type_vector type_vec;

    // empty line
    if (rng.empty()) {
        type_vec.push_back(csv_type::unknown);
        return type_vec;
    }

    for (auto fld = begin(rng); fld != end(rng); ) {
        // advance past leading whitespace
        while (fld != end(rng) && match_any_char(fld, whitesp_charset)) 
            ++fld;

        // blank entry
        if (fld == end(rng)) {
            type_vec.push_back(csv_type::unknown);
            break;
        }

        // quoted value
        if (match_symbol(make_subrange2(fld, end(rng)), quote_lead_symbol)) {
            fld += size(quote_lead_symbol);
            auto first = fld;
            for (; fld != end(rng) && !match_symbol(make_subrange2(fld, end(rng)), quote_trail_symbol); ++fld); // find trailing quote
            type_vec.push_back(match_type(make_subrange2(first, fld), flags));
            if (fld == end(rng)) // trailing quote not found
                break;
            fld += size(quote_trail_symbol); // advance past quote

            for (; fld != end(rng) && !match_any_char(fld, sep_charset); ++fld); // advance to next sep_charset or eol
            if (fld != end(rng)) // move to start of next fld
                ++fld;
        }
        // unquoted value
        else {
            auto first = fld;
            auto last = fld;
            for (; fld != end(rng) && !match_any_char(fld, sep_charset); ++fld) {
                if (!match_any_char(fld, whitesp_charset))
                    last = fld + 1;
            }
            type_vec.push_back(match_type(make_subrange2(first, last), flags));
            if (fld != end(rng)) // move to start of next fld
                ++fld;
        }
    }
    return type_vec;
}

int sint_bits(csv_type ct) {
    switch (ct) {
    case csv_type::int8: return 8;
    case csv_type::int16: return 16;
    case csv_type::int32: return 32;
    case csv_type::int64: return 64;
    default: return 0;
    }
}
int uint_bits(csv_type ct) {
    switch (ct) {
    case csv_type::uint8: return 8;
    case csv_type::uint16: return 16;
    case csv_type::uint32: return 32;
    case csv_type::uint64: return 64;
    default: return 0;
    }
}
int int_bits(csv_type ct) { return std::max(sint_bits(ct), uint_bits(ct)); }

bool is_sint(csv_type ct) { return sint_bits(ct) > 0; }
bool is_uint(csv_type ct) { return uint_bits(ct) > 0; }
bool is_int(csv_type ct) { return int_bits(ct) > 0; }

bool is_boolean(csv_type ct) { return ct == csv_type::boolean; }
bool is_float(csv_type ct) { return ct == csv_type::float32 || ct == csv_type::float64 || ct == csv_type::float80; }
bool is_string(csv_type ct) { return ct == csv_type::string; }
bool is_unknown(csv_type ct) { return ct == csv_type::unknown; }

csv_type make_sint(int bits) {
    switch (bits) {
    case 8: return csv_type::int8;
    case 16: return csv_type::int16;
    case 32: return csv_type::int32;
    case 64: return csv_type::int64;
    default: assert(false); return csv_type::unknown;
    }
}
csv_type make_uint(int bits) {
    switch (bits) {
    case 8: return csv_type::uint8;
    case 16: return csv_type::uint16;
    case 32: return csv_type::uint32;
    case 64: return csv_type::uint64;
    default: assert(false); return csv_type::unknown;
    }
}

void accum_line_types(csv_type_vector& accum_types, csv_type_vector const& line_types) {
    for (size_t i = 0; i < std::min(accum_types.size(), line_types.size()); ++i) {
        csv_type const afld = accum_types[i];
        csv_type const lfld = line_types[i];
        if (is_int(afld)) {
            if (is_int(lfld)) {
                // larger signed int?
                if (is_sint(afld) && is_sint(lfld)) {
                    if (sint_bits(afld) < sint_bits(lfld))
                        accum_types[i] = lfld;
                }
                // larger unsigned int?
                else if (is_uint(afld) && is_uint(lfld)) {
                    if (uint_bits(afld) < uint_bits(lfld))
                        accum_types[i] = lfld;
                }
                // mixed signed & unsigned
                else {
                    // make it signed, with enough bits for both
                    accum_types[i] = make_sint(std::max(sint_bits(afld), uint_bits(lfld)));
                }
            }
            else if (is_boolean(lfld)) {
                accum_types[i] = csv_type::string;
            }
            else if (is_float(lfld)) {
                accum_types[i] = lfld; // float overrides int
            }
            else if (is_string(lfld)) {
                accum_types[i] = lfld; // string overrides int
            }
            else if (is_unknown(lfld)) {
                // leave as-is
            }
            else {
                assert(false); // unexpected
            }
        }
        else if (is_boolean(afld)) {
            if (is_boolean(lfld) || is_unknown(lfld)) {
                // leave as-is
            }
            else {
                accum_types[i] = csv_type::string;
            }
        }
        else if (is_float(afld)) {
            if (is_string(lfld))
                accum_types[i] = lfld; // string overrides float
            else if (is_boolean(lfld))
                accum_types[i] = csv_type::string;
        }
        else if (is_string(afld)) {
            // once a string, always a string
        }
        else if (is_unknown(afld)) {
            accum_types[i] = lfld;
        }
        else {
            assert(false); // unexpected
        }
    }

    // append new columns, if any
    for (size_t i = accum_types.size(); i < line_types.size(); ++i)
        accum_types.push_back(line_types[i]);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
