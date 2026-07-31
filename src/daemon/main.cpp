#include "vader5/config.hpp"
#include "vader5/gamepad.hpp"
#include "vader5/keycodes.hpp"
#include "vader5/types.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <thread>
#include <vector>

#include <poll.h>
#include <unistd.h>

namespace {
std::atomic<bool> g_running{true};
std::atomic<bool> g_reload{false};
std::atomic<bool> g_calibrate{false};

void handle_signal(int signum) {
    if (signum == SIGHUP) {
        g_reload.store(true, std::memory_order_relaxed);
    } else if (signum == SIGUSR1) {
        g_calibrate.store(true, std::memory_order_relaxed);
    } else {
        g_running.store(false, std::memory_order_relaxed);
    }
}

constexpr auto RETRY_INTERVAL = std::chrono::seconds(2);

struct Args {
    std::string config_path;
    std::string device_name;
    std::string profile;
    std::string switch_profile;
    bool config_explicit = false;
    bool check_config = false;
    bool list_keys = false;
    bool list_buttons = false;
    bool list_profiles = false;
    bool calibrate_gyro = false;
};

auto parse_args(int argc, char** argv) -> Args {
    Args out;
    out.config_path = vader5::Config::default_path();
    const std::span args(argv, static_cast<size_t>(argc));
    for (size_t i = 1; i < args.size(); ++i) {
        if ((std::strcmp(args[i], "-c") == 0 || std::strcmp(args[i], "--config") == 0) &&
            i + 1 < args.size()) {
            out.config_path = args[++i];
            out.config_explicit = true;
        } else if ((std::strcmp(args[i], "-d") == 0 || std::strcmp(args[i], "--device") == 0) &&
                   i + 1 < args.size()) {
            out.device_name = args[++i];
        } else if (std::strcmp(args[i], "--check-config") == 0) {
            out.check_config = true;
        } else if (std::strcmp(args[i], "--list-keys") == 0) {
            out.list_keys = true;
        } else if (std::strcmp(args[i], "--list-buttons") == 0) {
            out.list_buttons = true;
        } else if (std::strcmp(args[i], "--list-profiles") == 0) {
            out.list_profiles = true;
        } else if (std::strcmp(args[i], "--profile") == 0 && i + 1 < args.size()) {
            out.profile = args[++i];
        } else if (std::strcmp(args[i], "--switch-profile") == 0 && i + 1 < args.size()) {
            out.switch_profile = args[++i];
        } else if (std::strcmp(args[i], "--calibrate-gyro") == 0) {
            out.calibrate_gyro = true;
        }
    }
    return out;
}

auto profile_dir(const Args& args) -> std::string {
    if (args.config_explicit) {
        return std::filesystem::path(args.config_path).parent_path().string();
    }
    return "/etc/vader5";
}

auto resolve_config_path(const std::string& base, const std::string& profile) -> std::string {
    namespace fs = std::filesystem;
    const fs::path dir = fs::path(base).parent_path();
    const fs::path profiles = dir / "profiles";
    if (!profile.empty()) {
        return (profiles / (profile + ".toml")).string();
    }
    std::ifstream active(dir / "active");
    std::string name;
    if (active && std::getline(active, name)) {
        const auto begin = name.find_first_not_of(" \t\r\n");
        const auto end = name.find_last_not_of(" \t\r\n");
        if (begin != std::string::npos) {
            name = name.substr(begin, end - begin + 1);
            std::error_code ec;
            const fs::path candidate = profiles / (name + ".toml");
            if (fs::exists(candidate, ec)) {
                return candidate.string();
            }
        }
    }
    return base;
}

void print_profiles(const std::string& dir) {
    namespace fs = std::filesystem;
    const fs::path profiles = fs::path(dir) / "profiles";
    std::error_code ec;
    std::vector<std::string> names;
    for (const auto& entry : fs::directory_iterator(profiles, ec)) {
        if (entry.path().extension() == ".toml") {
            names.push_back(entry.path().stem().string());
        }
    }
    std::ranges::sort(names);
    for (const auto& name : names) {
        std::cout << name << "\n";
    }
}

void signal_daemons(int sig) {
    namespace fs = std::filesystem;
    const int self = getpid();
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/proc", ec)) {
        const std::string fname = entry.path().filename().string();
        int pid = 0;
        bool numeric = !fname.empty();
        for (const char digit : fname) {
            if (digit < '0' || digit > '9') {
                numeric = false;
                break;
            }
            pid = (pid * 10) + (digit - '0');
        }
        if (!numeric || pid == self) {
            continue;
        }
        std::ifstream comm(entry.path() / "comm");
        std::string name;
        if (comm && std::getline(comm, name) && name == "vader5d") {
            (void)::kill(pid, sig);
        }
    }
}

