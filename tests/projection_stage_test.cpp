// ProjectionStage tests (collecting emitter, no I/O), plus how a projected
// record looks to ConsoleSink.

#include <memory>
#include <string>
#include <vector>

#include "logflow/ConsoleSink.hpp"
#include "logflow/Fields.hpp"
#include "logflow/PredicateFilterStage.hpp"
#include "logflow/Predicates.hpp"
#include "logflow/ProjectionStage.hpp"
#include "mini_test.hpp"
#include "test_support.hpp"

using namespace support;
using logflow::LogRecord;
using logflow::ProjectionStage;
using Names = std::vector<std::string>;

namespace {
LogRecord project(const Names& fields, const LogRecord& in) {
    ProjectionStage stage(fields);
    CollectingEmitter<LogRecord> out;
    stage.process(in, out);
    return out.items.at(0);
}
}  // namespace

TEST(projection_keeps_the_selected_fields_and_blanks_the_others) {
    const LogRecord in = makeRecord(404, "/x", 5, "1.2.3.4", "Safari");
    const LogRecord r = project({"clientIp", "status"}, in);
    CHECK_EQ(r.clientIp, std::string("1.2.3.4"));
    CHECK_EQ(r.status, 404);
    CHECK_EQ(r.path, std::string(""));
    CHECK_EQ(r.method, std::string(""));
    CHECK_EQ(r.userAgent, std::string(""));
    CHECK_EQ(r.bytes, std::int64_t(0));
    CHECK(r.timestamp == LogRecord::Timestamp{});
}

TEST(projection_drops_the_raw_line_and_the_old_attributes) {
    LogRecord in(makeRecord(200, "/secret?token=abc").timestamp, "1.2.3.4", "GET",
                 "/secret?token=abc", 200, 1, "ua", {{"country", "TR"}}, "the whole raw line");
    const LogRecord r = project({"status"}, in);
    CHECK_EQ(r.raw, std::string(""));            // raw would bring every field back
    CHECK_EQ(r.attributes.count("country"), std::size_t(0));
}

TEST(projection_records_which_fields_were_kept_in_the_requested_order) {
    const LogRecord r = project({"path", "timestamp", "status"}, makeRecord(200, "/a"));
    CHECK(logflow::fields::isProjected(r));
    CHECK_EQ(logflow::fields::selected(r), (Names{"path", "timestamp", "status"}));
}

TEST(an_unprojected_record_selects_every_field) {
    const LogRecord r = makeRecord(200, "/a");
    CHECK(!logflow::fields::isProjected(r));
    CHECK_EQ(logflow::fields::selected(r), logflow::fields::allNames());
}

TEST(projection_emits_exactly_one_record_per_input_record) {
    ProjectionStage stage({"status"});
    CollectingEmitter<LogRecord> out;
    stage.process(makeRecord(200, "/a"), out);
    stage.process(makeRecord(404, "/b"), out);
    CHECK_EQ(out.items.size(), std::size_t(2));
    CHECK_EQ(out.items[1].status, 404);
}

TEST(projection_rejects_bad_field_lists_at_construction) {
    CHECK_THROWS(ProjectionStage(Names{}), std::invalid_argument);
    CHECK_THROWS(ProjectionStage(Names{"status", "colour"}), std::invalid_argument);
    CHECK_THROWS(ProjectionStage(Names{"status", "status"}), std::invalid_argument);
    CHECK_THROWS(ProjectionStage(Names{"Status"}), std::invalid_argument);  // names are case-sensitive
}

TEST(console_sink_prints_a_projected_record_as_name_value_pairs) {
    const LogRecord r = project({"timestamp", "clientIp", "status"}, makeRecord(404, "/a", 0, "1.2.3.4"));
    CHECK_EQ(logflow::ConsoleSink::format(r),
             std::string("timestamp=2026-09-21T09:00:00Z clientIp=1.2.3.4 status=404"));
}

TEST(filtering_after_a_projection_that_kept_the_field_works) {
    auto sink = std::make_shared<CollectingSink<LogRecord>>();
    logflow::from(std::make_shared<VectorSource<LogRecord>>(
                      std::vector<LogRecord>{makeRecord(200, "/a"), makeRecord(500, "/b")}))
        .via(std::make_shared<ProjectionStage>(Names{"status", "path"}))
        .via(std::make_shared<logflow::PredicateFilterStage>(logflow::predicates::statusAtLeast(400)))
        .to(sink)
        .run();
    CHECK_EQ(sink->items.size(), std::size_t(1));
    CHECK_EQ(sink->items[0].path, std::string("/b"));
}

// The trade-off from the assignment, pinned down: composable does not mean
// commutative. A filter placed after a projection only sees what survived it.
TEST(filtering_after_a_projection_that_dropped_the_field_sees_blank_values) {
    auto sink = std::make_shared<CollectingSink<LogRecord>>();
    logflow::from(std::make_shared<VectorSource<LogRecord>>(
                      std::vector<LogRecord>{makeRecord(200, "/a"), makeRecord(500, "/b")}))
        .via(std::make_shared<ProjectionStage>(Names{"path"}))  // status is gone
        .via(std::make_shared<logflow::PredicateFilterStage>(logflow::predicates::statusAtLeast(400)))
        .to(sink)
        .run();
    CHECK_EQ(sink->items.size(), std::size_t(0));  // status reads as 0 for every record
}
