// SPDX-License-Identifier: MIT
#pragma once
#include <string.h>
#include <strings.h>

enum format_t { FORMAT_UNDEF, FORMAT_XHTML, FORMAT_EPUB, FORMAT_TEXT, FORMAT_MARKDOWN, FORMAT_PDF };

static inline format_t document_format(const char *filename)
{
    const char *ext = filename ? strrchr(filename, '.') : NULL;
    if (!ext) return FORMAT_UNDEF;
    if (!strcasecmp(ext, ".epub")) return FORMAT_EPUB;
#ifdef DSL_DOCUMENTS
    if (!strcasecmp(ext, ".txt")) return FORMAT_TEXT;
    if (!strcasecmp(ext, ".md") || !strcasecmp(ext, ".markdown")) return FORMAT_MARKDOWN;
    if (!strcasecmp(ext, ".pdf")) return FORMAT_PDF;
#endif
    return FORMAT_UNDEF;
}
