# Change log

## v3: Increment 3, composition: filtering, projection and the pipeline builder

### Added
- `include/logflow/PredicateFilterStage.hpp`, `src/PredicateFilterStage.cpp`: forwards a
  record only if a `Predicate<LogRecord>` accepts it; counts seen/passed.
- `include/logflow/Predicates.hpp`, `src/Predicates.cpp`: `Predicate<T>` alias and the
  `statusAtLeast`, `pathStartsWith`, `timestampBetween` factories.
- `include/logflow/ProjectionStage.hpp`, `src/ProjectionStage.cpp`: reduces a record to
  selected fields (drops `raw` and attributes).
- `include/logflow/FileSink.hpp`, `src/FileSink.cpp`: CSV output file with a real
  `open()`/`close()` lifecycle.
- `include/logflow/Fields.hpp`, `src/Fields.cpp`: field names, text values, projection
  marker, CSV quoting.
- `include/logflow/CommandLine.hpp`, `src/CommandLine.cpp`: `--output`, `--min-status`,
  `--path-prefix`, `--fields`.
- `experiments/order_experiment.cpp`, `make experiment`: the stage-order experiment.
- Tests: `predicate_filter_stage_test.cpp`, `projection_stage_test.cpp`,
  `file_sink_test.cpp`, `command_line_test.cpp`, `integration_test.cpp`, five lifecycle
  tests in `pipeline_test.cpp` (45 new, 85 in total).

### Changed
- `include/logflow/Sink.hpp`: `open()` / `close()` hooks (default: do nothing).
- `include/logflow/Pipeline.hpp`, `src/Pipeline.cpp`: the sink joins the lifecycle (opened
  after the stages, closed before them); `PipelineBuilder::then()` renamed `via()`.
- `src/ConsoleSink.cpp`: prints projected records as `name=value`; the ISO-8601
  formatter moved to `TimeUtil.hpp`.
- `src/Main.cpp`: builds the pipeline from the command-line options.
- `Makefile`, `CMakeLists.txt`: new sources, tests and experiment; `make check` also
  verifies a filtered, projected file against `awk`; CMake version 3.0.0.
- `tests/test_support.hpp`: `LifecycleSink`, `makeRecord`, `TempPath`, `readFile`.
- `README.md`, `ARCHITECTURE.md` (section 10), this file.

### Unchanged
`Source.hpp`, `Emitter.hpp`, `StageException.hpp`, `Stage.hpp` (code), `LogRecord.hpp`,
`ParserStage`, `FileLineSource`, `tests/mini_test.hpp`, `tests/test_main.cpp`.

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
