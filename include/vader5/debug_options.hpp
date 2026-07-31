#pragma once

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

namespace vader5 {

struct DebugOptions {
    int input_iface{-1};
    int config_iface{1};

    static auto parse(int argc, const char* const* argv) -> DebugOptions {
        DebugOptions opts;
        const std::span<const char* const> args(argv, static_cast<size_t>(argc));
        for (size_t i = 1; i < args.size(); ++i) {
            std::string arg(args[i]);
            try {
                if (arg == "--input" && i + 1 < args.size()) {
                    opts.input_iface = std::stoi(args[++i]);
                } else if (arg == "--config" && i + 1 < args.size()) {
                    opts.config_iface = std::stoi(args[++i]);
                }
            } catch (const std::exception&) {
                std::cerr << "Invalid value for " << arg << "\n";
                std::exit(1);
            }
        }
        return opts;
    }
};

}
