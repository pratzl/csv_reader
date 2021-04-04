#include "csv_test.hpp"
#include <cassert>

using namespace std::literals;

using std::string;

void test_match_type() {
    csv_flags flags = csv_flags::detect_true_false_bool | csv_flags::detect_yes_no_bool;
    assert(match_type(""s, flags) == csv_type::unknown);
    assert(match_type("true"s, flags) == csv_type::boolean);
    assert(match_type("TRUE"s, flags) == csv_type::boolean);
    assert(match_type("false"s, flags) == csv_type::boolean);
    assert(match_type("FALSE"s, flags) == csv_type::boolean);
    assert(match_type("0x0A"s, flags) == csv_type::uint8);
    assert(match_type("0xFFFF"s, flags) == csv_type::uint16);
    assert(match_type("0"s, flags) == csv_type::int8);
    assert(match_type("-1000"s, flags) == csv_type::int16);
    assert(match_type("+1000"s, flags) == csv_type::int16);
    assert(match_type("3.123"s, flags) == csv_type::float64);
    assert(match_type("3e123"s, flags) == csv_type::float64);
}

void test_eval_line_types() {
    string const sep_charset = ",";
    string const qlead_sym = "\"";
    string const qtrail_sym = "\"";
    string const ws_charset = " \t";
    csv_flags flags = csv_flags::detect_true_false_bool | csv_flags::detect_yes_no_bool;
    //csv_type_vector ctv = eval_line_types("", sep_charset, qlead_sym, qtrail_sym, ws_charset);
    assert(eval_line_types("", sep_charset, qlead_sym, qtrail_sym, ws_charset, flags) == csv_type_vector{ csv_type::unknown });
    assert(eval_line_types("abc", sep_charset, qlead_sym, qtrail_sym, ws_charset, flags) == csv_type_vector{ csv_type::string });
    assert(eval_line_types("\"xyz\"", sep_charset, qlead_sym, qtrail_sym, ws_charset, flags) == csv_type_vector{ csv_type::string });
    assert(eval_line_types("123", sep_charset, qlead_sym, qtrail_sym, ws_charset, flags) == csv_type_vector{ csv_type::int8 });
    assert(eval_line_types("3.", sep_charset, qlead_sym, qtrail_sym, ws_charset, flags) == csv_type_vector{ csv_type::float64 });
    assert(eval_line_types("0xffff", sep_charset, qlead_sym, qtrail_sym, ws_charset, flags) == csv_type_vector{ csv_type::uint16 });

    csv_type_vector ctv1{ csv_type::int8,csv_type::float64, csv_type::string, csv_type::uint8 };
    assert(eval_line_types("1, 2.3, \"abc\", 0x00", sep_charset, qlead_sym, qtrail_sym, ws_charset, flags) == ctv1);
}

