#pragma once

#include <variant>
#include <string>
#include <string_view>
#include <vector>
#include <ranges>

template<typename Rng>
concept char_range = std::ranges::random_access_range<Rng> && std::is_same_v<std::ranges::range_value_t<Rng>, char>;
template<typename Iterator>
concept char_iterator = std::random_access_iterator<Iterator> && std::is_same_v<typename Iterator::value_type, char>;

enum class int_type : int8_t { decimal, hexidecimal, undefined };

using csv_value = std::variant<bool, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double, long double, std::string>;
using csv_value_vector = std::vector<csv_value>;

enum class csv_type : int8_t { boolean, int8, uint8, int16, uint16, int32, uint32, int64, uint64, float32, float64, float80, string, unknown };
using csv_type_vector = std::vector<csv_type>;

using csv_name = std::string;
using csv_name_vector = std::vector<csv_name>;

/// <summary>
/// Descriptor of the CSV file contents that the user can provide.
/// </summary>
enum class csv_flags : int16_t {
	has_header_row = 0x01,
	no_header_row = 0x20,
	detect_header_row = 0x00,
	header_mask = (has_header_row | no_header_row),

	skip_empty_lines = 0x0004,
	include_empty_lines = 0x0000,

	allow_only_fixed_column_count = 0x0008,
	allow_variable_column_count = 0x0000,

	detect_signed_int = 0x0010,    // has decimal digits 0..9, with optional lead of + or -
	detect_unsigned_int = 0x0020,  // has hex digits 0..9, a..f|A..F, with optional lead of 0x or 0X
	detect_any_int = (detect_signed_int | detect_unsigned_int),

	detect_true_false_bool = 0x0040, // true/false interpreted as boolean (case-insensitive)
	detect_yes_no_bool = 0x0080,     // yes/no interpreted as boolean (case-insensitive)
	detect_integer_bool = 0x0100,    // integer values mixed with true/false & yes/no in same column
	detect_any_bool = (detect_true_false_bool | detect_yes_no_bool | detect_integer_bool),

	none = 0x00,
	header_default = has_header_row | skip_empty_lines | allow_variable_column_count | detect_any_int | detect_any_bool,
	no_header_defaults = no_header_row | skip_empty_lines | allow_variable_column_count | detect_any_int | detect_any_bool
};
csv_flags& operator&=(csv_flags& lhs, csv_flags rhs);
csv_flags operator&(csv_flags lhs, csv_flags rhs);
csv_flags operator|(csv_flags lhs, csv_flags rhs);

csv_type smallest_int_type(const char* first, const char* last);
csv_type smallest_uint_type(const char* first, const char* last, int base = 10);
csv_type smallest_float_type(const char* first, const char* last);

/// <summary>
/// Accumulate the types from two lines to determine a common type that will hold both.
/// </summary>
/// <param name="accum_types">The accumulated types so far.</param>
/// <param name="line_types">The new set of types to accumulate into accum_types.</param>
void accum_line_types(csv_type_vector& accum_types, csv_type_vector const& line_types);

/// <summary>
/// Classify a character range as one of the types defined in csv_value enum.
/// </summary>
/// <param name="chars">Range of characters to evaluate</param>
/// <param name="flags">Hints as to what to excpect/accept</param>
/// <returns>The best type that the character can be assigned to. For integers, it's the smallest integer type that
/// can hold the value. If detect_true_false_bool or detect_yes_no_bool is passed then true/false or yes/no values will be treated
/// as boolean. For floating point values, only double is supported. If the characters can't be matched to any of
/// those types it defaults to string.</returns> 
template<char_range Rng>
auto match_type(Rng const& chars, csv_flags flags) -> csv_type {
	auto first = begin(chars);
	auto last = end(chars);
	csv_type fld_type = csv_type::unknown;
	// empty
	if (last - first == 0)
		return csv_type::unknown;

	// bool: true/false
	if ((flags & csv_flags::detect_true_false_bool) == csv_flags::detect_true_false_bool) {
		if ((last - first == 4) && (first[0] == 'T' || first[0] == 't') && (first[1] == 'R' || first[1] == 'r') && (first[2] == 'U' || first[2] == 'u') && (first[3] == 'E' || first[3] == 'e'))
			return csv_type::boolean;
		if ((last - first == 5) && (first[0] == 'F' || first[0] == 'f') && (first[1] == 'A' || first[1] == 'a') && (first[2] == 'L' || first[2] == 'l') && (first[3] == 'S' || first[3] == 's') && (first[4] == 'E' || first[4] == 'e'))
			return csv_type::boolean;
	}
	// bool: yes/no
	if ((flags & csv_flags::detect_yes_no_bool) == csv_flags::detect_yes_no_bool) {
		if ((last - first == 3) && (first[0] == 'Y' || first[0] == 'y') && (first[1] == 'E' || first[1] == 'e') && (first[2] == 'S' || first[2] == 's'))
			return csv_type::boolean;
		if ((last - first == 2) && (first[0] == 'N' || first[0] == 'n') && (first[1] == 'O' || first[1] == 'o'))
			return csv_type::boolean;
	}

	// hex value?
	if (last - first >= 2 && first[0] == '0' && (first[1] == 'x' || first[1] == 'X')) {
		if ((first += 2) == last)
			return csv_type::string;
		fld_type = smallest_uint_type(&*first, &*first + (last - first), 16);
		if (fld_type != csv_type::unknown)
			return fld_type;
	}
	// chop leading +
	if ((last - first) > 0 && first[0] == '+')
		if (++first == last)
			return csv_type::string; // isolated '+'
	// signed integer?
	fld_type = smallest_int_type(&*first, &*first + (last - first));
	if (fld_type != csv_type::unknown)
		return fld_type;
	// floating point
	fld_type = smallest_float_type(&*first, &*first + (last - first));
	if (fld_type != csv_type::unknown)
		return fld_type;

	// anything else defaults to string
	return csv_type::string;
}

// LineRange: *iterator = string_view

template<typename Fnc>
void parse_line(std::string_view const& rng
	, std::string const& sep_charset
	, std::string const& quote_lead_symbol
	, std::string const& quote_trail_symbol
	, std::string const& whitesp_charset
	, csv_flags          flags
	, Fnc& action) {

}

csv_type_vector eval_line_types(std::string_view const& rng
	, std::string const& sep_charset
	, std::string const& quote_lead_symbol
	, std::string const& quote_trail_symbol
	, std::string const& whitesp_charset
	, csv_flags          flags);


template<std::ranges::forward_range LineRange, typename ColsOutIter>
std::pair< csv_name_vector, csv_type_vector> sample_lines(const LineRange& lines, int max_lines, csv_flags flags) {
	csv_name_vector col_names;
	csv_type_vector col_types;

}

struct csv_row {
	const csv_name_vector& col_names;
	const csv_type_vector& col_types;
	csv_value_vector& col_values;

	csv_row(const csv_name_vector& names, const csv_type_vector& types, csv_value_vector& values) : col_names(names), col_types(types), col_values(values) {}
	csv_row(const csv_row&) = default;
	~csv_row() = default;

	csv_row& operator=(const csv_row&) = default;
};

template<std::ranges::forward_range LineRange, typename ColsOutIter>
requires std::output_iterator<ColsOutIter, csv_row>
auto csv_reader(const LineRange& lines, ColsOutIter cols, int prescan_lines = 100, csv_flags flags = csv_flags::defaults) {
	// Phase 1: scan n rows to determine column names & types
	// Phase 2: read rows and output values
}
