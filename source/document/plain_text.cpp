// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "document.h"
#include "text_input.h"
#include "utf8.h"
#include <string>

namespace document {
int read_text(FILE *file, Sink &sink, Limits limits)
{
    if (!sink.preformatted(true)) return LayoutError;
    TextInput input(file, limits.input_bytes);
    std::string chunk;
    bool carriage_return = false;
    for (int32_t code; (code = input.next()) >= 0;) {
        if (code == '\n' && carriage_return) { carriage_return = false; continue; }
        carriage_return = code == '\r';
        char bytes[4];
        chunk.append(bytes, utf8_encode(carriage_return ? '\n' : code, bytes));
        if (chunk.size() >= 512) {
            if (!sink.text(chunk.data(), chunk.size())) return LayoutError;
            chunk.clear();
        }
    }
    if (input.status()) return input.status();
    return sink.text(chunk.data(), chunk.size()) ? Ok : LayoutError;
}
}
#endif
