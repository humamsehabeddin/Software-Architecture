// PredicateFilterStage and the predicates:: factories. Stage tests use the
// CollectingEmitter, so no file, source or sink is involved.

#include <chrono>
#include <memory>
#include <vector>

#include "logflow/PredicateFilterStage.hpp"
#include "logflow/Predicates.hpp"
#include "mini_test.hpp"
#include "test_support.hpp"

using namespace support;
using logflow::LogRecord;
using logflow::PredicateFilterStage;
namespace pred = logflow::predicates;

namespace {
using Records = std::vector<LogRecord>;

Records run(PredicateFilterStage& stage, const Records& input) {
    CollectingEmitter<LogRecord> out;
    for (const LogRecord& r : input) stage.process(r, out);
    return out.items;
}
}  // namespace

TEST(filter_passes_records_that_satisfy_the_predicate_and_drops_the_rest) {
    PredicateFilterStage stage(pred::statusAtLeast(400));
    Records out = run(stage, {makeRecord(200, "/a"), makeRecord(404, "/b"), makeRecord(399, "/c"),
                              makeRecord(500, "/d")});
    CHECK_EQ(out.size(), std::size_t(2));
    CHECK_EQ(out[0].path, std::string("/b"));
    CHECK_EQ(out[1].path, std::string("/d"));
}

TEST(filter_passes_records_on_unchanged) {
    PredicateFilterStage stage(pred::statusAtLeast(0));
    const LogRecord in = makeRecord(404, "/x?q=1", 7, "9.9.9.9", "UA");
    Records out = run(stage, {in});
    CHECK_EQ(out.size(), std::size_t(1));
    CHECK_EQ(out[0].raw, in.raw);
    CHECK_EQ(out[0].clientIp, in.clientIp);
    CHECK_EQ(out[0].userAgent, in.userAgent);
    CHECK(out[0].timestamp == in.timestamp);
}

TEST(filter_counts_what_it_saw_and_what_it_let_through) {
    PredicateFilterStage stage(pred::statusAtLeast(400));
    run(stage, {makeRecord(200, "/a"), makeRecord(500, "/b"), makeRecord(301, "/c")});
    CHECK_EQ(stage.seenCount(), std::size_t(3));
    CHECK_EQ(stage.passedCount(), std::size_t(1));
    CHECK_EQ(stage.rejectedCount(), std::size_t(2));
}

TEST(filter_rejects_an_empty_predicate_at_construction) {
    CHECK_THROWS(PredicateFilterStage(logflow::Predicate<LogRecord>()), std::invalid_argument);
}

TEST(filter_accepts_any_callable_such_as_a_lambda) {
    PredicateFilterStage stage([](const LogRecord& r) { return r.method == "GET" && r.bytes == 100; });
    CHECK_EQ(run(stage, {makeRecord(200, "/a")}).size(), std::size_t(1));
}

TEST(status_predicate_is_inclusive_at_the_boundary) {
    auto p = pred::statusAtLeast(400);
    CHECK(!p(makeRecord(399, "/")));
    CHECK(p(makeRecord(400, "/")));
    CHECK(p(makeRecord(599, "/")));
}

TEST(path_predicate_is_a_plain_prefix_test_on_the_logged_target) {
    auto p = pred::pathStartsWith("/api");
    CHECK(p(makeRecord(200, "/api")));
    CHECK(p(makeRecord(200, "/api/orders")));
    CHECK(p(makeRecord(200, "/api?x=1")));  // query string is part of the path
    CHECK(p(makeRecord(200, "/apiary")));   // documented: it is a string prefix
    CHECK(!p(makeRecord(200, "/products/api")));
    CHECK(!p(makeRecord(200, "/ap")));
    CHECK(!p(makeRecord(200, "")));
}

TEST(timestamp_predicate_is_half_open_from_inclusive_to_exclusive) {
    const LogRecord base = makeRecord(200, "/", 0);
    const auto t = [&](int s) { return base.timestamp + std::chrono::seconds(s); };
    auto p = pred::timestampBetween(t(10), t(20));
    CHECK(!p(makeRecord(200, "/", 9)));
    CHECK(p(makeRecord(200, "/", 10)));
    CHECK(p(makeRecord(200, "/", 19)));
    CHECK(!p(makeRecord(200, "/", 20)));
}

TEST(filters_compose_in_a_pipeline_and_the_order_does_not_change_the_result) {
    const Records input = {makeRecord(500, "/api/a"), makeRecord(200, "/api/b"),
                           makeRecord(404, "/web/c"), makeRecord(404, "/api/d"),
                           makeRecord(200, "/web/e")};
    auto runOrder = [&](bool statusFirst) {
        auto status = std::make_shared<PredicateFilterStage>(pred::statusAtLeast(400));
        auto path = std::make_shared<PredicateFilterStage>(pred::pathStartsWith("/api"));
        auto sink = std::make_shared<CollectingSink<LogRecord>>();
        auto b = logflow::from(std::make_shared<VectorSource<LogRecord>>(input));
        auto p = statusFirst ? b.via(status).via(path).to(sink) : b.via(path).via(status).to(sink);
        p.run();
        std::vector<std::string> paths;
        for (const LogRecord& r : sink->items) paths.push_back(r.path);
        return paths;
    };
    CHECK_EQ(runOrder(true), (std::vector<std::string>{"/api/a", "/api/d"}));
    CHECK_EQ(runOrder(false), runOrder(true));
}
