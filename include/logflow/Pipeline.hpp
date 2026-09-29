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

/// Type-erased handle to a stage's lifecycle, in pipeline order.
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
/// from(source).then(stage)...to(sink); then call run().
class Pipeline {
public:
    /// Push every record from the source, through the stages in order, into the sink.
    ///
    /// Lifecycle: open() on each stage in order, then data flows, then close()
    /// on each stage in reverse order. close() runs even if a stage, the
    /// source, or the sink throws; the original exception is rethrown.
    void run() const;

    std::size_t stageCount() const { return stages_.size(); }

private:
    template <typename T>
    friend class PipelineBuilder;

    Pipeline(std::function<void()> body, std::vector<detail::StageHooks> stages);

    static void closeOpened(const std::vector<detail::StageHooks>& stages,
                            std::size_t opened, bool rethrow);

    std::function<void()> body_;
    std::vector<detail::StageHooks> stages_;
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
    template <typename S>
    auto then(std::shared_ptr<S> stage) const -> PipelineBuilder<typename S::output_type> {
        using Next = typename S::output_type;
        static_assert(std::is_base_of<Stage<T, Next>, S>::value,
                      "Pipeline::then: the stage's input type does not match the "
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

    /// Finish the chain with a sink.
    Pipeline to(std::shared_ptr<Sink<T>> sink) const {
        if (!sink) throw std::invalid_argument("Pipeline: sink must not be null");
        Upstream upstream = upstream_;
        return Pipeline(
            [upstream, sink] {
                detail::SinkEmitter<T> adapter(*sink);
                upstream(adapter);
            },
            stages_);
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

/// Begin a pipeline:  from(source).then(stage).then(stage).to(sink).run();
template <typename S>
auto from(std::shared_ptr<S> source) -> PipelineBuilder<typename S::output_type> {
    using O = typename S::output_type;
    static_assert(std::is_base_of<Source<O>, S>::value, "from(): argument must be a Source");
    return PipelineBuilder<O>::startingAt(std::move(source));
}

}  // namespace logflow
