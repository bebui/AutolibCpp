#!/bin/sh
# Regenerates the results of the Java library fr.menana:automaton used by the tests : tests/data/java_oracle.tsv,
# tests/data/java_counts.tsv and tests/data/java_weighted.tsv.
# Usage : tools/java-oracle/generate.sh path/to/automaton-4.0.0.jar [seed] [count]
set -eu
jar="$1"
seed="${2:-2025}"
count="${3:-600}"
here="$(cd "$(dirname "$0")" && pwd)"
build="$(mktemp -d)"
javac -cp "$jar" -d "$build" "$here/Oracle.java" "$here/Count.java" "$here/WeightedOracle.java"
java -cp "$jar:$build" Oracle "$seed" "$count" > "$here/../../tests/data/java_oracle.tsv"
java -cp "$jar:$build" Count > "$here/../../tests/data/java_counts.tsv"
java -cp "$jar:$build" WeightedOracle 2026 400 > "$here/../../tests/data/java_weighted.tsv"
rm -rf "$build"
