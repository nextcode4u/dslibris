#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
ulimit -c 0
cd "$(dirname "$0")/.."
rebuild=false
for input in tools/prepare_dependencies.py ports/dependencies.json ports/pdfio_compat.c ports/expat_config.h; do
    if [ ! -f build/deps-host/.ready ] || [ "$input" -nt build/deps-host/.ready ]; then
        rebuild=true
    fi
done
if "$rebuild"; then
    python3 tools/prepare_dependencies.py --host
fi
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT HUP INT TERM
python3 tests/make_document_fixtures.py "$output/fixtures"
c++ -std=c++17 -DDSL_DOCUMENTS -Wall -Wextra -Werror -g -fsanitize=address,undefined -Iinclude -Ibuild/deps-host/include -Isource/document source/document/*.cpp tests/document_test.cpp -Lbuild/deps-host/lib -lmd4c -lpdfio -lz -lm -o "$output/document-test"
"$output/document-test" "$output/fixtures"
