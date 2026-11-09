// SPDX-License-Identifier: MIT
#pragma once
#include <string.h>
#include <strings.h>

enum format_t { FORMAT_UNDEF, FORMAT_XHTML, FORMAT_EPUB, FORMAT_TEXT, FORMAT_MARKDOWN, FORMAT_PDF, FORMAT_FB2, FORMAT_FB2_ZIP, FORMAT_RTF, FORMAT_DOCX, FORMAT_ODT, FORMAT_MOBI, FORMAT_OPF, FORMAT_OEB_ZIP, FORMAT_HTML_ZIP, FORMAT_TEXT_ZIP, FORMAT_PML, FORMAT_TCR, FORMAT_PML_ZIP };

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
    if (!strcasecmp(ext, ".pmlz")) return FORMAT_PML_ZIP;
    if (!strcasecmp(ext, ".pml")) return FORMAT_PML;
    if (!strcasecmp(ext, ".tcr")) return FORMAT_TCR;
    if (!strcasecmp(ext, ".opf") || !strcasecmp(ext, ".oeb")) return FORMAT_OPF;
    if (!strcasecmp(ext, ".oebzip")) return FORMAT_OEB_ZIP;
    if (!strcasecmp(ext, ".htmlz")) return FORMAT_HTML_ZIP;
    if (!strcasecmp(ext, ".txtz")) return FORMAT_TEXT_ZIP;
    if (!strcasecmp(ext, ".kepub")) return FORMAT_EPUB;
    if (!strcasecmp(ext, ".txt") || !strcasecmp(ext, ".text")) return FORMAT_TEXT;
    if (!strcasecmp(ext, ".md") || !strcasecmp(ext, ".markdown")) return FORMAT_MARKDOWN;
    if (!strcasecmp(ext, ".pdf")) return FORMAT_PDF;
    if (!strcasecmp(ext, ".html") || !strcasecmp(ext, ".htm") || !strcasecmp(ext, ".xhtml")) return FORMAT_XHTML;
    if (!strcasecmp(ext, ".fb2")) return FORMAT_FB2;
    if (!strcasecmp(ext, ".fbz")) return FORMAT_FB2_ZIP;
    if (!strcasecmp(ext, ".rtf")) return FORMAT_RTF;
    if (!strcasecmp(ext, ".docx")) return FORMAT_DOCX;
    if (!strcasecmp(ext, ".odt")) return FORMAT_ODT;
    if (!strcasecmp(ext, ".mobi") || !strcasecmp(ext, ".prc") || !strcasecmp(ext, ".pdb")
        || !strcasecmp(ext, ".azw") || !strcasecmp(ext, ".azw3")) return FORMAT_MOBI;

#endif
    return FORMAT_UNDEF;
}
