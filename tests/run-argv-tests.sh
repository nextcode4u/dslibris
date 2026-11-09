#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
ulimit -c 0
cd "$(dirname "$0")/.."
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT HUP INT TERM
cc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/argv_path_test.c -o "$output/argv-path-test"
"$output/argv-path-test"
cp source/argv_launch.cpp "$output/argv_launch.cpp"
c++ -std=c++17 -DDSL_DOCUMENTS -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/stubs -Iinclude "$output/argv_launch.cpp" tests/argv_launch_test.cpp -o "$output/argv-launch-test"
"$output/argv-launch-test"
