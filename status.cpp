#include <cstring>
#include <ctime>
#include <dirent.h>
#include <linux/nl80211.h>
#include <linux/wireless.h>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <unistd.h>

struct BatteryInfo
{
    bool charging = false;
    int capacity = -1;
    double usage;
    char name[16];
};

std::string combine_path(std::string_view path, std::string_view filename)
{
    std::string tmp = path.data();
    tmp += "/";
    tmp += filename;
    return tmp;
}

long read_number_file(std::string_view path)
{
    FILE* f = fopen(path.data(), "r");

    if (!f)
    {
        return -1;
    }

    long val = -1;

    if (fscanf(f, "%ld", &val) != 1)
    {
        val = -1;
    }

    fclose(f);
    return val;
};

void print_loadavg()
{
    FILE* f = fopen("/proc/loadavg", "r");

    if (!f)
    {
        return;
    }

    double l1, l5, l15;
    char procs[32];
    fscanf(f, "%lf %lf %lf %31s", &l1, &l5, &l15, procs);
    fclose(f);
    printf("%0.2f %0.2f %0.2f %s", l1, l5, l15, procs);
}
// void print_loadavg()
// {
//     double l[3];

//     if (getloadavg(l,3) == 3)
//         printf("%0.2f %0.2f %0.2f", l[0], l[1], l[2]);
// }

void print_mem()
{
    FILE* f = fopen("/proc/meminfo", "r");

    if (!f)
    {
        return;
    }

    int found = 0;
    long total = 0, free_ = 0, buffers = 0, cached = 0, sreclaim = 0, shmem = 0;
    long value;
    char key[32];
    char unit[8];
    while (fscanf(f, "%31s %ld %7s", key, &value, unit) == 3)
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

    printf("RAM: %0.2f GB/%0.2f GB", used / 1024.0 / 1024.0, avail / 1024.0 / 1024.0);
}

void print_brightness()
{
    DIR* d = opendir("/sys/class/backlight");

    if (!d)
    {
        return;
    }

    struct dirent* e = readdir(d);

    // skip hidden
    while (e && e->d_name[0] == '.')
    {
        e = readdir(d);
    }

    if (!e)
    {
        closedir(d);
        return;
    }

    char path[256];
    snprintf(path, sizeof(path), "/sys/class/backlight/%s/max_brightness", e->d_name);
    const long max = read_number_file(path);

    if (max <= 0)
    {
        closedir(d);
        return;
    }

    snprintf(path, sizeof(path), "/sys/class/backlight/%s/brightness", e->d_name);
    const long cur = read_number_file(path);
    closedir(d);
    const int percent = (cur * 100) / max;
    printf("Bright: %d%%", percent);
}

void print_datetime()
{
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%F %R", &tm);
    printf("%s", buf);
}

std::string getSSID(const std::string& iface)
{
    const int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0)
    {
        return "";
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
        return "";
    }

    close(sock);
    return std::string(ssid, req.u.essid.length);
}

std::string detect_wifi_iface()
{
    DIR* d = opendir("/sys/class/net");

    if (!d)
    {
        return {};
    }

    struct dirent* e;

    while ((e = readdir(d)))
    {
        const char* n = e->d_name;

        if (!strncmp(n, "wlan", 4) || !strncmp(n, "wlp", 3) || !strncmp(n, "wifi", 4))
        {
            std::string r = n;
            closedir(d);
            return r;
        }
    }

    closedir(d);
    return {};
}

int get_capacity(std::string_view path)
{
    const long val = read_number_file(combine_path(path, "capacity"));

    if (val > 0)
    {
        return val;
    }

    const long energy_full = read_number_file(combine_path(path, "energy_full"));

    if (energy_full > 0)
    {
        const long energy_now = read_number_file(combine_path(path, "energy_now"));
        return static_cast<int>(energy_now * 100 / energy_full);
    }

    const long charge_full = read_number_file(combine_path(path, "charge_full"));

    if (charge_full > 0)
    {
        const long charge_now = read_number_file(combine_path(path, "charge_now"));
        return static_cast<int>(charge_now * 100 / charge_full);
    }

    return -1; // unknown
}

