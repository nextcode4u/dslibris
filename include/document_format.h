// SPDX-License-Identifier: MIT
#pragma once
#include <string.h>
#include <strings.h>

enum format_t { FORMAT_UNDEF, FORMAT_XHTML, FORMAT_EPUB, FORMAT_TEXT, FORMAT_MARKDOWN, FORMAT_PDF, FORMAT_FB2, FORMAT_FB2_ZIP, FORMAT_RTF, FORMAT_DOCX, FORMAT_ODT };

static inline format_t document_format(const char *filename)
{
#ifdef DSL_DOCUMENTS
    size_t length = filename ? strlen(filename) : 0;
    if (length >= 8 && !strcasecmp(filename + length - 8, ".fb2.zip")) return FORMAT_FB2_ZIP;
#endif
    const char *ext = filename ? strrchr(filename, '.') : NULL;
    if (!ext) return FORMAT_UNDEF;
    if (!strcasecmp(ext, ".epub")) return FORMAT_EPUB;
#ifdef DSL_DOCUMENTS
    if (!strcasecmp(ext, ".txt")) return FORMAT_TEXT;
    if (!strcasecmp(ext, ".md") || !strcasecmp(ext, ".markdown")) return FORMAT_MARKDOWN;
    if (!strcasecmp(ext, ".pdf")) return FORMAT_PDF;
    if (!strcasecmp(ext, ".html") || !strcasecmp(ext, ".htm") || !strcasecmp(ext, ".xhtml")) return FORMAT_XHTML;
    if (!strcasecmp(ext, ".fb2")) return FORMAT_FB2;
    if (!strcasecmp(ext, ".fbz")) return FORMAT_FB2_ZIP;
    if (!strcasecmp(ext, ".rtf")) return FORMAT_RTF;
    if (!strcasecmp(ext, ".docx")) return FORMAT_DOCX;
    if (!strcasecmp(ext, ".odt")) return FORMAT_ODT;

#endif
    return FORMAT_UNDEF;
}
