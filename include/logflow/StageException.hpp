#pragma once

#include <stdexcept>
#include <string>

namespace logflow {

/// Thrown by a Stage when it cannot process a record.
/// The Pipeline lets it propagate out of run() after closing all stages.
class StageException : public std::runtime_error {
public:
    explicit StageException(const std::string& message) : std::runtime_error(message) {}
    explicit StageException(const char* message) : std::runtime_error(message) {}
};

}  // namespace logflow
