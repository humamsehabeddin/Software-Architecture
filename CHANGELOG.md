# Change log

## v2: Increment 2, typed records and the first real stage

### Added
- `include/logflow/LogRecord.hpp`: immutable domain record (timestamp, clientIp,
  method, path, status, bytes, userAgent, attributes, raw).
- `include/logflow/ParserStage.hpp`, `src/ParserStage.cpp`: Common/combined Log
  Format parser; malformed lines are skipped and counted.
- `include/logflow/TimeUtil.hpp`: portable date arithmetic.
- `tests/parser_stage_test.cpp`: 16 parser tests (no file access).
- `scripts/coverage.sh`, `make coverage`: line-coverage report.
- `data/access-with-errors.log`: small log with malformed lines for demos.
- `CHANGELOG.md` (this file).

### Changed
- `include/logflow/ConsoleSink.hpp`, `src/ConsoleSink.cpp`: sink now consumes
  `LogRecord` and prints one readable line per record.
- `src/Main.cpp`: adds the parser stage, prints record/malformed totals at the end.
- `tests/test_support.hpp`: adds the `CollectingEmitter<T>` test double.
- `tests/components_test.cpp`: console-sink tests updated for records; the old
  "output is byte-identical to the input" test is replaced by an end-to-end
  parse test (that property no longer holds by design).
- `Makefile`, `CMakeLists.txt`: build the new sources/tests; `make check` now
  verifies 200 lines -> 200 records; new `make coverage`; CMake version 2.0.0.
- `README.md`, `ARCHITECTURE.md`: Increment 2 documentation, coverage figure.

### Unchanged (the point of the exercise)
`Pipeline.hpp`, `Pipeline.cpp`, `Source.hpp`, `Stage.hpp`, `Emitter.hpp`,
`Sink.hpp`, `StageException.hpp`, `FileLineSource.hpp/.cpp`,
`tests/pipeline_test.cpp`, `tests/mini_test.hpp`, `tests/test_main.cpp`.

## v1: Increment 1, the skeleton pipeline
