#pragma once

#include <cstddef>

#include "logflow/LogRecord.hpp"
#include "logflow/Predicates.hpp"
#include "logflow/Stage.hpp"

namespace logflow {

/// Passes a record on, unchanged, only if the predicate says yes; otherwise
/// emits nothing. It is a plain Stage<LogRecord, LogRecord>, so it can be put
/// anywhere after the parser, any number of times. See predicates:: for the
/// usual conditions.
///
/// Counts what it saw and what it let through; the order experiment uses that.
class PredicateFilterStage final : public Stage<LogRecord, LogRecord> {
public:
    /// Throws std::invalid_argument if the predicate is empty.
    explicit PredicateFilterStage(Predicate<LogRecord> keep);

    void process(const LogRecord& record, Emitter<LogRecord>& out) override;

    std::size_t seenCount() const { return seen_; }
    std::size_t passedCount() const { return passed_; }
    std::size_t rejectedCount() const { return seen_ - passed_; }

private:
    Predicate<LogRecord> keep_;
    std::size_t seen_ = 0;
    std::size_t passed_ = 0;
};

}  // namespace logflow
