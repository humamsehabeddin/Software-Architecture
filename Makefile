# LogFlow build. Works with the compiler that ships with Apple's Command Line
# Tools (clang++) or with g++. No Xcode project needed.
#
#   make          build build/logflow
#   make test     build and run the unit tests
#   make check    run logflow on the sample log and verify the output
#   make coverage line coverage of the unit tests (needs gcov)
#   make run      run logflow on the sample log
#   make clean    remove build/

CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Wpedantic
CPPFLAGS := -Iinclude -MMD -MP
BUILD    := build
SAMPLE   := data/access-small.log

CORE_SRC := src/FileLineSource.cpp src/ConsoleSink.cpp src/ParserStage.cpp src/Pipeline.cpp
CORE_OBJ := $(patsubst src/%.cpp,$(BUILD)/src/%.o,$(CORE_SRC))
MAIN_OBJ := $(BUILD)/src/Main.o
TEST_SRC := $(wildcard tests/*.cpp)
TEST_OBJ := $(patsubst tests/%.cpp,$(BUILD)/tests/%.o,$(TEST_SRC))

.PHONY: all test check run coverage clean

all: $(BUILD)/logflow

$(BUILD)/logflow: $(MAIN_OBJ) $(CORE_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/src/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DLOGFLOW_SAMPLE_LOG='"$(CURDIR)/$(SAMPLE)"' -c $< -o $@

$(BUILD)/logflow_tests: $(TEST_OBJ) $(CORE_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: $(BUILD)/logflow_tests
	cd $(BUILD) && ./logflow_tests

check: $(BUILD)/logflow
	@lines=$$(wc -l < $(SAMPLE)); \
	out=$$(./$(BUILD)/logflow $(SAMPLE) 2>$(BUILD)/check.err | wc -l); \
	cat $(BUILD)/check.err; \
	if [ "$$out" -eq "$$lines" ] && grep -q "0 malformed" $(BUILD)/check.err; then \
	  echo "OK: $$out input lines -> $$out records, 0 malformed"; \
	else echo "FAIL: $$lines input lines, $$out output lines"; exit 1; fi

coverage:
	@scripts/coverage.sh

run: $(BUILD)/logflow
	./$(BUILD)/logflow $(SAMPLE)

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*/*.d)
