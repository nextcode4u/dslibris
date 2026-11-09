// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "entities.h"
#include "utf8.h"
extern "C" {
#include "entity.h"
}

namespace document {
static void append(std::string &text, uint32_t code)
{
    char bytes[4];
    text.append(bytes, utf8_encode(code ? code : 0xfffd, bytes));
}
std::string decode_entity(const std::string &entity)
{
    if (entity.size() < 3 || entity.front() != '&' || entity.back() != ';') return entity;
    std::string result;
    if (entity[1] == '#') {
        size_t i = 2;
        unsigned base = 10;
        if (entity[i] == 'x' || entity[i] == 'X') { base = 16; ++i; }
        if (i + 1 >= entity.size()) return entity;
        uint32_t code = 0;
        for (; i + 1 < entity.size(); ++i) {
            unsigned char c = entity[i];
            unsigned digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 99;
            if (digit >= base || code > (0x10ffff - digit) / base) return entity;
            code = code * base + digit;
        }
        append(result, code);
        return result;
    }
    const ENTITY *found = entity_lookup(entity.data(), entity.size());
    if (!found) return entity;
    append(result, found->codepoints[0]);
    if (found->codepoints[1]) append(result, found->codepoints[1]);
    return result;
}
}
#endif
