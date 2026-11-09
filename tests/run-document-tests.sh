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
python3 tests/make_text_fixtures.py "$output/fixtures"
python3 tests/make_mobi_fixtures.py "$output/fixtures"
python3 tests/make_package_fixtures.py "$output/fixtures"
python3 tests/make_legacy_fixtures.py "$output/fixtures"
for suite in document formats mobi package legacy; do
    c++ -std=c++17 -DDSL_DOCUMENTS -DIOAPI_NO_64 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Iinclude -Ibuild/deps-host/include -Isource/document source/document/*.cpp source/ioapi.c source/unzip.c "tests/${suite}_test.cpp" -Lbuild/deps-host/lib -lmd4c -lpdfio -lexpat -lmobi -lz -lm -o "$output/$suite-test"
    "$output/$suite-test" "$output/fixtures"
done

c++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Iinclude -Ibuild/deps-host/include tests/prefs_xml_test.cpp -Lbuild/deps-host/lib -lexpat -o "$output/prefs-xml-test"
"$output/prefs-xml-test"
