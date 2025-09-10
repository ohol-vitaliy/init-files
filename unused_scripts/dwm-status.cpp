#include <X11/Xlib.h>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <functional>
#include <glob.h>
#include <linux/wireless.h>
#include <memory>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <tuple>
#include <unistd.h>
#include <vector>

struct BatteryPaths
{
    std::string name;
    std::string status;
    std::function<const int()> get_capacity;
    std::function<const float()> get_usage;
};

const char load_path[] = "/proc/loadavg";
const char mem_path[] = "/proc/meminfo";
const char net_path[] = "/sys/class/net";
const char thermal_path[] = "/sys/class/thermal";

static std::string iface;
static std::string value_bright_path;
static std::string max_bright_path;
static std::string temp_path;
static Display* dpy;
static std::vector<std::string> freq_paths;
static std::vector<BatteryPaths> batteries;

// =====================================================================
// Helpers
// =====================================================================

std::string c_vsprintf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    va_list args_copy;
    va_copy(args_copy, args);

    const int size = vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);

    if (size < 0)
    {
        va_end(args);
        return {};
    }

    std::string result(size, '\0');
    vsnprintf(result.data(), result.size() + 1, fmt, args);

    va_end(args);
    return result;
}

std::string combine_path(const std::string& path, const std::string& filename)
{
    return path + "/" + filename;
}

bool exists(const std::string& path)
{
    return access(path.c_str(), F_OK) == 0;
}

long read_number_file(const std::string& path)
{
    std::unique_ptr<FILE, decltype(&fclose)> f(fopen(path.c_str(), "r"), fclose);

    if (!f)
    {
        return -1;
    }

    char buf[32];
    const size_t n = fread(buf, 1, sizeof(buf) - 1, f.get());

    if (n == 0)
    {
        return -1;
    }

    buf[n] = '\0';
    return strtol(buf, nullptr, 10);
};

std::string execscript(const std::string& cmd)
{
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe)
    {
        return {};
    }

    std::array<char, 2048> buffer;
    std::string result;

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }

    return result;
}

// =====================================================================
// Load
// =====================================================================

std::string print_loadavg()
{
    std::unique_ptr<FILE, decltype(&fclose)> f(fopen(load_path, "r"), fclose);

    if (!f)
    {
        return {};
    }

    double l1, l5, l15;
    char procs[32];
    fscanf(f.get(), "%lf %lf %lf %31s", &l1, &l5, &l15, procs);
    return c_vsprintf("%0.2f %0.2f %0.2f %s", l1, l5, l15, procs);
    //     double l[3];
    //     if (getloadavg(l,3) == 3)
    //         printf("%0.2f %0.2f %0.2f", l[0], l[1], l[2]);
}

// =====================================================================
// CPU freq
// =====================================================================

void get_freq_paths()
{
    glob_t glob_result{};
    freq_paths.clear();

    if (glob("/sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq", 0, nullptr, &glob_result)
        == 0)
    {

        for (size_t i = 0; i < glob_result.gl_pathc; ++i)
        {
            freq_paths.push_back(glob_result.gl_pathv[i]);
        }
    }

    globfree(&glob_result);
}

double print_cpu_freq()
{
    if (freq_paths.empty())
    {
        return 0.0;
    }

    double sum = 0.0;

    for (const auto& path : freq_paths)
    {
        sum += read_number_file(path);
    }

    const double avg = sum / freq_paths.size();
    return avg / 1e6;
}

// =====================================================================
// RAM
// =====================================================================

std::string print_mem()
{
    std::unique_ptr<FILE, decltype(&fclose)> f(fopen(mem_path, "r"), fclose);

    if (!f)
    {
        return {};
    }

    int found = 0;
    long total = 0, free_ = 0, buffers = 0, cached = 0, sreclaim = 0, shmem = 0;
    long value;
    char key[32];
    char unit[8];
    while (fscanf(f.get(), "%31s %ld %7s", key, &value, unit) == 3)
    {
        switch (key[0])
        {
        case 'M':
            if (strncmp(key, "MemTotal:", 9) == 0)
            {
                total = value;
                ++found;
            }
            else if (strncmp(key, "MemFree:", 8) == 0)
            {
                free_ = value;
                ++found;
            }

            break;
        case 'B':
            if (strncmp(key, "Buffers:", 8) == 0)
            {
                buffers = value;
                ++found;
            }

            break;
        case 'C':
            if (strncmp(key, "Cached:", 7) == 0)
            {
                cached = value;
                ++found;
            }

            break;
        case 'S':
            if (strncmp(key, "SReclaimable:", 13) == 0)
            {
                sreclaim = value;
                ++found;
            }
            else if (strncmp(key, "Shmem:", 6) == 0)
            {
                shmem = value;
                ++found;
            }

            break;
        }

        if (found == 6)
        {
            break;
        }
    }

    const long avail = free_ + buffers + cached + sreclaim - shmem;
    const long used = total - avail;
    return c_vsprintf("%0.2f GB/%0.2f GB", used / 1024.0 / 1024.0, avail / 1024.0 / 1024.0);
}

