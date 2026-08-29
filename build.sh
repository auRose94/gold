#!/usr/bin/env bash
#
# Build everything: core + game + web modules, tests, and all example
# projects, then run the test suite.
#
# Usage:
#   ./build.sh             build everything + examples, run tests
#   ./build.sh --core      build only the shared core (fast), run tests
#   ./build.sh --clean     wipe the build directory first
#   ./build.sh --no-test   configure and build, skip the test run
#
set -euo pipefail

cd "$(dirname "$0")"

BUILD_DIR="build"
BUILD_CORE=0
BUILD_TEST=1
CLEAN=0
JOBS="${JOBS:-$(nproc)}"

for arg in "$@"; do
	case "$arg" in
		--core) BUILD_CORE=1 ;;
		--clean) CLEAN=1 ;;
		--no-test) BUILD_TEST=0 ;;
		*)
			echo "unknown option: $arg" >&2
			exit 1
			;;
	esac
done

if [[ "$CLEAN" -eq 1 ]]; then
	rm -rf "$BUILD_DIR"
fi

# Everything (game + web + examples) needs OpenSSL and the bundled 3rdParty.
# The core-only build is the fast path with the smallest dependency surface.
if [[ "$BUILD_CORE" -eq 1 ]]; then
	echo "== Configuring core-only build =="
	cmake -S . -B "$BUILD_DIR" \
		-DGOLD_BUILD_GAME=OFF \
		-DGOLD_BUILD_WEB=OFF
else
	echo "== Configuring full build (modules + tests + examples) =="
	cmake -S . -B "$BUILD_DIR" \
		-DGOLD_BUILD_GAME=ON \
		-DGOLD_BUILD_WEB=ON \
		-DGOLD_BUILD_TESTS=ON \
		-DGOLD_BUILD_EXAMPLES=ON
fi

echo "== Building (jobs=$JOBS) =="
cmake --build "$BUILD_DIR" -j"$JOBS"

if [[ "$BUILD_TEST" -eq 1 ]]; then
	echo "== Running tests =="
	ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

echo "== Done =="