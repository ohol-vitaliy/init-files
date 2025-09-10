// proc_mem.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <dirent.h>
#include <unistd.h>
#include <pwd.h>
#include <cstring>

struct Entry {
    double rss_mb;
    std::string line;
};

bool is_number(const char* s) {
    for (; *s; ++s) {
        if (!isdigit(*s)) return false;
    }
    return true;
}

int main() {
    long pagesize = sysconf(_SC_PAGESIZE);
    if (pagesize <= 0) pagesize = 4096;

    std::cout <<
        "SIZE - data + stack\n"
        "RSS - actual RAM the process is using right now (includes shared libs)\n"
        "Private - RAM uniquely used by this process (without shared libs)\n\n";

    std::cout << std::setw(13) << "SIZE(MB)"
              << std::setw(13) << "RSS(MB)"
              << std::setw(13) << "Private(MB)"
              << " " << std::left << std::setw(8) << "USER"
              << " COMMAND\n";

    std::vector<Entry> entries;

    DIR* proc = opendir("/proc");
    if (!proc) return 1;

    struct dirent* ent;
    while ((ent = readdir(proc)) != nullptr) {
        if (!is_number(ent->d_name)) continue;

        std::string pid = ent->d_name;
        std::string base = "/proc/" + pid;

        // statm
        std::ifstream statm(base + "/statm");
        if (!statm) continue;

        long size, rss, shared, text, lib, data;
        statm >> size >> rss >> shared >> text >> lib >> data;
        if (!statm) continue;

        double size_mb = (double)data * pagesize / 1048576.0;
        double rss_mb  = (double)rss * pagesize / 1048576.0;
        double priv_mb = (double)(rss - shared) * pagesize / 1048576.0;

        // cmdline
        std::ifstream cmdf(base + "/cmdline", std::ios::binary);
        if (!cmdf) continue;

        std::string cmd((std::istreambuf_iterator<char>(cmdf)),
                         std::istreambuf_iterator<char>());

        for (char& c : cmd) {
            if (c == '\0') c = ' ';
        }

        if (cmd.empty()) continue;
        if (cmd.size() > 80) cmd = cmd.substr(0, 80);

        // uid -> user
        std::ifstream status(base + "/status");
        std::string line, user = "?";
        uid_t uid = -1;

        while (std::getline(status, line)) {
            if (line.rfind("Uid:", 0) == 0) {
                std::istringstream iss(line);
                std::string tmp;
                iss >> tmp >> uid;
                break;
            }
        }

        if (uid != (uid_t)-1) {
            struct passwd* pw = getpwuid(uid);
            if (pw) user = pw->pw_name;
            else user = std::to_string(uid);
        }

        std::ostringstream out;
        out << std::right << std::fixed << std::setprecision(2)
            << std::setw(13) << size_mb
            << std::setw(13) << rss_mb
            << std::setw(13) << priv_mb << " "
            << std::left << std::setw(8) << user << " "
            << cmd;

        entries.push_back({rss_mb, out.str()});
    }

    closedir(proc);

    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) {
                  return a.rss_mb > b.rss_mb;
              });

    for (const auto& e : entries) {
        std::cout << e.line << "\n";
    }

    // Kernel memory usage
    std::ifstream meminfo("/proc/meminfo");
    long total = 0;
    std::string line;

    while (std::getline(meminfo, line)) {
        if (line.find("KernelStack") != std::string::npos ||
            line.find("Slab") != std::string::npos ||
            line.find("PageTables") != std::string::npos ||
            line.find("VmallocUsed") != std::string::npos) {

            std::istringstream iss(line);
            std::string key;
            long value;
            iss >> key >> value;
            total += value;
        }
    }

    std::cout << "\nKernel RAM usage: "
              << std::fixed << std::setprecision(2)
              << (total / 1024.0) << " MB\n";

    return 0;
}
