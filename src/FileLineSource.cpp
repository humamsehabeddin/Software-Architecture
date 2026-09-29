#include "logflow/FileLineSource.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

namespace logflow {

FileLineSource::FileLineSource(std::string path) : path_(std::move(path)) {}

void FileLineSource::produce(Emitter<std::string>& out) {
    std::ifstream in(path_, std::ios::in | std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open input file: " + path_);
    }

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();  // tolerate Windows line endings
        }
        out.emit(std::move(line));
    }

    // getline stops on EOF (fine) or on a real I/O error (not fine).
    if (in.bad()) {
        throw std::runtime_error("error while reading input file: " + path_);
    }
}

}  // namespace logflow
