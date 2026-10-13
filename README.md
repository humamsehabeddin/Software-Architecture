# LogFlow

A log-processing pipeline, built incrementally as a Software Architecture
term project. **This is Increment 3: composition (filtering, projection and the
pipeline builder).**

`logflow` reads an access log, parses it, filters it, keeps only the fields you
ask for, and writes the result, through a `Source → (Stages) → Sink` pipeline
rather than through a `main()` that does everything. See
[ARCHITECTURE.md](ARCHITECTURE.md) for the design and the stage-order experiment
(section 10).

## Requirements

- A C++17 compiler (Apple clang from the **Command Line Tools**, or g++)
- `make`

On a Mac you do **not** need the full Xcode app. If `clang++ --version` fails,
run `xcode-select --install`; that also installs `git`.

## Build, run, test

```sh
make          # builds build/logflow
make run      # runs it on data/access-small.log
make test     # unit + integration tests (85)
make check    # runs the real executable on the sample log: 200 lines -> 200 records, and a
              # filtered/projected file whose row count is checked against awk
make coverage # builds the tests with gcov instrumentation and prints line coverage
make experiment # the filter-order experiment (ARCHITECTURE.md, section 10)
make clean
```

## Command line

```
logflow <logfile> [--output FILE] [--min-status N] [--path-prefix PREFIX] [--fields f1,f2,...]
```

| Option | Effect |
|---|---|
| `--min-status N` | keep records with status >= N (100-599) |
| `--path-prefix P` | keep records whose request path starts with P (query string included) |
| `--fields a,b,c` | keep only these fields, in this order. Names: `timestamp, clientIp, method, path, status, bytes, userAgent` |
| `--output FILE` | write CSV (no header) to FILE; without it the result is printed to the console |

```sh
./build/logflow data/access-small.log --min-status 400 --path-prefix /api \
    --fields timestamp,clientIp,status,path --output errors.csv
# logflow: 200 records, 0 malformed lines skipped, 190 filtered out
head -2 errors.csv
# 2026-09-21T09:00:13Z,192.0.2.131,500,/api/orders
# 2026-09-21T09:00:45Z,203.0.113.61,401,/api/login
```

`--output` may not be the input file (the output file is truncated when the run
starts).

Without `make`:

```sh
clang++ -std=c++17 -Iinclude src/*.cpp -o logflow && ./logflow data/access-small.log
```

CMake is also supported (`cmake -S . -B build-cmake && cmake --build build-cmake`,
then `ctest --test-dir build-cmake`), and `cmake -G Xcode` generates an Xcode
project if you want one later.

Exit codes: `0` success, `1` runtime error (for example a missing file), `2` wrong usage.

## Layout

```
include/logflow/   the contracts and public headers
  Emitter.hpp  Source.hpp  Stage.hpp  Sink.hpp  StageException.hpp
  LogRecord.hpp                      (the immutable domain record)
  Pipeline.hpp                       (the connector and builder; depends only on the above)
  FileLineSource.hpp  ParserStage.hpp  ConsoleSink.hpp           (Increments 1-2)
  PredicateFilterStage.hpp  ProjectionStage.hpp  FileSink.hpp    (Increment 3)
  Predicates.hpp  Fields.hpp  CommandLine.hpp                    (helpers for the above)
  TimeUtil.hpp                       (portable calendar arithmetic)
src/               one .cpp per header above, plus Main.cpp
tests/             unit + integration tests, tiny test harness, test doubles
experiments/       order_experiment.cpp  (stage order: result vs. cost)
scripts/           coverage.sh
data/              access-small.log        (200 valid lines, combined access-log format)
                   access-with-errors.log  (7 lines, 4 of them malformed, for demos)
ARCHITECTURE.md   design decisions and the experiment;  CHANGELOG.md  what changed in each increment
```

## Using the pipeline

```cpp
using namespace logflow;
auto parser = std::make_shared<ParserStage>();
auto pipeline = from(std::make_shared<FileLineSource>(in))
                    .via(parser)                                                    // stages, in order
                    .via(std::make_shared<PredicateFilterStage>(predicates::statusAtLeast(400)))
                    .via(std::make_shared<ProjectionStage>(
                        std::vector<std::string>{"timestamp", "clientIp", "status", "path"}))
                    .to(std::make_shared<FileSink>(out));
pipeline.run();   // opens the stages and the sink, streams the records, closes everything
// parser->malformedCount() now holds the number of skipped lines
```

## Versions

| Tag | Increment |
|---|---|
| `v1` | The skeleton pipeline |
| `v2` | `LogRecord`, `ParserStage`, collecting-emitter tests |
| `v3` | `PredicateFilterStage`, `ProjectionStage`, `FileSink`, `via()` builder, command-line options, stage-order experiment |

## Test coverage (v3)

Measured with `make coverage` (g++ 13 + gcov, `-O0`, Linux). The test suite (85 tests)
covers **95.4 %** of the lines in `src/*.cpp` (307 lines):

| File | Line coverage |
| ---- | ------------- |
| `src/Pipeline.cpp`, `src/PredicateFilterStage.cpp`, `src/Predicates.cpp`, `src/ProjectionStage.cpp` | 100 % |
| `src/ParserStage.cpp` | 97.3 % |
| `src/ConsoleSink.cpp` | 94.7 % |
| `src/Fields.cpp` | 93.2 % |
| `src/FileLineSource.cpp` | 92.3 % |
| `src/FileSink.cpp` | 91.3 % |
| `src/CommandLine.cpp` | 90.9 % |

`src/Main.cpp` is excluded: it only wires components together and is exercised
by `make check`, not by the unit tests. Header-only code is not counted. Re-run
`make coverage` on your machine; the numbers differ slightly between compilers
(Apple's llvm-cov counts lines a little differently).
