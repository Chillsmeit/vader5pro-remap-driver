#pragma once

#include <linux/input-event-codes.h>

#include <optional>
#include <string_view>
#include <vector>

namespace vader5 {

auto keycode_from_name(std::string_view name) -> std::optional<int>;
auto keycode_names() -> std::vector<std::string_view>;

}