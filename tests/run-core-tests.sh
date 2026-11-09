#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
cd "$(dirname "$0")/.."
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT HUP INT TERM
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/utf8_test.cpp -o "$output/utf8-test"
"$output/utf8-test"
