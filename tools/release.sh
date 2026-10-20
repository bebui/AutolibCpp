#!/bin/sh
# Releases a version : sets it in CMakeLists.txt (read by the Python package), commits, tags vX.Y.Z and pushes.
# The tag makes the "Python wheels" workflow build, test and publish the wheels on PyPI.
# Usage : tools/release.sh 1.2.0
set -eu
version="$1"
echo "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || { echo "version X.Y.Z expected"; exit 1; }
[ "$(git rev-parse --abbrev-ref HEAD)" = master ] || { echo "release from master"; exit 1; }
git diff --quiet && git diff --cached --quiet || { echo "uncommitted changes"; exit 1; }
git pull --ff-only
sed -i.bak -E "0,/^    VERSION [0-9]+\.[0-9]+\.[0-9]+/s//    VERSION $version/" CMakeLists.txt && rm CMakeLists.txt.bak
git diff --quiet || git commit -qam "Release $version"
git tag -a "v$version" -m "Release $version"
git push origin master "v$version"
