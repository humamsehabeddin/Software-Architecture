// Integration tests: real pipelines over a small fixture, asserting the
// contents of the output file.

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "logflow/FileLineSource.hpp"
#include "logflow/FileSink.hpp"
#include "logflow/ParserStage.hpp"
#include "logflow/Pipeline.hpp"
#include "logflow/PredicateFilterStage.hpp"
#include "logflow/Predicates.hpp"
#include "logflow/ProjectionStage.hpp"
#include "mini_test.hpp"
#include "test_support.hpp"

#ifndef LOGFLOW_SAMPLE_LOG
#define LOGFLOW_SAMPLE_LOG "data/access-small.log"
#endif

using namespace support;
namespace pred = logflow::predicates;

namespace {

// Seven lines: 3 with status >= 400, 2 of those under /api, one malformed line.
// The user agents contain spaces; one path has a query string.
const char* const kFixture =
    "10.0.0.1 - - [21/Sep/2026:09:00:00 +0000] \"GET /home HTTP/1.1\" 200 512 \"-\" \"Mozilla/5.0 (X11)\"\n"
    "10.0.0.2 - - [21/Sep/2026:09:00:01 +0000] \"GET /api/orders?id=7 HTTP/1.1\" 500 90 \"-\" \"curl/8.0\"\n"
    "this line is not a log line\n"
    "10.0.0.3 - - [21/Sep/2026:09:00:02 +0000] \"POST /api/login HTTP/1.1\" 200 40 \"-\" \"curl/8.0\"\n"
    "10.0.0.4 - - [21/Sep/2026:09:00:03 +0000] \"GET /missing HTTP/1.1\" 404 0 \"-\" \"Googlebot/2.1\"\n"
    "10.0.0.5 - - [21/Sep/2026:09:00:04 +0000] \"GET /api/items HTTP/1.1\" 404 77 \"-\" \"Mozilla/5.0 (Mac)\"\n"
    "10.0.0.6 - - [21/Sep/2026:09:00:05 +0000] \"GET /about HTTP/1.1\" 304 0 \"-\" \"Safari\"\n";

const char* const kExpectedErrorsUnderApi =
    "2026-09-21T09:00:01Z,10.0.0.2,500,/api/orders?id=7\n"
    "2026-09-21T09:00:04Z,10.0.0.5,404,/api/items\n";

}  // namespace

// The assignment's pipeline: read, parse, filter, project, write. Three stages.
TEST(three_stage_pipeline_writes_the_expected_file) {
    TempFile in("it_in", kFixture);
    TempPath out("it_out");
    auto parser = std::make_shared<logflow::ParserStage>();
    auto p = logflow::from(std::make_shared<logflow::FileLineSource>(in.path()))
                 .via(parser)
                 .via(std::make_shared<logflow::PredicateFilterStage>(pred::statusAtLeast(400)))
                 .via(std::make_shared<logflow::ProjectionStage>(
                     std::vector<std::string>{"timestamp", "clientIp", "status", "path"}))
                 .to(std::make_shared<logflow::FileSink>(out.path()));
    p.run();

    CHECK_EQ(p.stageCount(), std::size_t(3));
    CHECK_EQ(parser->malformedCount(), std::size_t(1));
    CHECK_EQ(readFile(out.path()),
             std::string("2026-09-21T09:00:01Z,10.0.0.2,500,/api/orders?id=7\n"
                         "2026-09-21T09:00:03Z,10.0.0.4,404,/missing\n"
                         "2026-09-21T09:00:04Z,10.0.0.5,404,/api/items\n"));
}

TEST(two_filters_and_a_projection_in_either_filter_order_give_the_same_file) {
    TempFile in("it2_in", kFixture);
    auto runWith = [&](bool statusFirst, const std::string& tag) {
        TempPath out(tag);
        auto status = std::make_shared<logflow::PredicateFilterStage>(pred::statusAtLeast(400));
        auto path = std::make_shared<logflow::PredicateFilterStage>(pred::pathStartsWith("/api"));
        auto b = logflow::from(std::make_shared<logflow::FileLineSource>(in.path()))
                     .via(std::make_shared<logflow::ParserStage>());
        b = statusFirst ? b.via(status).via(path) : b.via(path).via(status);
        b.via(std::make_shared<logflow::ProjectionStage>(
               std::vector<std::string>{"timestamp", "clientIp", "status", "path"}))
            .to(std::make_shared<logflow::FileSink>(out.path()))
            .run();
        return readFile(out.path());
    };
    CHECK_EQ(runWith(true, "it2_a"), std::string(kExpectedErrorsUnderApi));
    CHECK_EQ(runWith(false, "it2_b"), std::string(kExpectedErrorsUnderApi));
}

TEST(filtering_before_or_after_a_projection_that_keeps_the_field_gives_the_same_file) {
    TempFile in("it3_in", kFixture);
    const std::vector<std::string> fields = {"timestamp", "clientIp", "status", "path"};
    auto runWith = [&](bool filterFirst, const std::string& tag) {
        TempPath out(tag);
        auto filter = std::make_shared<logflow::PredicateFilterStage>(pred::statusAtLeast(400));
        auto projection = std::make_shared<logflow::ProjectionStage>(fields);
        auto b = logflow::from(std::make_shared<logflow::FileLineSource>(in.path()))
                     .via(std::make_shared<logflow::ParserStage>());
        (filterFirst ? b.via(filter).via(projection) : b.via(projection).via(filter))
            .to(std::make_shared<logflow::FileSink>(out.path()))
            .run();
        return readFile(out.path());
    };
    CHECK_EQ(runWith(true, "it3_a"), runWith(false, "it3_b"));
}

TEST(a_pipeline_with_no_matching_record_produces_an_empty_file) {
    TempFile in("it4_in", kFixture);
    TempPath out("it4_out");
    logflow::from(std::make_shared<logflow::FileLineSource>(in.path()))
        .via(std::make_shared<logflow::ParserStage>())
        .via(std::make_shared<logflow::PredicateFilterStage>(pred::statusAtLeast(599)))
        .to(std::make_shared<logflow::FileSink>(out.path()))
        .run();
    std::ifstream f(out.path());
    CHECK(f.good());  // the file exists ...
    CHECK_EQ(readFile(out.path()), std::string(""));  // ... and is empty
}

// Independent reference: split the sample log's lines by hand instead of
// using ParserStage ("ip - - [date +0000] \"GET /path HTTP/1.1\" status ...").
TEST(sample_log_filtered_by_status_and_path_matches_a_hand_rolled_count) {
    std::size_t expected = 0;
    for (const std::string& line : readLinesViaPipeline(LOGFLOW_SAMPLE_LOG)) {
        std::istringstream words(line);
        std::vector<std::string> w;
        for (std::string t; words >> t;) w.push_back(t);
        if (std::stoi(w[8]) >= 400 && w[6].compare(0, 4, "/api") == 0) ++expected;
    }
    CHECK(expected > 0);

    TempPath out("it5_out");
    logflow::from(std::make_shared<logflow::FileLineSource>(LOGFLOW_SAMPLE_LOG))
        .via(std::make_shared<logflow::ParserStage>())
        .via(std::make_shared<logflow::PredicateFilterStage>(pred::statusAtLeast(400)))
        .via(std::make_shared<logflow::PredicateFilterStage>(pred::pathStartsWith("/api")))
        .via(std::make_shared<logflow::ProjectionStage>(std::vector<std::string>{"status", "path"}))
        .to(std::make_shared<logflow::FileSink>(out.path()))
        .run();
    const std::string text = readFile(out.path());
    CHECK_EQ(static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n')), expected);
}
