#pragma once

#include "logflow/Emitter.hpp"
#include "logflow/StageException.hpp"

namespace logflow {

/// A processing step: consumes items of type I, emits items of type O.
///
/// One input may result in zero, one, or many emit() calls.
/// Implementations may throw StageException from process().
template <typename I, typename O>
class Stage {
public:
    using input_type = I;
    using output_type = O;

    virtual ~Stage() = default;

    virtual void process(const I& input, Emitter<O>& out) = 0;

    /// Lifecycle hooks (unused this week). Called by Pipeline::run(): open()
    /// on every stage in order before data flows, close() in reverse order
    /// afterwards, even if the run failed.
    virtual void open() {}
    virtual void close() {}
};

}  // namespace logflow
