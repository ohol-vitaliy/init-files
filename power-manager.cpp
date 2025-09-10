#include <array>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <glob.h>
#include <linux/input.h>
#include <map>
#include <netinet/in.h>
#include <poll.h>
#include <regex>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <syslog.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

enum class PowerMode
{
    High,
    Mid,
    Low
};

constexpr const char* to_string(const PowerMode mode)
{
    switch (mode)
    {
    case PowerMode::High:
        return "high";
    case PowerMode::Mid:
        return "mid";
    case PowerMode::Low:
        return "low";
    }
    return "unknown";
}

enum class ConfigType
{
    File,
    Glob
};
struct Setting
{
    ConfigType type;
    std::string_view path;
    std::string_view high_value;
    std::string_view mid_value;
    std::string_view low_value;
};

constexpr int PORT = 12345;
constexpr int BUFFER_SIZE = 1024;
constexpr const char* ACStatePath = "/sys/class/power_supply/AC/online";
constexpr const char* InputDevicesPath = "/proc/bus/input/devices";
constexpr std::array<Setting, 20> settings{{
    {ConfigType::File, "/proc/sys/vm/laptop_mode", "0", "5", "5"},
    {ConfigType::File, "/proc/sys/vm/dirty_writeback_centisecs", "500", "6000", "6000"},
    {ConfigType::File, "/proc/sys/vm/dirty_background_ratio", "5", "10", "10"},
    {ConfigType::File, "/proc/sys/vm/dirty_ratio", "50", "20", "20"},
    {ConfigType::File,
     "/sys/module/pcie_aspm/parameters/policy",
     "performance",
     "powersave",
     "powersave"},
    {ConfigType::File, "/sys/block/sda/device/power/control", "on", "auto", "auto"},
    {ConfigType::File, "/sys/module/block/parameters/events_dfl_poll_msecs", "0", "0", "0"},
    {ConfigType::File, "/sys/module/snd_hda_intel/parameters/power_save", "0", "1", "1"},
    {ConfigType::File, "/proc/sys/kernel/nmi_watchdog", "1", "0", "0"},
    {ConfigType::Glob,
     "/sys/bus/usb/devices/usb*/power/wakeup",
     "disabled",
     "disabled",
     "disabled"},
    {ConfigType::Glob,
     "/sys/bus/i2c/devices/i2c-*/device/power/control",
     "on",
     "auto",
     "auto"},
    {ConfigType::Glob,
     "/sys/class/scsi_host/host*/link_power_management_policy",
     "max_performance",
     "med_power_with_dipm",
     "med_power_with_dipm"},
    {ConfigType::Glob,
     "/sys/bus/pci/devices/0000:00:17.0/ata*/power/control",
     "on",
     "auto",
     "auto"},
    {ConfigType::Glob, "/sys/bus/pci/devices/*/power/control", "on", "auto", "auto"},
    {ConfigType::Glob,
     "/sys/devices/system/cpu*/cpufreq/policy*/energy_performance_preference",
     "performance",
     "power",
     "power"},
    {ConfigType::Glob,
     "/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor",
     "performance",
     "ondemand",
     "powersave"},
    {ConfigType::File, "/sys/devices/system/cpu/intel_pstate/no_turbo", "0", "1", "1"},
    {ConfigType::File, "/sys/devices/system/cpu/intel_pstate/max_perf_pct", "100", "80", "50"},
    {ConfigType::File, "/sys/devices/system/cpu/intel_pstate/min_perf_pct", "80", "10", "10"},
    {ConfigType::File,
     "/sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference",
     "128",
     "255",
     "255"},
}};
// const std::vector<DeviceConfig> kDevices = {
//     {"Lid Switch", EV_SW, SW_LID, "Lid closed"},
//     {"Power Button", EV_KEY, KEY_POWER, "Power button"},
// };

struct input_event ev;
std::vector<pollfd> fds;
std::map<int, std::function<void(int)>> fd_map;
std::atomic<PowerMode> current_mode{PowerMode::Mid};

