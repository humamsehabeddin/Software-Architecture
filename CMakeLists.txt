cmake_minimum_required(VERSION 3.16)
project(LogFlow VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library: everything except main(), so tests can link it too.
add_library(logflow_core STATIC
    src/FileLineSource.cpp
    src/ConsoleSink.cpp
    src/ParserStage.cpp
    src/Pipeline.cpp
    src/Fields.cpp
    src/Predicates.cpp
    src/PredicateFilterStage.cpp
    src/ProjectionStage.cpp
    src/FileSink.cpp
    src/CommandLine.cpp)
target_include_directories(logflow_core PUBLIC include)
if(NOT MSVC)
    target_compile_options(logflow_core PUBLIC -Wall -Wextra -Wpedantic)
endif()

add_executable(logflow src/Main.cpp)
target_link_libraries(logflow PRIVATE logflow_core)

enable_testing()
add_executable(logflow_tests
    tests/test_main.cpp
    tests/pipeline_test.cpp
    tests/components_test.cpp
    tests/parser_stage_test.cpp
    tests/predicate_filter_stage_test.cpp
    tests/projection_stage_test.cpp
    tests/file_sink_test.cpp
    tests/command_line_test.cpp
    tests/integration_test.cpp)
target_link_libraries(logflow_tests PRIVATE logflow_core)
target_compile_definitions(logflow_tests PRIVATE
    LOGFLOW_SAMPLE_LOG="${CMAKE_CURRENT_SOURCE_DIR}/data/access-small.log")
add_test(NAME unit_tests COMMAND logflow_tests)

# The ordering experiment (see ARCHITECTURE.md, section 10).
add_executable(order_experiment experiments/order_experiment.cpp)
target_link_libraries(order_experiment PRIVATE logflow_core)
