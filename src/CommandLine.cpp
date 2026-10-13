#include "logflow/CommandLine.hpp"

#include <stdexcept>

#include "logflow/Fields.hpp"

namespace logflow {
namespace {

int parseStatus(const std::string& text) {
    if (text.size() != 3 || text.find_first_not_of("0123456789") != std::string::npos ||
        text < "100" || text > "599") {
        throw std::invalid_argument("--min-status needs a number from 100 to 599, got: " + text);
    }
    return std::stoi(text);
}

}  // namespace

Options parseArguments(const std::vector<std::string>& args) {
    Options o;
    bool haveInput = false, haveStatus = false, havePrefix = false, haveFields = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a.compare(0, 2, "--") != 0) {
            if (haveInput) throw std::invalid_argument("more than one input file: " + a);
            o.input = a;
            haveInput = true;
            continue;
        }
        if (i + 1 >= args.size()) throw std::invalid_argument(a + " needs a value");
        const std::string& v = args[++i];
        if (a == "--output") {
            if (o.output) throw std::invalid_argument("--output given twice");
            o.output = v;
        } else if (a == "--min-status") {
            if (haveStatus) throw std::invalid_argument("--min-status given twice");
            o.minStatus = parseStatus(v);
            haveStatus = true;
        } else if (a == "--path-prefix") {
            if (havePrefix) throw std::invalid_argument("--path-prefix given twice");
            o.pathPrefix = v;
            havePrefix = true;
        } else if (a == "--fields") {
            if (haveFields) throw std::invalid_argument("--fields given twice");
            o.fields = fields::split(v, ',');
            for (const std::string& f : o.fields) {
                if (!fields::isKnown(f)) throw std::invalid_argument("unknown field: " + f);
            }
            haveFields = true;
        } else {
            throw std::invalid_argument("unknown option: " + a);
        }
    }
    if (!haveInput) throw std::invalid_argument("no input file given");
    if (o.output && *o.output == o.input) {
        throw std::invalid_argument("--output must not be the input file");
    }
    return o;
}

std::string usage(const std::string& program) {
    return "usage: " + program +
           " <logfile> [--output FILE] [--min-status N] [--path-prefix PREFIX]\n"
           "       [--fields f1,f2,...]\n"
           "fields: " + fields::join(fields::allNames(), ',') + "\n"
           "Without --output the result is printed to the console.\n";
}

}  // namespace logflow