__attribute__((format(printf, 2, 3))) void log(const int priority, const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsyslog(priority, fmt, args);
    va_end(args);

    if (priority != LOG_DEBUG)
    {
        va_start(args, fmt);
        vfprintf(stdout, fmt, args);
        fputc('\n', stdout);
        va_end(args);
    }
}

static void write_file(std::string_view path, std::string_view value)
{
    std::ofstream f(path.data(), std::ios::out | std::ios::trunc);

    if (!f)
    {
        log(LOG_ERR, "Cant open file %s", path.data());
        return;
    }

    f << value;
    f.flush();

    if (!f)
    {
        log(LOG_ERR, "Write failed to file %s", path.data());
        return;
    }

    log(LOG_DEBUG, "%s -> %s", path.data(), value.data());
}

static void write_glob(std::string_view pattern, std::string_view value)
{
    glob_t g{};
    const int r = glob(pattern.data(), GLOB_NOSORT, nullptr, &g);

    if (r == 0)
    {
        for (size_t i = 0; i < g.gl_pathc; i++)
        {
            write_file(g.gl_pathv[i], value);
        }
    }
    else if (r != GLOB_NOMATCH)
    {
        log(LOG_ERR, "Glob error: %s", pattern.data());
    }

    globfree(&g);
}

void put_device_in_sleep()
{
    log(LOG_INFO, "Sleeping");
    sync();
    write_file("/sys/power/state", "mem\n");
}

bool pid1_is_busybox()
{
    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/1/exe", ec);

    if (ec)
    {
        return false;
    }

    auto name = exe.filename().string();
    return name == "busybox";
}

static void run_power_command(const char* busybox_cmd, const char* sbin_cmd)
{
    sync();
    system(pid1_is_busybox() ? busybox_cmd : sbin_cmd);
}

void reboot_device()
{
    log(LOG_INFO, "Rebooting");
    run_power_command("/bin/busybox reboot", "/sbin/reboot");
}

void powerdown_device()
{
    log(LOG_INFO, "Powering off");
    run_power_command("/bin/busybox poweroff", "/sbin/poweroff");
}

void apply_power_setting(const PowerMode mode)
{
    log(LOG_INFO, "Running on %s...", to_string(mode));
    current_mode = mode;

    for (const Setting& setting : settings)
    {
        std::string_view value = setting.low_value;

        switch (mode)
        {
        case PowerMode::High:
            value = setting.high_value;
            break;
        case PowerMode::Mid:
            value = setting.mid_value;
            break;
        case PowerMode::Low:
            value = setting.low_value;
            break;
        }

        if (setting.type == ConfigType::File)
        {
            write_file(setting.path, value);
        }
        else if (setting.type == ConfigType::Glob)
        {
            write_glob(setting.path, value);
        }
    }
}

std::string find_event_device(std::string_view name)
{
    std::ifstream infile(InputDevicesPath);

    if (!infile.is_open())
    {
        return "";
    }

    std::string line, found;
    bool match = false;

    while (std::getline(infile, line))
    {
        if (line.find(name) != std::string::npos)
        {
            match = true;
        }
        else if (match)
        {
            std::smatch m;

            if (std::regex_search(line, m, std::regex("event[0-9]+")))
            {
                return "/dev/input/" + m.str();
            }
        }
    }

    return "";
}

void process_event(const int fd, const int type, const int code, std::string_view action)
{
    if (ev.type != type || ev.code != code || ev.value != 1)
    {
        return;
    }

    log(LOG_INFO, "%s triggered", action.data());
    put_device_in_sleep();
}

