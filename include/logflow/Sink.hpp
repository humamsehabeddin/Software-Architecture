#pragma once

namespace logflow {

/// The end of a pipeline: receives every record that survives the stages.
template <typename I>
class Sink {
public:
    using input_type = I;

    virtual ~Sink() = default;

    virtual void consume(const I& item) = 0;
};

}  // namespace logflow
