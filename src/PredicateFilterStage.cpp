#include "logflow/PredicateFilterStage.hpp"

#include <stdexcept>
#include <utility>

namespace logflow {

PredicateFilterStage::PredicateFilterStage(Predicate<LogRecord> keep) : keep_(std::move(keep)) {
    if (!keep_) throw std::invalid_argument("PredicateFilterStage: predicate must not be empty");
}

void PredicateFilterStage::process(const LogRecord& record, Emitter<LogRecord>& out) {
    ++seen_;
    if (!keep_(record)) return;
    ++passed_;
    out.emit(record);
}

}  // namespace logflow
