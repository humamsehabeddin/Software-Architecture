#include <algorithm>
#include <chrono>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "logflow/ConsoleSink.hpp"
#include "logflow/FileLineSource.hpp"
#include "logflow/ParserStage.hpp"
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

namespace {
logflow::LogRecord sampleRecord(const std::string& agent = "Safari/17.4") {
    return logflow::LogRecord(
        logflow::LogRecord::Timestamp(std::chrono::seconds(1789981200)),  // 2026-09-21T09:00:00Z
        "203.0.113.240", "GET", "/products/42", 404, 8301, agent, {}, "raw line");
}
}  // namespace

TEST(console_sink_formats_a_record_as_one_readable_line) {
    CHECK_EQ(logflow::ConsoleSink::format(sampleRecord()),
             std::string("2026-09-21T09:00:00Z 203.0.113.240 GET /products/42 -> 404 8301B "
                         "\"Safari/17.4\""));
}

TEST(console_sink_writes_each_record_on_its_own_line) {
    std::ostringstream out;
    logflow::ConsoleSink sink(out);
    sink.consume(sampleRecord("a"));
    sink.consume(sampleRecord("b"));
    CHECK_EQ(out.str(),
             std::string("2026-09-21T09:00:00Z 203.0.113.240 GET /products/42 -> 404 8301B \"a\"\n"
                         "2026-09-21T09:00:00Z 203.0.113.240 GET /products/42 -> 404 8301B \"b\"\n"));
}

TEST(console_sink_reports_a_failed_write) {
    std::ostringstream out;
    out.setstate(std::ios::badbit);
    logflow::ConsoleSink sink(out);
    CHECK_THROWS(sink.consume(sampleRecord()), std::runtime_error);
}

// ---------------- End to end: the sample log ----------------

TEST(sample_log_is_read_line_for_line_by_the_source) {
    // Independent reference: read the file the plain way.
    std::ifstream in(LOGFLOW_SAMPLE_LOG);
    CHECK(in.good());
    Strings expected;
    std::string line;
    while (std::getline(in, line)) expected.push_back(line);
    CHECK(expected.size() >= 100);

    CHECK_EQ(readLinesViaPipeline(LOGFLOW_SAMPLE_LOG), expected);
}

TEST(sample_log_parses_completely_into_one_console_line_per_record) {
    const Strings lines = readLinesViaPipeline(LOGFLOW_SAMPLE_LOG);

    auto parser = std::make_shared<logflow::ParserStage>();
    std::ostringstream printed;
    logflow::from(std::make_shared<logflow::FileLineSource>(LOGFLOW_SAMPLE_LOG))
        .then(parser)
        .to(std::make_shared<logflow::ConsoleSink>(printed))
        .run();

    CHECK_EQ(parser->parsedCount(), lines.size());
    CHECK_EQ(parser->malformedCount(), std::size_t(0));
    const std::string text = printed.str();
    CHECK_EQ(std::count(text.begin(), text.end(), '\n'),
             static_cast<std::ptrdiff_t>(lines.size()));
    CHECK(text.compare(0, 20, "2026-09-21T09:00:00Z") == 0);
}

TEST(a_log_with_bad_lines_is_still_processed_and_the_bad_lines_are_counted) {
    TempFile f("mixed", "garbage\n\n"
                        "1.1.1.1 - - [21/Sep/2026:09:00:00 +0000] \"GET / HTTP/1.1\" 200 1\n"
                        "1.1.1.1 - - [21/Sep/2026:09:00:00 +0000] \"GET / HTTP/1.1\" 2oo 1\n");
    auto parser = std::make_shared<logflow::ParserStage>();
    std::ostringstream printed;
    logflow::from(std::make_shared<logflow::FileLineSource>(f.path()))
        .then(parser)
        .to(std::make_shared<logflow::ConsoleSink>(printed))
        .run();
    CHECK_EQ(parser->parsedCount(), std::size_t(1));
    CHECK_EQ(parser->malformedCount(), std::size_t(3));
}