// =====================================================================
// Brighness
// =====================================================================

std::string get_brightness_dir()
{
    std::unique_ptr<DIR, decltype(&closedir)> d(opendir("/sys/class/backlight"), closedir);

    if (!d)
    {
        return {};
    }

    struct dirent* e = readdir(d.get());

    // skip hidden
    while (e && e->d_name[0] == '.')
    {
        e = readdir(d.get());
    }

    if (!e)
    {
        return {};
    }

    return c_vsprintf("/sys/class/backlight/%s", e->d_name);
}

int print_brightness()
{
    if (max_bright_path.empty() || value_bright_path.empty())
    {
        return {};
    }

    const long max = read_number_file(max_bright_path);

    if (max <= 0)
    {
        return {};
    }

    const long cur = read_number_file(value_bright_path);
    const int percent = (cur * 100) / max;
    return percent;
}

// =====================================================================
// DateTime
// =====================================================================

std::string print_datetime()
{
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%a %F %R", &tm);
    return buf;
}

// =====================================================================
// Wi-Fi
// =====================================================================

std::string getSSID(const std::string& iface)
{
    const int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0)
    {
        return {};
    }

    struct iwreq req;
    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, iface.c_str(), IFNAMSIZ);

    char ssid[IW_ESSID_MAX_SIZE + 1] = {0};
    req.u.essid.pointer = ssid;
    req.u.essid.length = IW_ESSID_MAX_SIZE + 1;
    req.u.essid.flags = 0;

    if (ioctl(sock, SIOCGIWESSID, &req) == -1)
    {
        close(sock);
        return {};
    }

    close(sock);
    return std::string(ssid, req.u.essid.length);
}

std::string detect_wifi_iface()
{
    std::unique_ptr<DIR, decltype(&closedir)> d(opendir(net_path), closedir);

    if (!d)
    {
        return {};
    }

    struct dirent* e;

    while ((e = readdir(d.get())))
    {
        const char* n = e->d_name;

        if (!strncmp(n, "wlan", 4) || !strncmp(n, "wlp", 3) || !strncmp(n, "wifi", 4))
        {
            return n;
        }
    }

    return {};
}

// =====================================================================
// Temperature
// =====================================================================

void get_temp_path()
{
    std::unique_ptr<DIR, decltype(&closedir)> d(opendir(thermal_path), closedir);

    if (!d)
    {
        return;
    }

    struct dirent* e;

    while ((e = readdir(d.get())))
    {
        if (strncmp(e->d_name, "thermal_zone", 12) != 0)
        {
            continue;
        }

        char path[256];
        snprintf(path, sizeof(path), "/sys/class/thermal/%s/type", e->d_name);
        std::unique_ptr<FILE, decltype(&fclose)> f(fopen(path, "r"), fclose);

        if (!f)
        {
            continue;
        }

        char buf[32];
        fgets(buf, sizeof(buf), f.get());

        if (strstr(buf, "cpu") || strstr(buf, "x86_pkg_temp") || strstr(buf, "package"))
        {
            temp_path = c_vsprintf("/sys/class/thermal/%s/temp", e->d_name);
            return;
        }
    }
}

int get_temp()
{
    if (!temp_path.empty())
    {
        const long temp = read_number_file(temp_path);
        return temp / 1000;
    }

    const long temp = read_number_file("/sys/class/thermal/thermal_zone0/temp");
    return temp / 1000;
}

// =====================================================================
// Battery
// =====================================================================

