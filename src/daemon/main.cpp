#include "vader5/config.hpp"
#include "vader5/gamepad.hpp"
#include "vader5/keycodes.hpp"
#include "vader5/types.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <span>
#include <thread>

#include <poll.h>

namespace {
std::atomic<bool> g_running{true};
std::atomic<bool> g_reload{false};

void handle_signal(int signum) {
    if (signum == SIGHUP) {
        g_reload.store(true, std::memory_order_relaxed);
    } else {
        g_running.store(false, std::memory_order_relaxed);
    }
}

constexpr auto RETRY_INTERVAL = std::chrono::seconds(2);

struct Args {
    std::string config_path;
    std::string device_name;
    bool check_config = false;
    bool list_keys = false;
    bool list_buttons = false;
};

auto parse_args(int argc, char** argv) -> Args {
    Args out;
    out.config_path = vader5::Config::default_path();
    const std::span args(argv, static_cast<size_t>(argc));
    for (size_t i = 1; i < args.size(); ++i) {
        if ((std::strcmp(args[i], "-c") == 0 || std::strcmp(args[i], "--config") == 0) &&
            i + 1 < args.size()) {
            out.config_path = args[++i];
        } else if ((std::strcmp(args[i], "-d") == 0 || std::strcmp(args[i], "--device") == 0) &&
                   i + 1 < args.size()) {
            out.device_name = args[++i];
        } else if (std::strcmp(args[i], "--check-config") == 0) {
            out.check_config = true;
        } else if (std::strcmp(args[i], "--list-keys") == 0) {
            out.list_keys = true;
        } else if (std::strcmp(args[i], "--list-buttons") == 0) {
            out.list_buttons = true;
        }
    }
    return out;
}

auto run_check_config(const std::string& path) -> int {
    auto loaded = vader5::Config::load(path);
    if (!loaded) {
        std::cerr << "vader5d: config at " << path << " is invalid\n";
        return 1;
    }
    std::cout << "vader5d: config at " << path << " OK (" << loaded->button_remaps.size()
              << " base remaps, " << loaded->layers.size() << " layers)\n";
    return 0;
}

void print_keys() {
    for (const auto& name : vader5::keycode_names()) {
        std::cout << name << "\n";
    }
    for (const char* alias : {"mouse_left", "mouse_right", "mouse_middle", "mouse_side",
                              "mouse_extra", "mouse_forward", "mouse_back", "disabled"}) {
        std::cout << alias << "\n";
    }
    std::cout << "code:<N>\n";
    std::cout << "<key>+<key>+...\n";
}

void print_buttons() {
    for (const char* name : {"A", "B", "X", "Y", "LB", "RB", "LT", "RT", "M1", "M2", "M3", "M4",
                             "LM", "RM", "C", "Z", "START", "SELECT", "L3", "R3"}) {
        std::cout << name << "\n";
    }
}

auto do_reload(vader5::Gamepad& gamepad, const std::string& path, vader5::Config& cfg) -> bool {
    auto loaded = vader5::Config::load(path);
    if (!loaded) {
        std::cerr << "vader5d: reload failed, keeping current config\n";
        return false;
    }
    cfg = *loaded;
    if (!gamepad.reload(cfg)) {
        std::cout << "vader5d: config change needs device reinit, reconnecting\n";
        return true;
    }
    std::cout << "vader5d: config reloaded\n";
    return false;
}
}

auto main(int argc, char** argv) -> int {
    const auto args = parse_args(argc, argv);
    if (args.check_config) {
        return run_check_config(args.config_path);
    }
    if (args.list_keys) {
        print_keys();
        return 0;
    }
    if (args.list_buttons) {
        print_buttons();
        return 0;
    }

    vader5::Config cfg;
    const std::string& config_path = args.config_path;
    if (auto loaded = vader5::Config::load(config_path); loaded) {
        cfg = *loaded;
        std::cout << "vader5d: Loaded config from " << config_path << " ("
                  << cfg.button_remaps.size() << " base remaps, " << cfg.layers.size()
                  << " layers)\n";
    } else {
        std::cout << "vader5d: No usable config at " << config_path << ", using defaults\n";
    }

    std::cout << "vader5d: Waiting for Vader 5 Pro (VID:"
              << std::hex << std::setfill('0') << std::setw(4) << vader5::VENDOR_ID
              << " PID:" << std::setw(4) << vader5::PRODUCT_ID << std::dec << ")...\n";

    struct sigaction sa {};
    sa.sa_handler = handle_signal;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);

    sigset_t empty_mask;
    sigemptyset(&empty_mask);

    sigset_t block_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGTERM);
    sigaddset(&block_mask, SIGINT);
    sigaddset(&block_mask, SIGHUP);

    while (g_running.load(std::memory_order_relaxed)) {
        auto gamepad = vader5::Gamepad::open(cfg, args.device_name);
        if (!gamepad) {
            std::this_thread::sleep_for(RETRY_INTERVAL);
            continue;
        }

        std::cout << "vader5d: Device connected, running...\n";
        std::array<pollfd, 2> pfds{{
            {.fd = gamepad->fd(), .events = POLLIN, .revents = 0},
            {.fd = gamepad->ff_fd(), .events = POLLIN, .revents = 0},
        }};

        sigset_t old_mask;
        sigprocmask(SIG_BLOCK, &block_mask, &old_mask);

        while (g_running.load(std::memory_order_relaxed)) {
            if (g_reload.exchange(false, std::memory_order_relaxed) &&
                do_reload(*gamepad, config_path, cfg)) {
                break;
            }
            const int ret = ppoll(pfds.data(), pfds.size(), nullptr, &empty_mask);
            if (ret < 0) {
                const int err = errno;
                if (err == EINTR) {
                    continue;
                }
                std::cerr << "vader5d: poll error: " << std::strerror(err) << "\n";
                break;
            }

            if (ret > 0 && (pfds[0].revents & POLLIN) != 0) {
                auto result = gamepad->poll();
                if (!result) {
                    auto ec = result.error();
                    if (ec == std::errc::resource_unavailable_try_again) {
                        continue;
                    }
                    if (ec == std::errc::no_such_device || ec == std::errc::io_error) {
                        std::cout << "vader5d: Device disconnected\n";
                        break;
                    }
                    std::cerr << "vader5d: Read error: " << ec.message() << "\n";
                }
            }

            if (ret > 0 && (pfds[1].revents & POLLIN) != 0) {
                gamepad->poll_ff();
            }

            if ((pfds[0].revents & (POLLHUP | POLLERR)) != 0) {
                std::cout << "vader5d: Device disconnected\n";
                break;
            }
        }
        sigprocmask(SIG_SETMASK, &old_mask, nullptr);

        std::cout << "vader5d: Waiting for reconnection...\n";
    }

    std::cout << "vader5d: Shutting down\n";
    return 0;
}
