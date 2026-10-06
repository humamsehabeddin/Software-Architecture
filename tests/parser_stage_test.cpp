// ParserStage tests. None of them touches the file system: each builds the
// stage, hands it one string, and reads what it emitted from a
// CollectingEmitter (see test_support.hpp).

#include <chrono>
#include <string>

#include "logflow/ParserStage.hpp"
#include "mini_test.hpp"
#include "test_support.hpp"

using namespace support;
using logflow::LogRecord;
using logflow::ParserStage;

namespace {

const char* const kValid =
    "203.0.113.240 - - [21/Sep/2026:09:00:00 +0000] \"GET /products/42 HTTP/1.1\" 404 8301 "
    "\"-\" \"Mozilla/5.0 (Macintosh; Intel Mac OS X 14_4) Safari/605.1.15\"";

long long epochSeconds(const LogRecord& r) {
    return std::chrono::duration_cast<std::chrono::seconds>(r.timestamp.time_since_epoch())
        .count();
}

struct Run {
    ParserStage stage;
    CollectingEmitter<LogRecord> out;
    explicit Run(const std::string& line) { stage.process(line, out); }
};

}  // namespace

TEST(parser_turns_a_valid_line_into_a_record) {
    Run r(kValid);
    CHECK_EQ(r.out.items.size(), std::size_t(1));
    const LogRecord& rec = r.out.items[0];
    CHECK_EQ(epochSeconds(rec), 1789981200LL);  // 2026-09-21T09:00:00Z
    CHECK_EQ(rec.clientIp, std::string("203.0.113.240"));
    CHECK_EQ(rec.method, std::string("GET"));
    CHECK_EQ(rec.path, std::string("/products/42"));
    CHECK_EQ(rec.status, 404);
    CHECK_EQ(rec.bytes, std::int64_t(8301));
    CHECK_EQ(rec.userAgent, std::string("Mozilla/5.0 (Macintosh; Intel Mac OS X 14_4) Safari/605.1.15"));
    CHECK_EQ(rec.raw, std::string(kValid));
    CHECK(rec.attributes.empty());
    CHECK_EQ(r.stage.parsedCount(), std::size_t(1));
    CHECK_EQ(r.stage.malformedCount(), std::size_t(0));
}

TEST(parser_skips_and_counts_a_line_with_a_missing_field) {
    // status and bytes are missing
    Run r("192.0.2.10 - - [21/Sep/2026:09:00:03 +0000] \"GET /api/orders HTTP/1.1\"");
    CHECK(r.out.items.empty());
    CHECK_EQ(r.stage.malformedCount(), std::size_t(1));
}

TEST(parser_skips_and_counts_a_line_with_an_incomplete_request) {
    Run r("192.0.2.10 - - [21/Sep/2026:09:00:03 +0000] \"GET /api/orders\" 200 10");
    CHECK(r.out.items.empty());
    CHECK_EQ(r.stage.malformedCount(), std::size_t(1));
}

TEST(parser_skips_and_counts_a_bad_timestamp) {
    const char* bad[] = {
        "1.1.1.1 - - [21/Foo/2026:09:00:00 +0000] \"GET / HTTP/1.1\" 200 1",   // month
        "1.1.1.1 - - [31/Feb/2026:09:00:00 +0000] \"GET / HTTP/1.1\" 200 1",   // no such day
        "1.1.1.1 - - [21/Sep/2026:25:00:00 +0000] \"GET / HTTP/1.1\" 200 1",   // hour
        "1.1.1.1 - - [yesterday] \"GET / HTTP/1.1\" 200 1",                    // not a date
        "1.1.1.1 - - 21/Sep/2026:09:00:00 +0000 \"GET / HTTP/1.1\" 200 1",     // no brackets
    };
    ParserStage stage;
    CollectingEmitter<LogRecord> out;
    for (const char* line : bad) stage.process(line, out);
    CHECK(out.items.empty());
    CHECK_EQ(stage.malformedCount(), std::size_t(5));
}

TEST(parser_skips_and_counts_a_bad_status_code) {
    const char* bad[] = {
        "1.1.1.1 - - [21/Sep/2026:09:00:00 +0000] \"GET / HTTP/1.1\" abc 1",
        "1.1.1.1 - - [21/Sep/2026:09:00:00 +0000] \"GET / HTTP/1.1\" 20 1",
        "1.1.1.1 - - [21/Sep/2026:09:00:00 +0000] \"GET / HTTP/1.1\" 999 1",
        "1.1.1.1 - - [21/Sep/2026:09:00:00 +0000] \"GET / HTTP/1.1\" 2x0 1",
    };
    ParserStage stage;
    CollectingEmitter<LogRecord> out;
    for (const char* line : bad) stage.process(line, out);
    CHECK(out.items.empty());
    CHECK_EQ(stage.malformedCount(), std::size_t(4));
}

TEST(parser_skips_and_counts_blank_lines) {
    ParserStage stage;
    CollectingEmitter<LogRecord> out;
    stage.process("", out);
    stage.process("   \t ", out);
    CHECK(out.items.empty());
    CHECK_EQ(stage.malformedCount(), std::size_t(2));
}

