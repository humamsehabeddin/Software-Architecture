#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

namespace logflow {

/// One parsed access-log entry: the domain model that flows through the
/// pipeline from Increment 2 onwards (before, it was a bare std::string).
///
/// Immutable: every member is const and set once, in the constructor. A stage
/// that wants a changed record builds a new one; nobody downstream can be
/// surprised by an upstream stage editing a record it already handed over.
///
/// `attributes` is the extension point for weeks 5-7. Later stages will attach
/// facts the parser knows nothing about (a geo-IP country, a session id, ...)
/// without the record type, the parser, or any existing stage changing.
/// `raw` keeps the original line, so nothing the parser ignores is lost.
class LogRecord {
public:
    using Timestamp = std::chrono::system_clock::time_point;
    using Attributes = std::map<std::string, std::string>;

    LogRecord(Timestamp timestamp, std::string clientIp, std::string method,
              std::string path, int status, std::int64_t bytes,
              std::string userAgent, Attributes attributes, std::string raw)
        : timestamp(timestamp),
          clientIp(std::move(clientIp)),
          method(std::move(method)),
          path(std::move(path)),
          status(status),
          bytes(bytes),
          userAgent(std::move(userAgent)),
          attributes(std::move(attributes)),
          raw(std::move(raw)) {}

    const Timestamp timestamp;
    const std::string clientIp;
    const std::string method;
    const std::string path;  // request target exactly as logged, query string included
    const int status;
    const std::int64_t bytes;
    const std::string userAgent;
    const Attributes attributes;
    const std::string raw;
};

}  // namespace logflow