void event_loop()
{
    while (true)
    {
        const int ret = poll(const_cast<pollfd*>(fds.data()), fds.size(), -1);

        if (ret <= 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        for (const pollfd& p : fds)
        {
            if ((p.revents & POLLIN) == 0)
            {
                continue;
            }

            if (read(p.fd, &ev, sizeof(ev)) <= 0)
            {
                continue;
            }

            auto it = fd_map.find(p.fd);

            if (it == fd_map.end())
            {
                continue;
            }

            std::function<void(int)>& callback = it->second;
            callback(p.fd);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int get_input_device_fd(std::string_view name)
{
    const std::string dev_path = find_event_device(name);

    if (dev_path.empty())
    {
        return -1;
    }

    const int fd = open(dev_path.c_str(), O_RDONLY);

    if (fd < 0)
    {
        log(LOG_ERR, "Failed to open %s", dev_path.c_str());
        return -1;
    }

    log(LOG_INFO, "Monitoring %s at %s. fd: %d", name.data(), dev_path.c_str(), fd);
    return fd;
}

std::string handle_command(std::string_view cmd)
{
    if (cmd == "sleep")
    {
        put_device_in_sleep();
        return "";
    }
    else if (cmd == "reboot" || cmd == "restart")
    {
        reboot_device();
        return "";
    }
    else if (cmd == "off" || cmd == "down")
    {
        powerdown_device();
        return "";
    }
    else if (cmd == "low")
    {
        apply_power_setting(PowerMode::Low);
        return "OK";
    }
    else if (cmd == "mid")
    {
        apply_power_setting(PowerMode::Mid);
        return "OK";
    }
    else if (cmd == "high")
    {
        apply_power_setting(PowerMode::High);
        return "OK";
    }
    else if (cmd == "status")
    {
        return to_string(current_mode);
    }

    std::string response = "Unknown command: ";
    response += cmd;
    log(LOG_ERR, "%s", response.c_str());
    return response;
}

void udp_listener()
{
    const int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0)
    {
        perror("socket");
        return;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(sockfd);
        return;
    }

    log(LOG_INFO, "Commands socket running on port %d", PORT);
    char buffer[BUFFER_SIZE];

    while (true)
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        const int n = recvfrom(
            sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&client_addr, &client_len);

        if (n <= 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        buffer[n] = '\0';
        const std::string response = handle_command(std::string(buffer));

        if (!response.empty())
        {
            sendto(
                sockfd,
                response.c_str(),
                response.length(),
                0,
                (struct sockaddr*)&client_addr,
                client_len);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    close(sockfd);
}

// void event_listener()
// {
//     {
//         const int lifFD = get_input_device_fd("Lid Switch");

//         if (lifFD > 0)
//         {
//             fds.push_back({lifFD, POLLIN, 0});
//             fd_map[lifFD] = [](const int fd)
//             { process_event(fd, EV_SW, SW_LID, "Lid closed"); };
//         }
//     }
//     {
//         const int pwrButtonFD = get_input_device_fd("Power Button");

//         if (pwrButtonFD > 0)
//         {
//             fds.push_back({pwrButtonFD, POLLIN, 0});
//             fd_map[pwrButtonFD] = [](const int fd)
//             { process_event(fd, EV_KEY, KEY_POWER, "Power button pressed"); };
//         }
//     }

//     if (fds.empty())
//     {
//         log(LOG_ERR, "No matching input devices found");
//         return;
//     }

//     event_loop();

//     for (const pollfd& p : fds)
//     {
//         close(p.fd);
//     }
// }

void require_sudo()
{
    if (geteuid() != 0)
    {
        log(LOG_ERR, "Requested operation requires superuser privilege");
        std::exit(1);
    }
}

bool get_ac_state()
{
    std::ifstream ifs(ACStatePath);
    int state = 0;

    if (!(ifs >> state))
    {
        return false;
    }

    return state == 1;
}

// int monitor_ac_state()
// {
//     bool last_state = !get_ac_state();

//     while (true)
//     {
//         const bool current_state = get_ac_state();

//         if (current_state != last_state)
//         {
//             // TODO: add inhibit-charge setting when in work days and work hours
//             // TODO: add notification with beeping for low battery charge??
//             apply_power_setting(current_state);
//             last_state = current_state;
//         }

//         std::this_thread::sleep_for(std::chrono::seconds(10));
//     }
// }

int main()
{
    openlog("goto-sleep", LOG_PID | LOG_CONS, LOG_USER);
    require_sudo();
    apply_power_setting(PowerMode::Mid);

    // std::thread udp_listener_thread(udp_listener);
    // std::thread event_listener_thread(event_listener);
    // monitor_ac_state();

    // udp_listener_thread.join();
    // event_listener_thread.join();

    udp_listener();

    closelog();
    return 0;
}
