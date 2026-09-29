#pragma once

#include <string>

#include "logflow/Source.hpp"

namespace logflow {

/// Reads a text file and emits one std::string per line.
///
/// - The line terminator is not part of the record ("\n" and "\r\n" both stripped).
/// - A final line without a terminator is still emitted.
/// - Blank lines are emitted as empty strings; an empty file emits nothing.
/// - produce() throws std::runtime_error if the file cannot be opened or read.
class FileLineSource final : public Source<std::string> {
public:
    explicit FileLineSource(std::string path);

    void produce(Emitter<std::string>& out) override;

private:
    std::string path_;
};

}  // namespace logflow
