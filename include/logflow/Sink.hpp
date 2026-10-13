#pragma once

namespace logflow {

/// The end of a pipeline: receives every record that survives the stages.
///
/// Like a Stage, a Sink has lifecycle hooks (default: do nothing). A sink that
/// owns a resource (an output file) acquires it in open() and releases it in
/// close(). Pipeline::run() calls open() after the stages have been opened and
/// close() before they are closed, even if the run failed.
template <typename I>
class Sink {
public:
    using input_type = I;

    virtual ~Sink() = default;

    virtual void consume(const I& item) = 0;

    virtual void open() {}
    virtual void close() {}
};

}  // namespace logflow