auto run_switch_profile(const std::string& dir, const std::string& name) -> int {
    namespace fs = std::filesystem;
    const fs::path profile = fs::path(dir) / "profiles" / (name + ".toml");
    std::error_code ec;
    if (!fs::exists(profile, ec)) {
        std::cerr << "vader5d: no profile at " << profile.string() << "\n";
        return 1;
    }
    if (!vader5::Config::load(profile.string())) {
        std::cerr << "vader5d: profile '" << name << "' is invalid, not switching\n";
        return 1;
    }
    const fs::path active = fs::path(dir) / "active";
    std::ofstream out(active);
    if (!out) {
        std::cerr << "vader5d: cannot write " << active.string() << " (run with sudo?)\n";
        return 1;
    }
    out << name << "\n";
    out.close();
    signal_daemons(SIGHUP);
    std::cout << "vader5d: switched to profile '" << name << "'\n";
    return 0;
}

auto run_calibrate_gyro() -> int {
    signal_daemons(SIGUSR1);
    std::cout << "vader5d: keep the controller flat and still for ~2 seconds while it calibrates\n";
    return 0;
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

auto do_reload(vader5::Gamepad& gamepad, const std::string& base, const std::string& profile,
               vader5::Config& cfg) -> bool {
    const std::string path = resolve_config_path(base, profile);
    auto loaded = vader5::Config::load(path);
    if (!loaded) {
        std::cerr << "vader5d: reload failed, keeping current config\n";
        return false;
    }
    cfg = *loaded;
    if (!gamepad.reload(cfg)) {
        std::cout << "vader5d: reconnecting for " << path << "\n";
        return true;
    }
    std::cout << "vader5d: reloaded " << path << "\n";
    return false;
}

auto handle_cli_action(const Args& args) -> std::optional<int> {
    if (args.check_config) {
        return run_check_config(resolve_config_path(args.config_path, args.profile));
    }
    if (args.list_keys) {
        print_keys();
        return 0;
    }
    if (args.list_buttons) {
        print_buttons();
        return 0;
    }
    if (!args.switch_profile.empty()) {
        return run_switch_profile(profile_dir(args), args.switch_profile);
    }
    if (args.list_profiles) {
        print_profiles(profile_dir(args));
        return 0;
    }
    if (args.calibrate_gyro) {
        return run_calibrate_gyro();
    }
    return std::nullopt;
}
}

auto main(int argc, char** argv) -> int {
    const auto args = parse_args(argc, argv);
    if (auto rc = handle_cli_action(args)) {
        return *rc;
    }

    vader5::Config cfg;
    const std::string cfg_dir = profile_dir(args);
    const std::string config_path = resolve_config_path(args.config_path, args.profile);
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
    sigaction(SIGUSR1, &sa, nullptr);

    sigset_t empty_mask;
    sigemptyset(&empty_mask);

    sigset_t block_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGTERM);
    sigaddset(&block_mask, SIGINT);
    sigaddset(&block_mask, SIGHUP);
    sigaddset(&block_mask, SIGUSR1);

    while (g_running.load(std::memory_order_relaxed)) {
        auto gamepad = vader5::Gamepad::open(cfg, args.device_name, cfg_dir);
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
                do_reload(*gamepad, args.config_path, args.profile, cfg)) {
                break;
            }
            if (g_calibrate.exchange(false, std::memory_order_relaxed)) {
                gamepad->start_gyro_calibration();
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
