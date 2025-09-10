#include <array>
#include <chrono>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <glob.h>
#include <string_view>
#include <syslog.h>
#include <thread>
#include <unistd.h>

constexpr const char* ACStatePath = "/sys/class/power_supply/AC/online";

enum class ConfigType
{
    File,
    Glob
};

struct Setting
{
    ConfigType type;
    std::string_view path;
    std::string_view ac_value;
    std::string_view batt_value;
};

constexpr std::array<Setting, 19> settings{{
    {ConfigType::File, "/proc/sys/vm/laptop_mode", "0", "5"},
    {ConfigType::File, "/proc/sys/vm/dirty_writeback_centisecs", "500", "6000"},
    {ConfigType::File, "/proc/sys/vm/dirty_background_ratio", "5", "10"},
    {ConfigType::File, "/proc/sys/vm/dirty_ratio", "50", "20"},
    {ConfigType::File, "/sys/module/pcie_aspm/parameters/policy", "performance", "powersave"},
    {ConfigType::File, "/sys/block/sda/device/power/control", "on", "auto"},
    {ConfigType::File, "/sys/module/block/parameters/events_dfl_poll_msecs", "0", "0"},
    {ConfigType::File, "/sys/module/snd_hda_intel/parameters/power_save", "0", "1"},
    {ConfigType::File, "/proc/sys/kernel/nmi_watchdog", "1", "0"},
    {ConfigType::Glob, "/sys/bus/usb/devices/usb*/power/wakeup", "disabled", "disabled"},
    {ConfigType::Glob, "/sys/bus/i2c/devices/i2c-*/device/power/control", "on", "auto"},
    {ConfigType::Glob,
     "/sys/class/scsi_host/host*/link_power_management_policy",
     "max_performance",
     "med_power_with_dipm"},
    {ConfigType::Glob, "/sys/bus/pci/devices/0000:00:17.0/ata*/power/control", "on", "auto"},
    {ConfigType::Glob, "/sys/bus/pci/devices/*/power/control", "on", "auto"},
    {ConfigType::Glob,
     "/sys/devices/system/cpu*/cpufreq/policy*/energy_performance_preference",
     "performance",
     "power"},
    {ConfigType::Glob,
     "/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor",
     "powersave",
     "powersave"},
    {ConfigType::File, "/sys/devices/system/cpu/intel_pstate/max_perf_pct", "100", "60"},
    {ConfigType::File, "/sys/devices/system/cpu/intel_pstate/min_perf_pct", "60", "20"},
    {ConfigType::File,
     "/sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference",
     "128",
     "255"},
}};

void log(const int priority, const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsyslog(priority, fmt, args);
    va_end(args);

    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    fputc('\n', stdout);
    va_end(args);
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

int main()
{
    require_sudo();
    openlog("power-mode", LOG_PID | LOG_CONS, LOG_USER);
    bool last_state = !get_ac_state();

    while (true)
    {
        const bool current_state = get_ac_state();

        if (current_state == last_state)
        {
            // log(LOG_INFO, "same state");
            std::this_thread::sleep_for(std::chrono::seconds(10));
            continue;
        }

        log(LOG_INFO, current_state ? "Running on AC..." : "Running on battery...");

        for (const Setting& setting : settings)
        {
            const std::string_view& value =
                current_state ? setting.ac_value : setting.batt_value;

            if (setting.type == ConfigType::File)
            {
                write_file(setting.path, value);
            }
            else if (setting.type == ConfigType::Glob)
            {
                write_glob(setting.path, value);
            }
        }

        sync();
        sync();
        sync();
        last_state = current_state;
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}
