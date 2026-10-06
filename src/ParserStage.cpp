#include "logflow/ParserStage.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "logflow/TimeUtil.hpp"

namespace logflow {
namespace {

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool isDigit(char c) { return c >= '0' && c <= '9'; }

/// Walks a line left to right, one field at a time.
class Scanner {
public:
    explicit Scanner(const std::string& s) : s_(s) {}

    void skipSpaces() {
        while (pos_ < s_.size() && isSpace(s_[pos_])) ++pos_;
    }
    bool atEnd() {
        skipSpaces();
        return pos_ >= s_.size();
    }

    /// Next run of non-space characters.
    bool word(std::string& out) {
        skipSpaces();
        const std::size_t start = pos_;
        while (pos_ < s_.size() && !isSpace(s_[pos_])) ++pos_;
        out.assign(s_, start, pos_ - start);
        return !out.empty();
    }

    /// "[ ... ]" -> contents.
    bool bracketed(std::string& out) {
        skipSpaces();
        if (pos_ >= s_.size() || s_[pos_] != '[') return false;
        const std::size_t close = s_.find(']', pos_ + 1);
        if (close == std::string::npos) return false;
        out.assign(s_, pos_ + 1, close - pos_ - 1);
        pos_ = close + 1;
        return true;
    }

    /// '"..."' -> contents. Spaces are kept; \" and \\ are unescaped.
    bool quoted(std::string& out) {
        skipSpaces();
        if (pos_ >= s_.size() || s_[pos_] != '"') return false;
        out.clear();
        for (++pos_; pos_ < s_.size(); ++pos_) {
            const char c = s_[pos_];
            if (c == '\\' && pos_ + 1 < s_.size() &&
                (s_[pos_ + 1] == '"' || s_[pos_ + 1] == '\\')) {
                out += s_[++pos_];
            } else if (c == '"') {
                ++pos_;
                return true;
            } else {
                out += c;
            }
        }
        return false;  // no closing quote
    }

private:
    const std::string& s_;
    std::size_t pos_ = 0;
};

bool allDigits(const std::string& s, std::size_t from, std::size_t to) {
    if (from >= to) return false;
    for (std::size_t i = from; i < to; ++i) {
        if (!isDigit(s[i])) return false;
    }
    return true;
}

int toInt(const std::string& s, std::size_t from, std::size_t to) {
    int v = 0;
    for (std::size_t i = from; i < to; ++i) v = v * 10 + (s[i] - '0');
    return v;
}

/// "21/Sep/2026:09:00:00 +0000" -> UTC time point.
bool parseTimestamp(const std::string& t, LogRecord::Timestamp& out) {
    // dd/Mon/yyyy:HH:MM:SS +zzzz  (26 characters)
    if (t.size() != 26) return false;
    if (t[2] != '/' || t[6] != '/' || t[11] != ':' || t[14] != ':' || t[17] != ':' ||
        t[20] != ' ')
        return false;
    if (!allDigits(t, 0, 2) || !allDigits(t, 7, 11) || !allDigits(t, 12, 14) ||
        !allDigits(t, 15, 17) || !allDigits(t, 18, 20) || !allDigits(t, 22, 26))
        return false;
    if (t[21] != '+' && t[21] != '-') return false;

    static const char* const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    unsigned month = 0;
    for (unsigned i = 0; i < 12; ++i) {
        if (t.compare(3, 3, months[i]) == 0) month = i + 1;
    }
    if (month == 0) return false;

    const int day = toInt(t, 0, 2);
    const int year = toInt(t, 7, 11);
    const int hour = toInt(t, 12, 14);
    const int minute = toInt(t, 15, 17);
    const int second = toInt(t, 18, 20);
    const int zoneHours = toInt(t, 22, 24);
    const int zoneMinutes = toInt(t, 24, 26);

    if (day < 1 || day > static_cast<int>(timeutil::daysInMonth(year, month))) return false;
    if (hour > 23 || minute > 59 || second > 59) return false;
    if (zoneHours > 23 || zoneMinutes > 59) return false;

    const std::int64_t days = timeutil::daysFromCivil(year, month, static_cast<unsigned>(day));
    std::int64_t seconds = days * 86400 + hour * 3600 + minute * 60 + second;
    const std::int64_t offset = zoneHours * 3600 + zoneMinutes * 60;
    seconds -= (t[21] == '+') ? offset : -offset;  // local time -> UTC

    out = LogRecord::Timestamp(std::chrono::seconds(seconds));
    return true;
}

/// Parses one line; nullopt means "malformed".
std::optional<LogRecord> parse(const std::string& line) {
    Scanner in(line);
    std::string ip, ident, user, when, request, statusText, bytesText;

    if (!in.word(ip) || !in.word(ident) || !in.word(user)) return std::nullopt;

    LogRecord::Timestamp timestamp;
    if (!in.bracketed(when) || !parseTimestamp(when, timestamp)) return std::nullopt;

    if (!in.quoted(request)) return std::nullopt;
    Scanner req(request);
    std::string method, path, protocol;
    if (!req.word(method) || !req.word(path) || !req.word(protocol) || !req.atEnd())
        return std::nullopt;

    if (!in.word(statusText) || statusText.size() != 3 ||
        !allDigits(statusText, 0, 3))
        return std::nullopt;
    const int status = toInt(statusText, 0, 3);
    if (status < 100 || status > 599) return std::nullopt;

    if (!in.word(bytesText)) return std::nullopt;
    std::int64_t bytes = 0;
    if (bytesText != "-") {
        if (bytesText.size() > 15 || !allDigits(bytesText, 0, bytesText.size()))
            return std::nullopt;
        for (char c : bytesText) bytes = bytes * 10 + (c - '0');
    }

    std::string referer, userAgent;
    if (!in.atEnd()) {  // combined format: "referer" "user-agent"
        if (!in.quoted(referer) || !in.quoted(userAgent) || !in.atEnd()) return std::nullopt;
    }

    return LogRecord(timestamp, std::move(ip), std::move(method), std::move(path), status,
                     bytes, std::move(userAgent), LogRecord::Attributes{}, line);
}

}  // namespace

void ParserStage::process(const std::string& line, Emitter<LogRecord>& out) {
    std::optional<LogRecord> record = parse(line);
    if (!record) {
        ++malformed_;  // Increment 5 will decide what to do with these.
        return;
    }
    ++parsed_;
    out.emit(std::move(*record));
}

}  // namespace logflow