void detect_batteries()
{
    std::unique_ptr<DIR, decltype(&closedir)> d(opendir("/sys/class/power_supply"), closedir);

    if (!d)
    {
        return;
    }

    struct dirent* e;
    batteries.clear();

    while ((e = readdir(d.get())))
    {
        if (e->d_name[0] == '.')
        {
            continue;
        }

        char type_path[256];
        snprintf(type_path, sizeof(type_path), "/sys/class/power_supply/%s/type", e->d_name);

        std::unique_ptr<FILE, decltype(&fclose)> f(fopen(type_path, "r"), fclose);

        if (!f)
        {
            continue;
        }

        char buf[32];
        fgets(buf, sizeof(buf), f.get());

        if (strncmp(buf, "Battery", 7) != 0)
        {
            continue;
        }

        BatteryPaths b;
        std::string base = c_vsprintf("/sys/class/power_supply/%s", e->d_name);
        b.status = combine_path(base, "status");

        std::string capacity = combine_path(base, "capacity");
        std::string energy_now = combine_path(base, "energy_now");
        std::string energy_full = combine_path(base, "energy_full");

        if (exists(capacity))
        {
            b.get_capacity = [capacity]() -> long { return read_number_file(capacity); };
        }
        else if (exists(energy_now))
        {
            b.get_capacity = [energy_full, energy_now]() -> long
            {
                const long full = read_number_file(energy_full);

                if (full > 0)
                {
                    const long now = read_number_file(energy_now);
                    return now * 100 / full;
                }

                return 0;
            };
        }
        else
        {
            std::string charge_now = combine_path(base, "charge_now");
            std::string charge_full = combine_path(base, "charge_full");

            b.get_capacity = [charge_full, charge_now]() -> long
            {
                const long full = read_number_file(charge_full);

                if (full > 0)
                {
                    const long now = read_number_file(charge_now);
                    return now * 100 / full;
                }

                return 0;
            };
        }

        std::string power_now = combine_path(base, "power_now");

        if (exists(power_now))
        {
            b.get_usage = [power_now]() -> float
            {
                const long power = read_number_file(power_now);

                if (power > 0)
                {
                    return power / 1e6f;
                }

                return 0;
            };
        }
        else
        {
            std::string current_now = combine_path(base, "current_now");
            std::string voltage_now = combine_path(base, "voltage_now");

            b.get_usage = [current_now, voltage_now]() -> float
            {
                const long cur = read_number_file(current_now);
                const long volt = read_number_file(voltage_now);
                return (cur * volt) / 1e12f;
            };
        }

        if (!strncmp(e->d_name, "BAT", 3))
        {
            b.name = c_vsprintf("B%s", e->d_name + 3);
        }
        else
        {
            b.name = std::string(e->d_name);
        }

        batteries.push_back(std::move(b));
    }
}

std::tuple<std::string, bool> read_battery(const BatteryPaths& b)
{
    bool charging = false;
    std::unique_ptr<FILE, decltype(&fclose)> f(fopen(b.status.c_str(), "r"), fclose);

    if (f)
    {
        char buf[32];

        if (fgets(buf, sizeof(buf), f.get()))
        {
            charging = !strncmp(buf, "Charging", 8);
        }
    }

    const int capacity = b.get_capacity();
    const float usage = b.get_usage();
    const std::string s = c_vsprintf("%s: %d%% (%0.2f W) ", b.name.c_str(), capacity, usage);
    return std::make_tuple(s, charging);
}

std::string print_battery_infos()
{
    std::string ret;
    bool charging = false;

    for (BatteryPaths& b : batteries)
    {
        const auto [batt_data, bat_ch] = read_battery(b);

        if (bat_ch)
        {
            charging = true;
        }

        ret += batt_data;
    }

    if (charging)
    {
        ret += "CHRG ";
    }

    ret += c_vsprintf("(%d°C)", get_temp());
    return ret;
}

// =====================================================================
// Loop
// =====================================================================

int main()
{
    if (!(dpy = XOpenDisplay(NULL)))
    {
        fprintf(stderr, "dwmstatus: cannot open display.\n");
        return 1;
    }

    int bright = 0;
    double cpu_freq = 0.0;
    std::string load;
    std::string mem;
    std::string date;
    std::string batt;
    std::string kbmap;
    std::string wifi;
    std::string ssid;
    std::string status;

    iface = detect_wifi_iface();
    {
        std::string bright_dir = get_brightness_dir();
        max_bright_path = combine_path(bright_dir, "max_brightness");
        value_bright_path = combine_path(bright_dir, "brightness");
    }
    get_temp_path();
    detect_batteries();
    get_freq_paths();
    long counter = 0;

    while (1)
    {
        ++counter;

        if (counter >= 10)
        {
            counter = 0;
            wifi = "NC";

            if (!iface.empty())
            {
                ssid = getSSID(iface);

                if (!ssid.empty())
                {
                    wifi = ssid;
                }
            }

            kbmap = execscript("setxkbmap -query | awk '/^layout/ {printf \"%s\", $2}'");
            cpu_freq = print_cpu_freq();
        }

        load = print_loadavg();
        mem = print_mem();
        date = print_datetime();
        bright = print_brightness();
        batt = print_battery_infos();
        status = c_vsprintf(
            "| %s | %s | RAM: %s | %s |;| Bright: %d%% |;| %0.2f GHz | %s | %s |",
            wifi.c_str(),
            kbmap.c_str(),
            mem.c_str(),
            date.c_str(),
            bright,
            cpu_freq,
            load.c_str(),
            batt.c_str());
        XStoreName(dpy, DefaultRootWindow(dpy), status.c_str());
        XSync(dpy, False);
        sleep(1);
    }

    XCloseDisplay(dpy);
    return 0;
}
