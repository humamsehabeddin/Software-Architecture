#include "logflow/ConsoleSink.hpp"

#include <iostream>
#include <stdexcept>

namespace logflow {

ConsoleSink::ConsoleSink(std::ostream& out) : out_(out) {}

ConsoleSink::ConsoleSink() : out_(std::cout) {}

void ConsoleSink::consume(const std::string& item) {
    out_ << item << '\n';
    if (!out_) {
        throw std::runtime_error("failed to write to output stream");
    }
}

}  // namespace logflow