TEST(parser_tolerates_extra_whitespace_between_fields) {
    Run r("  192.0.2.1   -  -   [21/Sep/2026:09:00:00 +0000]   \"GET /a HTTP/1.1\"  200   12  "
          "\"-\"   \"curl/8.4\"   ");
    CHECK_EQ(r.out.items.size(), std::size_t(1));
    CHECK_EQ(r.out.items[0].clientIp, std::string("192.0.2.1"));
    CHECK_EQ(r.out.items[0].path, std::string("/a"));
    CHECK_EQ(r.out.items[0].status, 200);
    CHECK_EQ(r.out.items[0].bytes, std::int64_t(12));
    CHECK_EQ(r.out.items[0].userAgent, std::string("curl/8.4"));
}

TEST(parser_keeps_spaces_inside_a_quoted_user_agent) {
    Run r("1.1.1.1 - - [21/Sep/2026:09:00:00 +0000] \"GET / HTTP/1.1\" 200 5 \"-\" "
          "\"My  Browser 1.0 (X11; Linux)\"");
    CHECK_EQ(r.out.items.size(), std::size_t(1));
    CHECK_EQ(r.out.items[0].userAgent, std::string("My  Browser 1.0 (X11; Linux)"));
}

TEST(parser_unescapes_a_quote_inside_the_user_agent) {
    Run r("1.1.1.1 - - [21/Sep/2026:09:00:00 +0000] \"GET / HTTP/1.1\" 200 5 \"-\" "
          "\"say \\\"hi\\\"\"");
    CHECK_EQ(r.out.items.size(), std::size_t(1));
    CHECK_EQ(r.out.items[0].userAgent, std::string("say \"hi\""));
}

TEST(parser_keeps_the_query_string_as_part_of_the_path) {
    Run r("1.1.1.1 - - [21/Sep/2026:09:00:00 +0000] \"GET /search?q=log+flow&page=2 HTTP/1.1\" "
          "200 5 \"-\" \"curl/8.4\"");
    CHECK_EQ(r.out.items.size(), std::size_t(1));
    CHECK_EQ(r.out.items[0].path, std::string("/search?q=log+flow&page=2"));
}

TEST(parser_accepts_plain_common_log_format_without_referer_and_user_agent) {
    Run r("1.1.1.1 - frank [21/Sep/2026:09:00:00 +0000] \"POST /login HTTP/1.1\" 302 0");
    CHECK_EQ(r.out.items.size(), std::size_t(1));
    CHECK_EQ(r.out.items[0].method, std::string("POST"));
    CHECK_EQ(r.out.items[0].userAgent, std::string(""));
}

TEST(parser_reads_a_dash_in_the_bytes_field_as_zero) {
    Run r("1.1.1.1 - - [21/Sep/2026:09:00:00 +0000] \"GET / HTTP/1.1\" 304 - \"-\" \"x\"");
    CHECK_EQ(r.out.items.size(), std::size_t(1));
    CHECK_EQ(r.out.items[0].bytes, std::int64_t(0));
}

TEST(parser_converts_the_time_zone_offset_to_utc) {
    Run plus("1.1.1.1 - - [21/Sep/2026:12:00:00 +0300] \"GET / HTTP/1.1\" 200 1");
    Run minus("1.1.1.1 - - [21/Sep/2026:04:30:00 -0430] \"GET / HTTP/1.1\" 200 1");
    CHECK_EQ(epochSeconds(plus.out.items.at(0)), 1789981200LL);   // 09:00:00Z
    CHECK_EQ(epochSeconds(minus.out.items.at(0)), 1789981200LL + 0);  // 09:00:00Z
}

TEST(parser_handles_a_leap_day) {
    Run r("1.1.1.1 - - [29/Feb/2024:23:59:59 +0000] \"GET / HTTP/1.1\" 200 1");
    CHECK_EQ(epochSeconds(r.out.items.at(0)), 1709251199LL);
    Run notLeap("1.1.1.1 - - [29/Feb/2026:23:59:59 +0000] \"GET / HTTP/1.1\" 200 1");
    CHECK(notLeap.out.items.empty());
}

TEST(parser_rejects_an_unterminated_quote_and_trailing_junk) {
    ParserStage stage;
    CollectingEmitter<LogRecord> out;
    stage.process("1.1.1.1 - - [21/Sep/2026:09:00:00 +0000] \"GET / HTTP/1.1\" 200 1 \"-\" \"oops", out);
    stage.process("1.1.1.1 - - [21/Sep/2026:09:00:00 +0000] \"GET / HTTP/1.1\" 200 1 junk", out);
    CHECK(out.items.empty());
    CHECK_EQ(stage.malformedCount(), std::size_t(2));
}

TEST(parser_keeps_going_after_a_malformed_line) {
    ParserStage stage;
    CollectingEmitter<LogRecord> out;
    stage.process(kValid, out);
    stage.process("garbage", out);
    stage.process(kValid, out);
    CHECK_EQ(out.items.size(), std::size_t(2));
    CHECK_EQ(stage.parsedCount(), std::size_t(2));
    CHECK_EQ(stage.malformedCount(), std::size_t(1));
}
