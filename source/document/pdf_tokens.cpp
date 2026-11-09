// SPDX-License-Identifier: MIT
#ifdef DSL_DOCUMENTS
#include "pdf_tokens.h"
#include <ctype.h>

namespace document {
static bool whitespace(int c) { return c == 0 || c == 9 || c == 10 || c == 12 || c == 13 || c == 32; }
static bool delimiter(int c) { return c < 0 || whitespace(c) || c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' || c == '/' || c == '%'; }
static int hex(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
int PdfTokens::get()
{
    if (lookahead != -2) { int c = lookahead; lookahead = -2; return c; }
    if (position == bytes.size()) return -1;
    if (!remaining) { failed = true; return -1; }
    --remaining;
    return (unsigned char)bytes[position++];
}
int PdfTokens::peek() { if (lookahead == -2) lookahead = get(); return lookahead; }
bool PdfTokens::add(std::string &value, int c)
{
    if (value.size() >= 65536) { failed = true; return false; }
    value += (char)c;
    return true;
}
bool PdfTokens::next(PdfToken &token)
{
    token.value.clear();
    int c;
    while ((c = get()) >= 0) {
        if (whitespace(c)) continue;
        if (c == '%') { while ((c = get()) >= 0 && c != '\r' && c != '\n') {} continue; }
        break;
    }
    if (c < 0 || failed) return false;
    if (c == '(') {
        token.kind = PdfToken::String;
        unsigned depth = 1;
        while ((c = get()) >= 0) {
            if (c == '\\') {
                c = get();
                if (c < 0) break;
                if (c == '\r') { if (peek() == '\n') get(); continue; }
                if (c == '\n') continue;
                if (c >= '0' && c <= '7') {
                    int number = c - '0';
                    for (int i = 0; i < 2 && peek() >= '0' && peek() <= '7'; ++i) number = number * 8 + get() - '0';
                    c = number & 255;
                } else {
                    switch (c) { case 'n': c = '\n'; break; case 'r': c = '\r'; break;
                    case 't': c = '\t'; break; case 'b': c = '\b'; break; case 'f': c = '\f'; break; default: break; }
                }
            } else if (c == '(') {
                if (++depth > 32) { failed = true; return false; }
            } else if (c == ')' && --depth == 0) return true;
            if (!add(token.value, c)) return false;
        }
        failed = true;
        return false;
    }
    if (c == '<' && peek() != '<') {
        token.kind = PdfToken::String;
        int high = -1;
        while ((c = get()) >= 0) {
            if (c == '>') {
                if (high >= 0 && !add(token.value, high << 4)) return false;
                return true;
            }
            if (whitespace(c)) continue;
            int value = hex(c);
            if (value < 0) { failed = true; return false; }
            if (high < 0) high = value;
            else { if (!add(token.value, high * 16 + value)) return false; high = -1; }
        }
        failed = true;
        return false;
    }
    if (c == '[' || c == ']' || c == '<' || c == '>') {
        token.kind = PdfToken::Punctuation;
        token.value += (char)c;
        if ((c == '<' || c == '>') && peek() == c) token.value += (char)get();
        return true;
    }
    token.kind = c == '/' ? PdfToken::Name : PdfToken::Word;
    if (c != '/' && !add(token.value, c)) return false;
    while (!delimiter(peek())) {
        c = get();
        if (c == '#' && token.kind == PdfToken::Name) {
            int high = hex(get()), low = hex(get());
            if (high < 0 || low < 0) { failed = true; return false; }
            c = high * 16 + low;
        }
        if (!add(token.value, c)) return false;
    }
    return true;
}
}
#endif
