#!/bin/sh
# Regenerates tests/data/java_oracle.tsv and tests/data/java_counts.tsv from the Java library fr.menana:automaton.
# Usage : tools/java-oracle/generate.sh path/to/automaton-4.0.0.jar [seed] [count]
set -eu
jar="$1"
seed="${2:-2025}"
count="${3:-600}"
here="$(cd "$(dirname "$0")" && pwd)"
build="$(mktemp -d)"
javac -cp "$jar" -d "$build" "$here/Oracle.java" "$here/Count.java"
java -cp "$jar:$build" Oracle "$seed" "$count" > "$here/../../tests/data/java_oracle.tsv"
java -cp "$jar:$build" Count > "$here/../../tests/data/java_counts.tsv"
rm -rf "$build"
