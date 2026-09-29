#pragma once

#include <iosfwd>
#include <string>

#include "logflow/Sink.hpp"

namespace logflow {

/// Prints every record on its own line. Writes to std::cout by default; the
/// stream can be injected, which is what makes the sink testable.
class ConsoleSink final : public Sink<std::string> {
public:
    explicit ConsoleSink(std::ostream& out);
    ConsoleSink();  // std::cout

    void consume(const std::string& item) override;

private:
    std::ostream& out_;
};

}  // namespace logflow
