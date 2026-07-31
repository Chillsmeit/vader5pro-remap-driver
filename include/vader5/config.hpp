#pragma once

#include "types.hpp"

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace vader5 {

constexpr float DEFAULT_GYRO_SENSITIVITY = 1.5F;
constexpr float DEFAULT_GYRO_SMOOTHING = 0.3F;
constexpr int DEFAULT_HOLD_TIMEOUT_MS = 200;

struct RemapTarget {
    enum Type { DISABLED, KEY, MOUSE_BUTTON, MOUSE_MOVE, GAMEPAD_BUTTON };
    Type type{KEY};
    int code{0};
    uint16_t btn_mask{0};
    uint8_t ext_mask{0};
    std::vector<int> combo{}; // NOLINT(readability-redundant-member-init)
};

struct GyroConfig {
    enum Mode { OFF, MOUSE, JOYSTICK };
    Mode mode{OFF};
    float sensitivity_x{DEFAULT_GYRO_SENSITIVITY};
    float sensitivity_y{DEFAULT_GYRO_SENSITIVITY};
    int deadzone{0};
    float smoothing{DEFAULT_GYRO_SMOOTHING};
    float curve{1.0F};
    bool invert_x{false};
    bool invert_y{false};
};

struct StickConfig {
    enum Mode { GAMEPAD, MOUSE, SCROLL };
    Mode mode{GAMEPAD};
    int deadzone{128};
    float sensitivity{1.0F};
    bool suppress_gamepad{false};
};

struct DpadConfig {
    enum Mode { GAMEPAD, ARROWS };
    Mode mode{GAMEPAD};
    bool suppress_gamepad{false};
};

struct LayerConfig {
    enum Activation { HOLD, TOGGLE };
    std::string name;
    std::string trigger;
    std::optional<RemapTarget> tap;
    int hold_timeout{DEFAULT_HOLD_TIMEOUT_MS};
    Activation activation{HOLD};

    std::optional<GyroConfig> gyro;
    std::optional<StickConfig> stick_left;
    std::optional<StickConfig> stick_right;
    std::optional<DpadConfig> dpad;
    std::unordered_map<std::string, RemapTarget> remap;
};

struct Config {
    bool emulate_elite{true};
    std::array<std::optional<int>, 8> ext_mappings{};
    std::unordered_map<std::string, RemapTarget> button_remaps;
    GyroConfig gyro;
    StickConfig left_stick;
    StickConfig right_stick;
    DpadConfig dpad;
    std::unordered_map<std::string, LayerConfig> layers;

    static auto load(const std::string& path) -> Result<Config>;
    static auto default_path() -> std::string;
};

auto parse_remap_target(std::string_view value) -> std::optional<RemapTarget>;

}
