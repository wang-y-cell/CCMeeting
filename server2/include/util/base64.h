#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace util {

std::optional<std::vector<std::uint8_t>> base64_decode(const std::string& encoded);

}  // namespace util
