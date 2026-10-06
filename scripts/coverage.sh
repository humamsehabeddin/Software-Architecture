#!/bin/sh
# Line coverage of the compiled implementation files (src/*.cpp, without Main.cpp,
# which only wires things together and is exercised by `make check`, not by the
# unit tests). Builds the tests with coverage instrumentation in build/cov/,
# runs them, and asks gcov for the numbers. Usage:  make coverage
set -e
cd "$(dirname "$0")/.."

CXX_BIN="${CXX:-c++}"
# Apple's gcov is really llvm-cov; GNU gcov is used elsewhere.
if [ "$(uname)" = "Darwin" ]; then GCOV="xcrun llvm-cov gcov"; else GCOV="${GCOV:-gcov}"; fi

OUT=build/cov
rm -rf "$OUT"
mkdir -p "$OUT"

SRC=$(ls src/*.cpp | grep -v 'src/Main.cpp')
"$CXX_BIN" -std=c++17 -O0 -g --coverage -Iinclude \
    -DLOGFLOW_SAMPLE_LOG="\"$PWD/data/access-small.log\"" \
    $SRC tests/*.cpp -o "$OUT/logflow_tests"

( cd "$OUT" && ./logflow_tests | tail -1 )

echo
echo "Line coverage (src/*.cpp, excluding Main.cpp):"
cd "$OUT"
total_lines=0
covered_lines=0
for f in $(cd ../..; ls $SRC); do
    base=$(basename "$f" .cpp)
    # gcc names the data files <exe>-<source>.gcda
    gcda=$(ls ./*"-$base.gcda" 2>/dev/null | head -1)
    [ -n "$gcda" ] || continue
    line=$($GCOV -n "$gcda" 2>/dev/null | awk -v want="src/$base.cpp" '
        /^File/ { cur = ($0 ~ want "\x27") }
        cur && /^Lines executed/ { print; exit }')
    pct=$(echo "$line" | sed -E 's/.*executed:([0-9.]+)%.*/\1/')
    n=$(echo "$line" | sed -E 's/.* of ([0-9]+).*/\1/')
    printf '  %-26s %6s%%  (%s lines)\n' "$f" "$pct" "$n"
    total_lines=$((total_lines + n))
    covered_lines=$(awk -v c="$covered_lines" -v p="$pct" -v n="$n" 'BEGIN{printf "%.4f", c + p*n/100}')
done
awk -v c="$covered_lines" -v n="$total_lines" 'BEGIN{printf "\n  TOTAL: %.1f%% of %d lines\n", (n ? 100*c/n : 0), n}'
