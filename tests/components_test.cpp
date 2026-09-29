#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "logflow/ConsoleSink.hpp"
#include "logflow/FileLineSource.hpp"
#include "logflow/Pipeline.hpp"
#include "mini_test.hpp"
#include "test_support.hpp"

#ifndef LOGFLOW_SAMPLE_LOG
#define LOGFLOW_SAMPLE_LOG "data/access-small.log"
#endif

using namespace support;
using Strings = std::vector<std::string>;

// ---------------- FileLineSource ----------------

TEST(file_line_source_emits_one_record_per_line) {
    TempFile f("lines", "one\ntwo\nthree\n");
    CHECK_EQ(readLinesViaPipeline(f.path()), (Strings{"one", "two", "three"}));
}

TEST(file_line_source_emits_a_final_line_without_newline) {
    TempFile f("nonl", "one\ntwo");
    CHECK_EQ(readLinesViaPipeline(f.path()), (Strings{"one", "two"}));
}

TEST(file_line_source_preserves_blank_lines) {
    TempFile f("blank", "a\n\nb\n");
    CHECK_EQ(readLinesViaPipeline(f.path()), (Strings{"a", "", "b"}));
}

TEST(file_line_source_strips_windows_line_endings) {
    TempFile f("crlf", "a\r\nb\r\n");
    CHECK_EQ(readLinesViaPipeline(f.path()), (Strings{"a", "b"}));
}

TEST(file_line_source_emits_nothing_for_an_empty_file) {
    TempFile f("empty", "");
    CHECK_EQ(readLinesViaPipeline(f.path()), Strings{});
}

TEST(file_line_source_reports_a_missing_file) {
    CHECK_THROWS(readLinesViaPipeline("definitely/not/here.log"), std::runtime_error);
}

// ---------------- ConsoleSink ----------------

TEST(console_sink_writes_each_record_on_its_own_line) {
    std::ostringstream out;
    logflow::ConsoleSink sink(out);
    sink.consume("first");
    sink.consume("");
    sink.consume("third");
    CHECK_EQ(out.str(), std::string("first\n\nthird\n"));
}

TEST(console_sink_reports_a_failed_write) {
    std::ostringstream out;
    out.setstate(std::ios::badbit);
    logflow::ConsoleSink sink(out);
    CHECK_THROWS(sink.consume("x"), std::runtime_error);
}

// ---------------- End to end: the week-one requirement ----------------

TEST(sample_log_is_reproduced_line_for_line_through_the_pipeline) {
    // Independent reference: read the file the plain way.
    std::ifstream in(LOGFLOW_SAMPLE_LOG);
    CHECK(in.good());
    Strings expected;
    std::string line;
    while (std::getline(in, line)) expected.push_back(line);
    CHECK(expected.size() >= 100);

    CHECK_EQ(readLinesViaPipeline(LOGFLOW_SAMPLE_LOG), expected);
}

TEST(sample_log_printed_by_the_console_sink_is_byte_identical_to_the_file) {
    std::ifstream in(LOGFLOW_SAMPLE_LOG, std::ios::binary);
    std::ostringstream original;
    original << in.rdbuf();

    std::ostringstream printed;
    logflow::from(std::make_shared<logflow::FileLineSource>(LOGFLOW_SAMPLE_LOG))
        .to(std::make_shared<logflow::ConsoleSink>(printed))
        .run();
    CHECK_EQ(printed.str(), original.str());
}
