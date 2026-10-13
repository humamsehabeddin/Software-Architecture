#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "logflow/Emitter.hpp"
#include "logflow/Sink.hpp"
#include "logflow/Source.hpp"
#include "logflow/Stage.hpp"

// Pipeline depends ONLY on the interfaces above. It has never heard of
// FileLineSource or ConsoleSink; that is what keeps the architecture open
// for extension.

namespace logflow {

template <typename T>
class PipelineBuilder;

namespace detail {

/// Type-erased handle to a component's lifecycle (a stage or the sink).
struct StageHooks {
    std::function<void()> open;
    std::function<void()> close;
};

/// Emitter that forwards into a Sink.
template <typename T>
class SinkEmitter final : public Emitter<T> {
public:
    explicit SinkEmitter(Sink<T>& sink) : sink_(sink) {}
    void emit(T item) override { sink_.consume(item); }

private:
    Sink<T>& sink_;
};

/// Emitter that forwards into a Stage, which emits onward to `downstream`.
template <typename I, typename O>
class StageEmitter final : public Emitter<I> {
public:
    StageEmitter(Stage<I, O>& stage, Emitter<O>& downstream)
        : stage_(stage), downstream_(downstream) {}
    void emit(I item) override { stage_.process(item, downstream_); }

private:
    Stage<I, O>& stage_;
    Emitter<O>& downstream_;
};

}  // namespace detail

/// A fully assembled source -> stage* -> sink chain. Obtain one with
/// from(source).via(stage)...to(sink); then call run().
class Pipeline {
public:
    /// Push every record from the source, through the stages in order, into the sink.
    ///
    /// Lifecycle: open() on each stage in order, then on the sink; data flows;
    /// then close() in reverse order (sink first, then the stages last to
    /// first). close() runs even if a stage, the source, or the sink throws;
    /// the original exception is rethrown.
    void run() const;

    /// Number of stages between source and sink (the sink is not counted).
    std::size_t stageCount() const { return stageCount_; }

private:
    template <typename T>
    friend class PipelineBuilder;

    Pipeline(std::function<void()> body, std::vector<detail::StageHooks> lifecycle,
             std::size_t stageCount);

    static void closeOpened(const std::vector<detail::StageHooks>& lifecycle,
                            std::size_t opened, bool rethrow);

    std::function<void()> body_;
    std::vector<detail::StageHooks> lifecycle_;  // stages in order, then the sink
    std::size_t stageCount_;
};

/// Type-safe assembly of a pipeline. T is the type of record that flows out
/// of the chain built so far, so the compiler rejects stages that do not fit.
template <typename T>
class PipelineBuilder {
public:
    static PipelineBuilder startingAt(std::shared_ptr<Source<T>> source) {
        if (!source) throw std::invalid_argument("Pipeline: source must not be null");
        return PipelineBuilder(
            [source](Emitter<T>& out) { source->produce(out); }, {});
    }

    /// Append a stage. Its input type must equal T; the result carries its output type.
    /// A stage that keeps the type (a filter, a projection) gives back the same
    /// PipelineBuilder<T>, so such stages can be added in any order, any number
    /// of times, even conditionally in a loop.
    template <typename S>
    auto via(std::shared_ptr<S> stage) const -> PipelineBuilder<typename S::output_type> {
        using Next = typename S::output_type;
        static_assert(std::is_base_of<Stage<T, Next>, S>::value,
                      "Pipeline::via: the stage's input type does not match the "
                      "output type of the previous step");
        if (!stage) throw std::invalid_argument("Pipeline: stage must not be null");

        auto stages = stages_;
        stages.push_back({[stage] { stage->open(); }, [stage] { stage->close(); }});

        Upstream upstream = upstream_;
        return PipelineBuilder<Next>(
            [upstream, stage](Emitter<Next>& out) {
                detail::StageEmitter<T, Next> adapter(*stage, out);
                upstream(adapter);
            },
            std::move(stages));
    }

    /// Finish the chain with a sink. The sink's open()/close() join the
    /// lifecycle after the last stage.
    Pipeline to(std::shared_ptr<Sink<T>> sink) const {
        if (!sink) throw std::invalid_argument("Pipeline: sink must not be null");
        Upstream upstream = upstream_;
        auto lifecycle = stages_;
        lifecycle.push_back({[sink] { sink->open(); }, [sink] { sink->close(); }});
        return Pipeline(
            [upstream, sink] {
                detail::SinkEmitter<T> adapter(*sink);
                upstream(adapter);
            },
            std::move(lifecycle), stages_.size());
    }

private:
    template <typename U>
    friend class PipelineBuilder;

    using Upstream = std::function<void(Emitter<T>&)>;

    PipelineBuilder(Upstream upstream, std::vector<detail::StageHooks> stages)
        : upstream_(std::move(upstream)), stages_(std::move(stages)) {}

    Upstream upstream_;
    std::vector<detail::StageHooks> stages_;
};

/// Begin a pipeline:  from(source).via(stage).via(stage).to(sink).run();
template <typename S>
auto from(std::shared_ptr<S> source) -> PipelineBuilder<typename S::output_type> {
    using O = typename S::output_type;
    static_assert(std::is_base_of<Source<O>, S>::value, "from(): argument must be a Source");
    return PipelineBuilder<O>::startingAt(std::move(source));
}

}  // namespace logflow
