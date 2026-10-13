#include <string>
#include <vector>

#include "logflow/CommandLine.hpp"
#include "mini_test.hpp"

using logflow::parseArguments;
using Args = std::vector<std::string>;

TEST(cli_accepts_just_an_input_file) {
    const auto o = parseArguments({"in.log"});
    CHECK_EQ(o.input, std::string("in.log"));
    CHECK(!o.output && !o.minStatus && !o.pathPrefix);
    CHECK(o.fields.empty());
}

TEST(cli_reads_all_options_in_any_position) {
    const auto o = parseArguments({"--min-status", "400", "in.log", "--path-prefix", "/api",
                                   "--fields", "timestamp,clientIp,status", "--output", "out.csv"});
    CHECK_EQ(o.input, std::string("in.log"));
    CHECK_EQ(*o.minStatus, 400);
    CHECK_EQ(*o.pathPrefix, std::string("/api"));
    CHECK_EQ(*o.output, std::string("out.csv"));
    CHECK_EQ(o.fields, (Args{"timestamp", "clientIp", "status"}));
}

TEST(cli_rejects_a_missing_or_second_input_file) {
    CHECK_THROWS(parseArguments({}), std::invalid_argument);
    CHECK_THROWS(parseArguments({"--min-status", "400"}), std::invalid_argument);
    CHECK_THROWS(parseArguments({"a.log", "b.log"}), std::invalid_argument);
}

TEST(cli_rejects_unknown_options_and_missing_values) {
    CHECK_THROWS(parseArguments({"in.log", "--verbose", "1"}), std::invalid_argument);
    CHECK_THROWS(parseArguments({"in.log", "--min-status"}), std::invalid_argument);
}

TEST(cli_rejects_a_malformed_status) {
    for (const char* bad : {"abc", "40", "4000", "099", "600", "-1", ""}) {
        CHECK_THROWS(parseArguments({"in.log", "--min-status", bad}), std::invalid_argument);
    }
}

TEST(cli_rejects_unknown_fields_and_repeated_options) {
    CHECK_THROWS(parseArguments({"in.log", "--fields", "status,colour"}), std::invalid_argument);
    CHECK_THROWS(parseArguments({"in.log", "--fields", "status,"}), std::invalid_argument);
    CHECK_THROWS(parseArguments({"in.log", "--min-status", "400", "--min-status", "500"}),
                 std::invalid_argument);
}

TEST(cli_refuses_to_overwrite_the_input_file) {
    CHECK_THROWS(parseArguments({"in.log", "--output", "in.log"}), std::invalid_argument);
}
