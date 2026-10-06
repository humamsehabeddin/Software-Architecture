# LogFlow

A log-processing pipeline, built incrementally as a Software Architecture
term project. **This is Increment 2: typed records and the first real stage.**

`logflow data/access-small.log` reads a text file and prints every line to the
console, through a `Source → (Stages) → Sink` pipeline rather than through a
`main()` that does everything. See [ARCHITECTURE.md](ARCHITECTURE.md) for the
design.

## Requirements

- A C++17 compiler (Apple clang from the **Command Line Tools**, or g++)
- `make`

On a Mac you do **not** need the full Xcode app. If `clang++ --version` fails,
run `xcode-select --install`; that also installs `git`.

## Build, run, test

```sh
make          # builds build/logflow
make run      # runs it on data/access-small.log
make test     # unit tests (40)
make check    # runs the real executable on the sample log and verifies 200 lines -> 200 records
make coverage # builds the tests with gcov instrumentation and prints line coverage
make clean
```

Or run it on any file:

```sh
./build/logflow path/to/some.log
```

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
  Pipeline.hpp                       (the connector; depends only on the above)
  FileLineSource.hpp  ParserStage.hpp  ConsoleSink.hpp  (the implementations)
  TimeUtil.hpp                       (portable calendar arithmetic)
src/               FileLineSource.cpp  ParserStage.cpp  ConsoleSink.cpp  Pipeline.cpp  Main.cpp
tests/             unit tests + tiny test harness + test doubles (CollectingEmitter, ...)
scripts/           coverage.sh
data/              access-small.log        (200 valid lines, combined access-log format)
                   access-with-errors.log  (7 lines, 4 of them malformed, for demos)
ARCHITECTURE.md   design decisions;  CHANGELOG.md  what changed in each increment
ARCHITECTURE.md   two-box diagram and design decisions
```

## Using the pipeline

```cpp
auto parser = std::make_shared<logflow::ParserStage>();
auto pipeline = logflow::from(std::make_shared<logflow::FileLineSource>(path))
                    .then(parser)                              // stages go here, in order
                    .to(std::make_shared<logflow::ConsoleSink>());
pipeline.run();
// parser->malformedCount() now holds the number of skipped lines
```

## Versions

| Tag | Increment |
|---|---|
| `v1` | The skeleton pipeline |
| `v2` | `LogRecord`, `ParserStage`, collecting-emitter tests |

## Test coverage (v2)

Measured with `make coverage` (Apple clang + llvm-cov gcov, `-O0`). The unit-test suite
(40 tests) covers **94.1 %** of the lines in `src/*.cpp` (186 lines):

| File | Line coverage |
| ---- | ------------- |
| `src/ParserStage.cpp` | 97.5 % |
| `src/FileLineSource.cpp` | 92.9 % |
| `src/ConsoleSink.cpp` | 81.5 % |
| `src/Pipeline.cpp` | 91.7 % |

`src/Main.cpp` is excluded: it only wires components together and is exercised
by `make check`, not by the unit tests. Header-only code is not counted. Re-run
`make coverage` on your machine; the numbers can differ slightly between
compilers.
