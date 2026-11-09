// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "document.h"
#include "text_input.h"
#include "utf8.h"
#include <string>

namespace document {
static int read_decoded_text(TextInput &input, Sink &sink)
{
    if (!sink.preformatted(true)) return LayoutError;
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
int read_text(FILE *file, Sink &sink, Limits limits)
{
    TextInput input(file, limits.input_bytes);
    return read_decoded_text(input, sink);
}
int read_text_data(const char *data, size_t size, Sink &sink, Limits limits, Encoding encoding)
{
    TextInput input(data, size, limits.input_bytes, encoding);
    return read_decoded_text(input, sink);
}

}
#endif
