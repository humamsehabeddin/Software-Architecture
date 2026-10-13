#include <memory>
#include <string>
#include <vector>

#include "logflow/Pipeline.hpp"
#include "mini_test.hpp"
#include "test_support.hpp"

using namespace support;
using Strings = std::vector<std::string>;

TEST(pipeline_without_stages_passes_records_source_to_sink_in_order) {
    auto sink = std::make_shared<CollectingSink<std::string>>();
    auto p = logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"a", "b", "c"}))
                 .to(sink);
    p.run();
    CHECK_EQ(sink->items, (Strings{"a", "b", "c"}));
    CHECK_EQ(p.stageCount(), std::size_t(0));
}

TEST(pipeline_applies_stages_in_the_order_they_were_added) {
    auto sink = std::make_shared<CollectingSink<std::string>>();
    logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"x"}))
        .via(std::make_shared<PrependStage>("a:"))
        .via(std::make_shared<PrependStage>("b:"))
        .to(sink)
        .run();
    CHECK_EQ(sink->items, (Strings{"b:a:x"}));
}

TEST(pipeline_supports_stages_that_change_the_record_type) {
    auto sink = std::make_shared<CollectingSink<std::size_t>>();
    auto p = logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"a", "bcd", ""}))
                 .via(std::make_shared<LengthStage>())
                 .to(sink);
    p.run();
    CHECK_EQ(sink->items, (std::vector<std::size_t>{1, 3, 0}));
    CHECK_EQ(p.stageCount(), std::size_t(1));
}

TEST(a_stage_may_emit_zero_or_one_output_per_input) {
    auto sink = std::make_shared<CollectingSink<std::string>>();
    logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"a", "", "b", ""}))
        .via(std::make_shared<DropEmptyStage>())
        .to(sink)
        .run();
    CHECK_EQ(sink->items, (Strings{"a", "b"}));
}

TEST(a_stage_may_emit_many_outputs_per_input) {
    auto sink = std::make_shared<CollectingSink<std::string>>();
    logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"a b", "", "c d e"}))
        .via(std::make_shared<SplitWordsStage>())
        .to(sink)
        .run();
    CHECK_EQ(sink->items, (Strings{"a", "b", "c", "d", "e"}));
}

TEST(records_are_streamed_one_at_a_time_not_buffered_between_steps) {
    Strings events;
    logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"1", "2"}))
        .via(std::make_shared<RecordingStage>("A", events))
        .to(std::make_shared<LoggingSink>(events))
        .run();
    CHECK_EQ(events, (Strings{"open:A", "process:A:1", "consume:1",
                              "process:A:2", "consume:2", "close:A"}));
}

TEST(stages_are_opened_in_order_and_closed_in_reverse_order) {
    Strings events;
    logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"x"}))
        .via(std::make_shared<RecordingStage>("A", events))
        .via(std::make_shared<RecordingStage>("B", events))
        .to(std::make_shared<LoggingSink>(events))
        .run();
    CHECK_EQ(events, (Strings{"open:A", "open:B", "process:A:x", "process:B:x",
                              "consume:x", "close:B", "close:A"}));
}

TEST(stages_are_closed_and_the_error_propagates_when_a_stage_fails) {
    Strings events;
    auto p = logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"x"}))
                 .via(std::make_shared<RecordingStage>("A", events))
                 .via(std::make_shared<RecordingStage>("B", events, false, true))
                 .to(std::make_shared<LoggingSink>(events));
    CHECK_THROWS(p.run(), logflow::StageException);
    CHECK_EQ(events, (Strings{"open:A", "open:B", "process:A:x", "process:B:x",
                              "close:B", "close:A"}));
}

TEST(only_stages_that_were_opened_are_closed_when_open_fails) {
    Strings events;
    auto p = logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"x"}))
                 .via(std::make_shared<RecordingStage>("A", events))
                 .via(std::make_shared<RecordingStage>("B", events, true, false))
                 .via(std::make_shared<RecordingStage>("C", events))
                 .to(std::make_shared<LoggingSink>(events));
    CHECK_THROWS(p.run(), logflow::StageException);
    CHECK_EQ(events, (Strings{"open:A", "close:A"}));
}

