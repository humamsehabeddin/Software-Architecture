// Main does exactly three things: read arguments, assemble the pipeline, run it.
// No business logic lives here.

#include <exception>
#include <iostream>
#include <memory>

#include "logflow/ConsoleSink.hpp"
#include "logflow/FileLineSource.hpp"
#include "logflow/ParserStage.hpp"
#include "logflow/Pipeline.hpp"

int main(int argc, char* argv[]) {
    // 1. Read command-line arguments.
    if (argc != 2) {
        std::cerr << "usage: " << (argc > 0 ? argv[0] : "logflow") << " <logfile>\n";
        return 2;
    }

    try {
        // 2. Assemble the pipeline.
        auto parser = std::make_shared<logflow::ParserStage>();
        auto pipeline = logflow::from(std::make_shared<logflow::FileLineSource>(argv[1]))
                            .then(parser)
                            .to(std::make_shared<logflow::ConsoleSink>());

        // 3. Run it, then report the total of skipped lines (stderr, so that
        //    stdout stays a clean stream of records).
        pipeline.run();
        std::cerr << "logflow: " << parser->parsedCount() << " records, "
                  << parser->malformedCount() << " malformed lines skipped\n";
    } catch (const std::exception& e) {
        std::cerr << "logflow: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
