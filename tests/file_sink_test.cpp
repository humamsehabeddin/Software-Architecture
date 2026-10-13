// FileSink tests. The point of this file is the lifecycle: nothing is written
// to disk until the sink is closed, and the pipeline is what closes it.

#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "logflow/FileSink.hpp"
#include "logflow/PredicateFilterStage.hpp"
#include "logflow/Predicates.hpp"
#include "logflow/ProjectionStage.hpp"
#include "mini_test.hpp"
#include "test_support.hpp"

using namespace support;
using logflow::FileSink;
using logflow::LogRecord;

TEST(file_sink_writes_all_fields_as_a_csv_row_for_an_unprojected_record) {
    TempPath out("fs_all");
    FileSink sink(out.path());
    sink.open();
    sink.consume(makeRecord(404, "/a", 3, "1.2.3.4", "UA"));
    sink.close();
    CHECK_EQ(readFile(out.path()), std::string("2026-09-21T09:00:03Z,1.2.3.4,GET,/a,404,100,UA\n"));
}

TEST(file_sink_writes_only_the_projected_columns_in_the_projected_order) {
    TempPath out("fs_proj");
    auto sink = std::make_shared<FileSink>(out.path());
    logflow::from(std::make_shared<VectorSource<LogRecord>>(
                      std::vector<LogRecord>{makeRecord(404, "/a", 0, "1.2.3.4"),
                                             makeRecord(500, "/b", 1, "5.6.7.8")}))
        .via(std::make_shared<logflow::ProjectionStage>(std::vector<std::string>{"status", "clientIp"}))
        .to(sink)
        .run();
    CHECK_EQ(readFile(out.path()), std::string("404,1.2.3.4\n500,5.6.7.8\n"));
    CHECK_EQ(sink->writtenCount(), std::size_t(2));
}

TEST(file_sink_quotes_cells_that_contain_commas_quotes_or_line_breaks) {
    TempPath out("fs_csv");
    FileSink sink(out.path());
    sink.open();
    sink.consume(makeRecord(200, "/a,b", 0, "1.1.1.1", "say \"hi\""));
    sink.close();
    CHECK_EQ(readFile(out.path()),
             std::string("2026-09-21T09:00:00Z,1.1.1.1,GET,\"/a,b\",200,100,\"say \"\"hi\"\"\"\n"));
}

TEST(file_sink_does_not_touch_the_disk_before_open) {
    TempPath out("fs_lazy");
    FileSink sink(out.path());
    CHECK_EQ(readFile(out.path()), std::string(""));  // the file does not exist yet
    std::ifstream probe(out.path());
    CHECK(!probe.good());
}

TEST(file_sink_refuses_records_before_open) {
    TempPath out("fs_early");
    FileSink sink(out.path());
    CHECK_THROWS(sink.consume(makeRecord(200, "/a")), std::logic_error);
}

TEST(file_sink_reports_an_output_file_it_cannot_create) {
    FileSink sink("no/such/directory/out.csv");
    CHECK_THROWS(sink.open(), std::runtime_error);
}

TEST(file_sink_close_is_idempotent_and_safe_without_open) {
    TempPath out("fs_close");
    FileSink never(out.path());
    never.close();  // never opened: nothing to do, must not throw
    FileSink sink(out.path());
    sink.open();
    sink.close();
    sink.close();
}

TEST(file_sink_open_truncates_an_existing_file) {
    TempFile old("fs_trunc", "old content that must disappear\n");
    FileSink sink(old.path());
    sink.open();
    sink.consume(makeRecord(200, "/new", 0, "1.1.1.1", "u"));
    sink.close();
    CHECK_EQ(readFile(old.path()), std::string("2026-09-21T09:00:00Z,1.1.1.1,GET,/new,200,100,u\n"));
}

TEST(everything_is_on_disk_once_pipeline_run_returns_because_the_pipeline_closes_the_sink) {
    TempPath out("fs_pipeline");
    std::vector<LogRecord> input;
    for (int i = 0; i < 5000; ++i) input.push_back(makeRecord(200, "/p" + std::to_string(i), i));
    logflow::from(std::make_shared<VectorSource<LogRecord>>(input))
        .to(std::make_shared<FileSink>(out.path()))
        .run();
    const std::string text = readFile(out.path());
    CHECK_EQ(std::count(text.begin(), text.end(), '\n'), std::ptrdiff_t(5000));
    CHECK(text.find("/p4999,") != std::string::npos);  // the very last row made it
}

TEST(the_output_file_is_closed_even_when_a_stage_fails_midway) {
    TempPath out("fs_fail");
    auto failing = std::make_shared<logflow::PredicateFilterStage>(
        [](const LogRecord& r) -> bool {
            if (r.path == "/boom") throw logflow::StageException("boom");
            return true;
        });
    auto p = logflow::from(std::make_shared<VectorSource<LogRecord>>(
                               std::vector<LogRecord>{makeRecord(200, "/ok", 0, "1.1.1.1", "u"),
                                                      makeRecord(200, "/boom")}))
                 .via(failing)
                 .to(std::make_shared<FileSink>(out.path()));
    CHECK_THROWS(p.run(), logflow::StageException);
    // The record that got through before the failure was flushed by close().
    CHECK_EQ(readFile(out.path()), std::string("2026-09-21T09:00:00Z,1.1.1.1,GET,/ok,200,100,u\n"));
}