int get_temp()
{
    DIR* d = opendir("/sys/class/thermal");

    if (!d)
    {
        return -1;
    }

    struct dirent* e;

    while ((e = readdir(d)))
    {
        if (strncmp(e->d_name, "thermal_zone", 12) != 0)
        {
            continue;
        }

        char path[256];
        snprintf(path, sizeof(path), "/sys/class/thermal/%s/type", e->d_name);
        FILE* f = fopen(path, "r");

        if (!f)
        {
            continue;
        }

        char buf[32];
        fgets(buf, sizeof(buf), f);
        fclose(f);

        if (strstr(buf, "cpu") || strstr(buf, "x86_pkg_temp") || strstr(buf, "package"))
        {
            snprintf(path, sizeof(path), "/sys/class/thermal/%s/temp", e->d_name);
            const long temp = read_number_file(path);
            closedir(d);
            return temp / 1000;
        }
    }

    closedir(d);

    const long temp = read_number_file("/sys/class/thermal/thermal_zone0/temp");
    return temp / 1000;
}

BatteryInfo get_battery(std::string_view path, std::string_view batt_name)
{
    const auto full_path = combine_path(path, batt_name);

    BatteryInfo bat;
    {
        if (strncmp(batt_name.data(), "BAT", 3) == 0)
        {
            snprintf(bat.name, sizeof(bat.name), "B%s", batt_name.data() + 3);
        }
        else
        {
            snprintf(bat.name, sizeof(bat.name), "%s", batt_name.data());
        }
    }
    {
        const auto status_path = combine_path(full_path, "status");
        FILE* f = fopen(status_path.c_str(), "r");
        char buf[32];

        if (f && fgets(buf, sizeof(buf), f))
        {
            bat.charging = strncmp(buf, "Charging", 8) == 0;
        }

        if (f)
        {
            fclose(f);
        }
    }
    bat.capacity = get_capacity(full_path);

    const long power = read_number_file(combine_path(full_path, "power_now"));

    if (power > 0)
    {
        bat.usage = (power / 1e6);
    }
    else
    {
        const long cur = read_number_file(combine_path(full_path, "current_now"));
        const long volt = read_number_file(combine_path(full_path, "voltage_now"));
        const double p = (cur * volt) / 1e12;
        bat.usage = p;
    }

    return bat;
}

void print_battery_infos()
{
    DIR* d = opendir("/sys/class/power_supply");

    if (!d)
    {
        return;
    }

    bool charging = false;
    struct dirent* e;

    while ((e = readdir(d)))
    {
        if (e->d_name[0] == '.')
        {
            continue;
        }

        char path[256];
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/type", e->d_name);
        FILE* f = fopen(path, "r");

        if (!f)
        {
            continue;
        }

        char buf[32];
        fgets(buf, sizeof(buf), f);
        fclose(f);

        if (strncmp(buf, "Battery", 7) != 0)
        {
            continue;
        }

        BatteryInfo bat = get_battery("/sys/class/power_supply/", e->d_name);

        if (bat.charging)
        {
            charging = true;
        }

        printf("%s: %d%% (%0.2f W) ", bat.name, bat.capacity, bat.usage);
    }

    closedir(d);

    if (charging)
    {
        printf("CHRG ");
    }

    printf("(%d°C)", get_temp());
}

int main()
{
    printf("| ");
    const std::string iface = detect_wifi_iface();

    if (!iface.empty())
    {
        const std::string ssid = getSSID(iface);

        if (!ssid.empty())
        {
            printf("%s", ssid.c_str());
        }
        else
        {
            printf("NC");
        }
    }

    printf(" | ");
    print_loadavg();
    printf(" | ");
    print_mem();
    printf(" | ");
    print_datetime();
    printf(" |;");

    printf("| ");
    print_brightness();
    printf(" |;");

    printf("| ");
    print_battery_infos();
    printf(" |");
}
