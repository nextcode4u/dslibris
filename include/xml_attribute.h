// SPDX-License-Identifier: MIT
#pragma once
#include <string>

// Preserve filenames and font names when writing double-quoted XML attributes.
inline std::string xml_attribute(const std::string &value)
{
    std::string result;
    for (char c : value) {
        switch (c) {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '"': result += "&quot;"; break;
        case '\'': result += "&apos;"; break;
        case '\n': result += "&#10;"; break;
        case '\r': result += "&#13;"; break;
        case '\t': result += "&#9;"; break;
        default: result += c;
        }
    }
    return result;
}
