// SPDX-License-Identifier: MIT
#include "xml_attribute.h"
#include <expat.h>
#include <cassert>
#include <cstring>
#include <string>

static void attribute(void *data, const char *, const char **attributes)
{
    assert(attributes[0] && !strcmp(attributes[0], "file"));
    *(std::string*)data = attributes[1];
}
int main()
{
    for (const char *name : {"Notes & letters.epub", "Café's \"book\".fb2", "A<B>C.txt", "line\nbreak\tname\r.md"}) {
        std::string parsed;
        std::string xml = "<book file=\"" + xml_attribute(name) + "\"/>";
        XML_Parser parser = XML_ParserCreate(nullptr);
        assert(parser);
        XML_SetUserData(parser, &parsed);
        XML_SetStartElementHandler(parser, attribute);
        assert(XML_Parse(parser, xml.data(), xml.size(), true) == XML_STATUS_OK);
        assert(parsed == name);
        XML_ParserFree(parser);
    }
}
