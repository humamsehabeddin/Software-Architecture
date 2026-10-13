#pragma once

#include <set>
#include <string>
#include <vector>

#include "logflow/LogRecord.hpp"
#include "logflow/Stage.hpp"

namespace logflow {

/// Reduces each record to a chosen subset of its fields (data minimisation:
/// what is not selected is not passed on).
///
/// The output is still a LogRecord, so the stage composes like any other
/// Stage<LogRecord, LogRecord>. Fields that were not selected are reset to
/// empty/zero, and `raw` and the extension attributes are dropped (`raw` would
/// bring every field straight back). The fields that were kept, in the order
/// requested, are recorded under fields::kProjectionAttribute so that sinks
/// know which fields are real.
///
/// Consequence: a stage placed AFTER a projection sees blank values for the
/// fields that were dropped. Filter on a field before projecting it away.
///
/// Field names: timestamp, clientIp, method, path, status, bytes, userAgent.
class ProjectionStage final : public Stage<LogRecord, LogRecord> {
public:
    /// Throws std::invalid_argument for an empty list, an unknown or a repeated name.
    explicit ProjectionStage(std::vector<std::string> fields);

    void process(const LogRecord& record, Emitter<LogRecord>& out) override;

private:
    std::vector<std::string> fields_;
    std::set<std::string> keep_;
};

}  // namespace logflow