TEST(stages_are_closed_when_the_source_fails) {
    Strings events;
    auto p = logflow::from(std::make_shared<logflow::FileLineSource>("no/such/file.log"))
                 .via(std::make_shared<RecordingStage>("A", events))
                 .to(std::make_shared<LoggingSink>(events));
    CHECK_THROWS(p.run(), std::runtime_error);
    CHECK_EQ(events, (Strings{"open:A", "close:A"}));
}

TEST(a_pipeline_can_be_run_more_than_once) {
    auto sink = std::make_shared<CollectingSink<std::string>>();
    auto p = logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"a"})).to(sink);
    p.run();
    p.run();
    CHECK_EQ(sink->items, (Strings{"a", "a"}));
}

TEST(null_components_are_rejected_at_assembly_time) {
    std::shared_ptr<VectorSource<std::string>> nullSource;
    CHECK_THROWS(logflow::from(nullSource), std::invalid_argument);

    auto builder = logflow::from(std::make_shared<VectorSource<std::string>>(Strings{}));
    std::shared_ptr<PrependStage> nullStage;
    CHECK_THROWS(builder.via(nullStage), std::invalid_argument);
    std::shared_ptr<logflow::Sink<std::string>> nullSink;
    CHECK_THROWS(builder.to(nullSink), std::invalid_argument);
}

// ---------------- the sink is part of the lifecycle ----------------

TEST(the_sink_is_opened_after_the_stages_and_closed_before_them) {
    Strings events;
    logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"x"}))
        .via(std::make_shared<RecordingStage>("A", events))
        .via(std::make_shared<RecordingStage>("B", events))
        .to(std::make_shared<LifecycleSink>(events))
        .run();
    CHECK_EQ(events, (Strings{"open:A", "open:B", "open:sink", "process:A:x", "process:B:x",
                              "consume:x", "close:sink", "close:B", "close:A"}));
}

TEST(a_sink_without_hooks_still_works_and_stage_count_ignores_the_sink) {
    auto sink = std::make_shared<CollectingSink<std::string>>();  // default open()/close()
    auto p = logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"a"}))
                 .via(std::make_shared<PrependStage>("p:"))
                 .to(sink);
    p.run();
    CHECK_EQ(sink->items, (Strings{"p:a"}));
    CHECK_EQ(p.stageCount(), std::size_t(1));
}

TEST(the_sink_is_closed_when_a_stage_fails) {
    Strings events;
    auto p = logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"x"}))
                 .via(std::make_shared<RecordingStage>("A", events, false, true))
                 .to(std::make_shared<LifecycleSink>(events));
    CHECK_THROWS(p.run(), logflow::StageException);
    CHECK_EQ(events, (Strings{"open:A", "open:sink", "process:A:x", "close:sink", "close:A"}));
}

TEST(stages_are_closed_and_nothing_flows_when_the_sink_cannot_open) {
    Strings events;
    auto p = logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"x"}))
                 .via(std::make_shared<RecordingStage>("A", events))
                 .to(std::make_shared<LifecycleSink>(events, /*failOnOpen=*/true));
    CHECK_THROWS(p.run(), std::runtime_error);
    CHECK_EQ(events, (Strings{"open:A", "close:A"}));
}

TEST(a_failing_sink_close_is_reported_after_all_stages_were_closed) {
    Strings events;
    auto p = logflow::from(std::make_shared<VectorSource<std::string>>(Strings{"x"}))
                 .via(std::make_shared<RecordingStage>("A", events))
                 .to(std::make_shared<LifecycleSink>(events, false, /*failOnClose=*/true));
    CHECK_THROWS(p.run(), std::runtime_error);
    CHECK_EQ(events, (Strings{"open:A", "open:sink", "process:A:x", "consume:x", "close:sink",
                              "close:A"}));
}
