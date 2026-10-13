#pragma once

#include <cstddef>
#include <fstream>
#include <string>

#include "logflow/LogRecord.hpp"
#include "logflow/Sink.hpp"

namespace logflow {

/// Writes every record as one CSV row to a file (RFC 4180 quoting; "\n" line ends).
/// Columns: the fields a ProjectionStage kept, in its order; for an unprojected
/// record, all fields in canonical order (see Fields.hpp). No header row.
///
/// Lifecycle, which is what this class is about:
///  - the constructor only remembers the path; nothing touches the disk yet;
///  - open() creates (or truncates) the file and throws if it cannot;
///  - consume() before open() is a programming error (std::logic_error);
///  - close() flushes and closes, and THROWS if the buffered data could not be
///    written. Output is buffered by the stream, so a sink that is never
///    closed can lose its last lines without any error. The pipeline calls
///    close() even when the run failed. close() twice is harmless.
class FileSink final : public Sink<LogRecord> {
public:
    explicit FileSink(std::string path);

    void open() override;
    void consume(const LogRecord& record) override;
    void close() override;

    std::size_t writtenCount() const { return written_; }

private:
    std::string path_;
    std::ofstream out_;
    std::size_t written_ = 0;
};

}  // namespace logflow
