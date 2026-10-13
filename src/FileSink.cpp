#include "logflow/FileSink.hpp"

#include <stdexcept>
#include <utility>

#include "logflow/Fields.hpp"

namespace logflow {
FileSink::FileSink(std::string path) : path_(std::move(path)) {}

void FileSink::open() {
    out_.open(path_, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out_) throw std::runtime_error("cannot open output file: " + path_);
    written_ = 0;
}

void FileSink::consume(const LogRecord& record) {
    if (!out_.is_open()) throw std::logic_error("FileSink::consume called before open()");
    std::string row;
    for (const std::string& name : fields::selected(record)) {
        if (!row.empty()) row += ',';
        row += fields::csvCell(fields::valueOf(record, name));
    }
    out_ << row << '\n';
    if (!out_) throw std::runtime_error("failed to write to output file: " + path_);
    ++written_;
}

void FileSink::close() {
    if (!out_.is_open()) return;
    out_.close();  // flushes the buffer; sets failbit if that failed
    if (out_.fail()) {
        out_.clear();
        throw std::runtime_error("failed to flush output file (data may be lost): " + path_);
    }
}

}  // namespace logflow