void test_accum_line_types() {
    // test no change when types identicai 
    csv_type_vector a01{ csv_type::int8, csv_type::int16, csv_type::int32, csv_type::int64 };
    csv_type_vector b01{ csv_type::int8, csv_type::int16, csv_type::int32, csv_type::int64 };
    csv_type_vector x01{ csv_type::int8, csv_type::int16, csv_type::int32, csv_type::int64 };
    accum_line_types(a01, b01);
    assert(a01 == x01);

    csv_type_vector a02{ csv_type::uint8, csv_type::uint16, csv_type::uint32, csv_type::uint64 };
    csv_type_vector b02{ csv_type::uint8, csv_type::uint16, csv_type::uint32, csv_type::uint64 };
    csv_type_vector x02{ csv_type::uint8, csv_type::uint16, csv_type::uint32, csv_type::uint64 };
    accum_line_types(a02, b02);
    assert(a02 == x02);

    csv_type_vector a03{ csv_type::float32, csv_type::float64, csv_type::float80 };
    csv_type_vector b03{ csv_type::float32, csv_type::float64, csv_type::float80 };
    csv_type_vector x03{ csv_type::float32, csv_type::float64, csv_type::float80 };
    accum_line_types(a03, b03);
    assert(a03 == x03);

    csv_type_vector a04{ csv_type::boolean, csv_type::string, csv_type::unknown };
    csv_type_vector b04{ csv_type::boolean, csv_type::string, csv_type::unknown };
    csv_type_vector x04{ csv_type::boolean, csv_type::string, csv_type::unknown };
    accum_line_types(a04, b04);
    assert(a04 == x04);
    accum_line_types(a04, b04);
    assert(a04 == x04);

    // promotion to bigger int
    csv_type_vector a05{ csv_type::int8, csv_type::int8, csv_type::int8, csv_type::int8 };
    csv_type_vector b05{ csv_type::int8, csv_type::int16, csv_type::int32, csv_type::int64 };
    csv_type_vector x05{ csv_type::int8, csv_type::int16, csv_type::int32, csv_type::int64 };
    accum_line_types(a05, b05);
    assert(a05 == x05);

    csv_type_vector a06{ csv_type::int16, csv_type::int16, csv_type::int16, csv_type::int16 };
    csv_type_vector b06{ csv_type::int8, csv_type::int16, csv_type::int32, csv_type::int64 };
    csv_type_vector x06{ csv_type::int16, csv_type::int16, csv_type::int32, csv_type::int64 };
    accum_line_types(a06, b06);
    assert(a06 == x06);

    csv_type_vector a07{ csv_type::int32, csv_type::int32, csv_type::int32, csv_type::int32 };
    csv_type_vector b07{ csv_type::int8, csv_type::int16, csv_type::int32, csv_type::int64 };
    csv_type_vector x07{ csv_type::int32, csv_type::int32, csv_type::int32, csv_type::int64 };
    accum_line_types(a07, b07);
    assert(a07 == x07);

    csv_type_vector a08{ csv_type::int64, csv_type::int64, csv_type::int64, csv_type::int64 };
    csv_type_vector b08{ csv_type::int8, csv_type::int16, csv_type::int32, csv_type::int64 };
    csv_type_vector x08{ csv_type::int64, csv_type::int64, csv_type::int64, csv_type::int64 };
    accum_line_types(a08, b08);
    assert(a08 == x08);

    // promotion to bigger uint
    csv_type_vector a09{ csv_type::uint8, csv_type::uint8, csv_type::uint8, csv_type::uint8 };
    csv_type_vector b09{ csv_type::uint8, csv_type::uint16, csv_type::uint32, csv_type::uint64 };
    csv_type_vector x09{ csv_type::uint8, csv_type::uint16, csv_type::uint32, csv_type::uint64 };
    accum_line_types(a09, b09);
    assert(a09 == x09);

    csv_type_vector a10{ csv_type::uint16, csv_type::uint16, csv_type::uint16, csv_type::uint16 };
    csv_type_vector b10{ csv_type::uint8, csv_type::uint16, csv_type::uint32, csv_type::uint64 };
    csv_type_vector x10{ csv_type::uint16, csv_type::uint16, csv_type::uint32, csv_type::uint64 };
    accum_line_types(a10, b10);
    assert(a10 == x10);

    csv_type_vector a11{ csv_type::uint32, csv_type::uint32, csv_type::uint32, csv_type::uint32 };
    csv_type_vector b11{ csv_type::uint8, csv_type::uint16, csv_type::uint32, csv_type::uint64 };
    csv_type_vector x11{ csv_type::uint32, csv_type::uint32, csv_type::uint32, csv_type::uint64 };
    accum_line_types(a11, b11);
    assert(a11 == x11);

    csv_type_vector a12{ csv_type::uint64, csv_type::uint64, csv_type::uint64, csv_type::uint64 };
    csv_type_vector b12{ csv_type::uint8, csv_type::uint16, csv_type::uint32, csv_type::uint64 };
    csv_type_vector x12{ csv_type::uint64, csv_type::uint64, csv_type::uint64, csv_type::uint64 };
    accum_line_types(a12, b12);
    assert(a12 == x12);

    // different types
    csv_type_vector a13{ csv_type::int8,   csv_type::int8,    csv_type::int8,    csv_type::int8, csv_type::int8 };
    csv_type_vector b13{ csv_type::uint64, csv_type::boolean, csv_type::float64, csv_type::string, csv_type::unknown };
    csv_type_vector x13{ csv_type::int64,  csv_type::string,  csv_type::float64, csv_type::string,  csv_type::int8 };
    accum_line_types(a13, b13);
    assert(a13 == x13);

    csv_type_vector a14{ csv_type::uint8,  csv_type::uint8,   csv_type::uint8,   csv_type::uint8,  csv_type::uint8 };
    csv_type_vector b14{ csv_type::uint64, csv_type::boolean, csv_type::float64, csv_type::string, csv_type::unknown };
    csv_type_vector x14{ csv_type::uint64, csv_type::string,  csv_type::float64, csv_type::string, csv_type::uint8 };
    accum_line_types(a14, b14);
    assert(a14 == x14);

    csv_type_vector a15{ csv_type::boolean, csv_type::boolean, csv_type::boolean, csv_type::boolean, csv_type::boolean };
    csv_type_vector b15{ csv_type::uint64, csv_type::boolean, csv_type::float64, csv_type::string, csv_type::unknown };
    csv_type_vector x15{ csv_type::string, csv_type::boolean,  csv_type::string, csv_type::string, csv_type::boolean };
    accum_line_types(a15, b15);
    assert(a15 == x15);

    csv_type_vector a16{ csv_type::float64,  csv_type::float64,   csv_type::float64,   csv_type::float64,  csv_type::float64 };
    csv_type_vector b16{ csv_type::uint64, csv_type::boolean, csv_type::float64, csv_type::string, csv_type::unknown };
    csv_type_vector x16{ csv_type::float64, csv_type::string,  csv_type::float64, csv_type::string, csv_type::float64 };
    accum_line_types(a16, b16);
    assert(a16 == x16);

    csv_type_vector a17{ csv_type::string,  csv_type::string,   csv_type::string,   csv_type::string,  csv_type::string };
    csv_type_vector b17{ csv_type::uint64, csv_type::boolean, csv_type::float64, csv_type::string, csv_type::unknown };
    csv_type_vector x17{ csv_type::string, csv_type::string,  csv_type::string, csv_type::string, csv_type::string };
    accum_line_types(a17, b17);
    assert(a17 == x17);

    csv_type_vector a18{ csv_type::unknown,  csv_type::unknown,   csv_type::unknown,   csv_type::unknown,  csv_type::unknown };
    csv_type_vector b18{ csv_type::uint64, csv_type::boolean, csv_type::float64, csv_type::string, csv_type::unknown };
    csv_type_vector x18{ csv_type::uint64, csv_type::boolean, csv_type::float64, csv_type::string, csv_type::unknown };
    accum_line_types(a18, b18);
    assert(a18 == x18);

    // Diff col count
    csv_type_vector a19{ csv_type::string,  csv_type::float64 };
    csv_type_vector b19{ csv_type::string, csv_type::float64, csv_type::unknown };
    csv_type_vector x19{ csv_type::string, csv_type::float64, csv_type::unknown };
    accum_line_types(a19, b19);
    assert(a19 == x19);

    csv_type_vector a20{ csv_type::string,  csv_type::float64 };
    csv_type_vector b20{ csv_type::string };
    csv_type_vector x20{ csv_type::string, csv_type::float64 };
    accum_line_types(a20, b20);
    assert(a20 == x20);
}

