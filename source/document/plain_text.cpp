// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "document.h"
#include <string>

namespace document {
int read_text(FILE *file, Sink &sink, Limits limits)
{
    if (!sink.preformatted(true)) return LayoutError;
    std::string chunk;
    size_t bytes = 0;
    bool carriage_return = false;
    for (int c; (c = fgetc(file)) != EOF;) {
        if (++bytes > limits.input_bytes) return TooLarge;
        // Flush only before a new UTF-8 sequence, never midway through one.
        if ((chunk.size() >= 512 && (c & 0xc0) != 0x80) || chunk.size() >= 516) {
            if (!sink.text(chunk.data(), chunk.size())) return LayoutError;
            chunk.clear();
        }
        if (c == '\n' && carriage_return) { carriage_return = false; continue; }
        carriage_return = c == '\r';
        chunk += (char)(carriage_return ? '\n' : c);
    }
    if (ferror(file)) return ReadError;
    return sink.text(chunk.data(), chunk.size()) ? Ok : LayoutError;
}
}
#endif
