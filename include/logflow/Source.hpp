#pragma once

#include "logflow/Emitter.hpp"

namespace logflow {

/// The start of a pipeline: produces records and pushes them to `out`.
template <typename O>
class Source {
public:
    using output_type = O;

    virtual ~Source() = default;

    /// Emit every record this source has, then return.
    virtual void produce(Emitter<O>& out) = 0;
};

}  // namespace logflow
