// Main does exactly three things: read arguments, assemble the pipeline, run it.
// No business logic lives here. Which stages exist is decided by the arguments;
// the order is fixed: parse, then the filters, then the projection, then the sink.

#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "logflow/CommandLine.hpp"
#include "logflow/ConsoleSink.hpp"
#include "logflow/FileLineSource.hpp"
#include "logflow/FileSink.hpp"
#include "logflow/ParserStage.hpp"
#include "logflow/Pipeline.hpp"
#include "logflow/PredicateFilterStage.hpp"
#include "logflow/Predicates.hpp"
#include "logflow/ProjectionStage.hpp"

int main(int argc, char* argv[]) {
    const std::string program = argc > 0 ? argv[0] : "logflow";

    try {
        // 1. Read command-line arguments.
        const logflow::Options opt =
            logflow::parseArguments(std::vector<std::string>(argv + (argc > 0 ? 1 : 0), argv + argc));

        // 2. Assemble the pipeline. Filters and projection keep the record type
        //    (LogRecord), so the builder keeps its type and stages can be added in a loop.
        auto parser = std::make_shared<logflow::ParserStage>();
        std::vector<std::shared_ptr<logflow::PredicateFilterStage>> filters;
        if (opt.minStatus) {
            filters.push_back(std::make_shared<logflow::PredicateFilterStage>(
                logflow::predicates::statusAtLeast(*opt.minStatus)));
        }
        if (opt.pathPrefix) {
            filters.push_back(std::make_shared<logflow::PredicateFilterStage>(
                logflow::predicates::pathStartsWith(*opt.pathPrefix)));
        }

        auto builder = logflow::from(std::make_shared<logflow::FileLineSource>(opt.input)).via(parser);
        for (const auto& filter : filters) builder = builder.via(filter);
        if (!opt.fields.empty()) {
            builder = builder.via(std::make_shared<logflow::ProjectionStage>(opt.fields));
        }

        std::shared_ptr<logflow::Sink<logflow::LogRecord>> sink;
        if (opt.output) {
            sink = std::make_shared<logflow::FileSink>(*opt.output);
        } else {
            sink = std::make_shared<logflow::ConsoleSink>();
        }
        auto pipeline = builder.to(sink);

        // 3. Run it, then report totals (stderr, so that stdout stays a clean
        //    stream of records).
        pipeline.run();
        std::size_t filteredOut = 0;
        for (const auto& filter : filters) filteredOut += filter->rejectedCount();
        std::cerr << "logflow: " << parser->parsedCount() << " records, "
                  << parser->malformedCount() << " malformed lines skipped, " << filteredOut
                  << " filtered out\n";
    } catch (const std::invalid_argument& e) {  // bad usage
        std::cerr << "logflow: " << e.what() << '\n' << logflow::usage(program);
        return 2;
    } catch (const std::exception& e) {  // runtime failure (I/O, ...)
        std::cerr << "logflow: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
