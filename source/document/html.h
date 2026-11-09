// SPDX-License-Identifier: MIT
#pragma once
#include "document.h"
#include "text_input.h"
namespace document {
int read_html_data(const char *data, size_t size, Sink &sink, Limits limits, Encoding encoding, bool honor_charset = false);
}
