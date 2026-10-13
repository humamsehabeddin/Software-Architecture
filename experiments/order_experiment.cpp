// Ordering experiment (Increment 3, task 4). `make experiment` builds and runs it.
//
// Question: stages compose in any order. Does the order change the RESULT?
// Does it change the COST? Same data, same stages, different order:
//
//   A. two filters:            status >= 400  then  path /api     vs  the reverse
//   B. filter and projection:  filter, then project               vs  project, then filter
//   C. the broken case:        project away `status`, then filter on `status`
//
// The input is the sample log repeated in memory (no disk in the timings).
// Output is never written to a file; every run feeds a sink that counts rows and
// hashes their CSV text, so "same result" means "same count and same hash".
//
// usage: order_experiment [logfile] [repeat] [runs]

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "logflow/FileLineSource.hpp"
#include "logflow/Fields.hpp"
#include "logflow/ParserStage.hpp"
#include "logflow/Pipeline.hpp"
#include "logflow/PredicateFilterStage.hpp"
#include "logflow/Predicates.hpp"
#include "logflow/ProjectionStage.hpp"

using namespace logflow;
namespace pred = logflow::predicates;

namespace {

/// Emits the same lines again and again; keeps the file system out of the timings.
class RepeatingSource final : public Source<std::string> {
public:
    RepeatingSource(std::vector<std::string> lines, int repeat)
        : lines_(std::move(lines)), repeat_(repeat) {}
    void produce(Emitter<std::string>& out) override {
        for (int r = 0; r < repeat_; ++r)
            for (const std::string& l : lines_) out.emit(l);
    }

private:
    std::vector<std::string> lines_;
    int repeat_;
};

/// Counts rows and hashes what a FileSink would have written (FNV-1a).
class DigestSink final : public Sink<LogRecord> {
public:
    void consume(const LogRecord& r) override {
        ++rows;
        for (const std::string& name : fields::selected(r)) {
            for (char c : fields::csvCell(fields::valueOf(r, name))) mix(c);
            mix(',');
        }
        mix('\n');
    }
    std::size_t rows = 0;
    std::uint64_t hash = 1469598103934665603ULL;

private:
    void mix(char c) { hash = (hash ^ static_cast<unsigned char>(c)) * 1099511628211ULL; }
};

struct Outcome {
    double bestMs = 0, medianMs = 0;
    std::size_t rows = 0;
    std::uint64_t hash = 0;
    std::size_t evaluations = 0;  // predicate calls, summed over the filters
};

using Stages = std::vector<std::shared_ptr<Stage<LogRecord, LogRecord>>>;
using Filters = std::vector<std::shared_ptr<PredicateFilterStage>>;

/// Runs parse -> stages -> digest `runs` times (fresh components every time).
template <typename MakeStages>
Outcome measure(const std::vector<std::string>& lines, int repeat, int runs, MakeStages make) {
    std::vector<double> ms;
    Outcome o;
    for (int i = 0; i < runs + 1; ++i) {  // run 0 is a warm-up and is dropped
        Filters filters;
        Stages stages = make(filters);
        auto sink = std::make_shared<DigestSink>();
        auto b = from(std::make_shared<RepeatingSource>(lines, repeat))
                     .via(std::make_shared<ParserStage>());
        for (const auto& s : stages) b = b.via(s);
        Pipeline p = b.to(sink);

        const auto t0 = std::chrono::steady_clock::now();
        p.run();
        const auto t1 = std::chrono::steady_clock::now();

        if (i == 0) continue;
        ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        o.rows = sink->rows;
        o.hash = sink->hash;
        o.evaluations = 0;
        for (const auto& f : filters) o.evaluations += f->seenCount();
    }
    std::sort(ms.begin(), ms.end());
    o.bestMs = ms.front();
    o.medianMs = ms[ms.size() / 2];
    return o;
}

void row(const std::string& label, const Outcome& o) {
    std::cout << "  " << std::left << std::setw(46) << label << std::right << std::setw(9)
              << o.rows << " rows  " << std::setw(10) << o.evaluations << " predicate calls  "
              << std::fixed << std::setprecision(1) << std::setw(8) << o.bestMs << " ms best  "
              << std::setw(8) << o.medianMs << " ms median\n";
}

void verdict(const Outcome& a, const Outcome& b) {
    std::cout << "  -> same result: " << (a.rows == b.rows && a.hash == b.hash ? "YES" : "NO")
              << ",  time ratio (second / first, best of runs): " << std::setprecision(2)
              << b.bestMs / a.bestMs << "\n\n";
}

std::shared_ptr<PredicateFilterStage> filter(Filters& keep, Predicate<LogRecord> p) {
    auto f = std::make_shared<PredicateFilterStage>(std::move(p));
    keep.push_back(f);
    return f;
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::string file = argc > 1 ? argv[1] : "data/access-small.log";
    const int repeat = argc > 2 ? std::atoi(argv[2]) : 2000;
    const int runs = argc > 3 ? std::atoi(argv[3]) : 7;

    std::vector<std::string> lines;
    {
        struct Collect final : Emitter<std::string> {
            std::vector<std::string>& v;
            explicit Collect(std::vector<std::string>& x) : v(x) {}
            void emit(std::string s) override { v.push_back(std::move(s)); }
        } collect(lines);
        FileLineSource(file).produce(collect);
    }
    std::cout << "input: " << file << " x " << repeat << " = " << lines.size() * repeat
              << " lines;  " << runs << " timed runs each (+1 warm-up)\n\n";

    const std::vector<std::string> kept = {"timestamp", "clientIp", "status", "path"};
    const std::vector<std::string> noStatus = {"timestamp", "clientIp", "path"};

    std::cout << "A. Two filters. Order should not change the result.\n";
    auto a1 = measure(lines, repeat, runs, [](Filters& f) {
        return Stages{filter(f, pred::statusAtLeast(400)), filter(f, pred::pathStartsWith("/api"))};
    });
    auto a2 = measure(lines, repeat, runs, [](Filters& f) {
        return Stages{filter(f, pred::pathStartsWith("/api")), filter(f, pred::statusAtLeast(400))};
    });
    row("status>=400, then path /api", a1);
    row("path /api, then status>=400", a2);
    verdict(a1, a2);

    std::cout << "B. Filter and projection. Order should not change the result\n"
                 "   (the projection keeps `status`, which the filter needs).\n";
    auto b1 = measure(lines, repeat, runs, [&](Filters& f) {
        return Stages{filter(f, pred::statusAtLeast(400)), std::make_shared<ProjectionStage>(kept)};
    });
    auto b2 = measure(lines, repeat, runs, [&](Filters& f) {
        return Stages{std::make_shared<ProjectionStage>(kept), filter(f, pred::statusAtLeast(400))};
    });
    row("filter status>=400, then project", b1);
    row("project, then filter status>=400", b2);
    verdict(b1, b2);

    std::cout << "C. Projection that DROPS `status`, then a filter on `status`.\n"
                 "   Composition still compiles and runs; the result is silently wrong.\n";
    auto c1 = measure(lines, repeat, runs, [&](Filters& f) {
        return Stages{filter(f, pred::statusAtLeast(400)), std::make_shared<ProjectionStage>(noStatus)};
    });
    auto c2 = measure(lines, repeat, runs, [&](Filters& f) {
        return Stages{std::make_shared<ProjectionStage>(noStatus), filter(f, pred::statusAtLeast(400))};
    });
    row("filter status>=400, then project (no status)", c1);
    row("project (no status), then filter status>=400", c2);
    verdict(c1, c2);
    return 0;
}
