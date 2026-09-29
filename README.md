# LogFlow

A log-processing pipeline, built incrementally as a Software Architecture
term project. **This is Increment 1: the skeleton pipeline.**

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
make test     # unit tests (22)
make check    # runs the real executable and diffs its output with the input
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
  Pipeline.hpp                       (the connector; depends only on the above)
  FileLineSource.hpp  ConsoleSink.hpp  (the two implementations)
src/               FileLineSource.cpp  ConsoleSink.cpp  Pipeline.cpp  Main.cpp
tests/             unit tests + tiny test harness
data/              access-small.log   (200 lines, common access-log format)
ARCHITECTURE.md   two-box diagram and design decisions
```

## Using the pipeline

```cpp
auto pipeline = logflow::from(std::make_shared<logflow::FileLineSource>(path))
                    // .then(std::make_shared<SomeStage>())   // stages go here, in order
                    .to(std::make_shared<logflow::ConsoleSink>());
pipeline.run();
```

## Versions

| Tag | Increment |
|---|---|
| `v1` | The skeleton pipeline |
