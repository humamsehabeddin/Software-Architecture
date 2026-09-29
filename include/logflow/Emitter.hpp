#pragma once

namespace logflow {

/// Where a component pushes its output.
///
/// A Stage does not *return* what it produces; it *emits* it. That single
/// decision lets one input turn into zero, one, or many outputs (a filter, a
/// map, a splitter) without changing the interface. See ARCHITECTURE.md.
template <typename T>
class Emitter {
public:
    using value_type = T;

    virtual ~Emitter() = default;

    /// Hand one item to whatever is downstream.
    virtual void emit(T item) = 0;
};

}  // namespace logflow
