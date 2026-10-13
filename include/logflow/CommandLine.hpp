#pragma once

#include <optional>
#include <string>
#include <vector>

namespace logflow {

/// What the command line asks for. Each filter that is not set is simply not
/// added to the pipeline.
struct Options {
    std::string input;                    // <logfile>
    std::optional<std::string> output;    // --output FILE   (absent: print to the console)
    std::optional<int> minStatus;         // --min-status N  keep records with status >= N
    std::optional<std::string> pathPrefix;  // --path-prefix P keep records whose path starts with P
    std::vector<std::string> fields;      // --fields a,b,c  keep only these fields (empty: all)
};

/// Turns the arguments (without the program name) into Options.
/// Throws std::invalid_argument with a message for the user on any mistake:
/// no/extra input file, unknown option, missing or malformed value, unknown
/// field name, a repeated option, or --output naming the input file (the sink
/// truncates its file, which would destroy the log being read).
Options parseArguments(const std::vector<std::string>& args);

/// One-paragraph usage text.
std::string usage(const std::string& program);

}  // namespace logflow
