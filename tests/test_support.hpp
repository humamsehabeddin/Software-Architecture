#pragma once
// Test doubles: small implementations of the interfaces, used only by tests.

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "logflow/FileLineSource.hpp"
#include "logflow/Pipeline.hpp"

namespace support {

template <typename T>
class VectorSource final : public logflow::Source<T> {
public:
    explicit VectorSource(std::vector<T> items) : items_(std::move(items)) {}
    void produce(logflow::Emitter<T>& out) override {
        for (const auto& i : items_) out.emit(i);
    }

private:
    std::vector<T> items_;
};

template <typename T>
class CollectingSink final : public logflow::Sink<T> {
public:
    void consume(const T& item) override { items.push_back(item); }
    std::vector<T> items;
};

/// Logs "consume:<item>" into a shared event list.
class LoggingSink final : public logflow::Sink<std::string> {
public:
    explicit LoggingSink(std::vector<std::string>& events) : events_(events) {}
    void consume(const std::string& item) override { events_.push_back("consume:" + item); }

private:
    std::vector<std::string>& events_;
};

using StringStage = logflow::Stage<std::string, std::string>;

/// Prepends a prefix (1 in -> 1 out).
class PrependStage final : public StringStage {
public:
    explicit PrependStage(std::string prefix) : prefix_(std::move(prefix)) {}
    void process(const std::string& in, logflow::Emitter<std::string>& out) override {
        out.emit(prefix_ + in);
    }

private:
    std::string prefix_;
};

/// Changes the record type: string -> length.
class LengthStage final : public logflow::Stage<std::string, std::size_t> {
public:
    void process(const std::string& in, logflow::Emitter<std::size_t>& out) override {
        out.emit(in.size());
    }
};

/// Drops empty strings (1 in -> 0 or 1 out).
class DropEmptyStage final : public StringStage {
public:
    void process(const std::string& in, logflow::Emitter<std::string>& out) override {
        if (!in.empty()) out.emit(in);
    }
};

/// Splits on spaces (1 in -> 0..n out).
class SplitWordsStage final : public StringStage {
public:
    void process(const std::string& in, logflow::Emitter<std::string>& out) override {
        std::string word;
        for (char c : in) {
            if (c == ' ') {
                if (!word.empty()) out.emit(word);
                word.clear();
            } else {
                word += c;
            }
        }
        if (!word.empty()) out.emit(word);
    }
};

/// Records lifecycle and processing events; can be told to fail.
class RecordingStage final : public StringStage {
public:
    RecordingStage(std::string name, std::vector<std::string>& events,
                   bool failOnOpen = false, bool failOnProcess = false)
        : name_(std::move(name)), events_(events),
          failOnOpen_(failOnOpen), failOnProcess_(failOnProcess) {}

    void open() override {
        if (failOnOpen_) throw logflow::StageException("open failed: " + name_);
        events_.push_back("open:" + name_);
    }
    void close() override { events_.push_back("close:" + name_); }
    void process(const std::string& in, logflow::Emitter<std::string>& out) override {
        events_.push_back("process:" + name_ + ":" + in);
        if (failOnProcess_) throw logflow::StageException("process failed: " + name_);
        out.emit(in);
    }

private:
    std::string name_;
    std::vector<std::string>& events_;
    bool failOnOpen_;
    bool failOnProcess_;
};

/// Writes a file on construction, deletes it on destruction.
class TempFile {
public:
    TempFile(const std::string& tag, const std::string& content)
        : path_("logflow_test_" + tag + ".tmp") {
        std::ofstream(path_, std::ios::binary) << content;
    }
    ~TempFile() { std::remove(path_.c_str()); }
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    const std::string& path() const { return path_; }

private:
    std::string path_;
};

inline std::vector<std::string> readLinesViaPipeline(const std::string& path) {
    auto sink = std::make_shared<CollectingSink<std::string>>();
    logflow::from(std::make_shared<logflow::FileLineSource>(path)).to(sink).run();
    return sink->items;
}

}  // namespace support
