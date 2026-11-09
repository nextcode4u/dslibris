// SPDX-License-Identifier: MIT
#pragma once
#include <string>
namespace document {
// Unknown/malformed entities are retained as literal text.
std::string decode_entity(const std::string &entity);
}
