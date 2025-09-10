#!/usr/bin/env python3
import os

PAGESIZE = os.sysconf("SC_PAGE_SIZE") if hasattr(os, "sysconf") else 4096

print("""SIZE - data + stack
RSS - actual RAM the process is using right now (includes shared libs)
Private - RAM uniquely used by this process (without shared libs)
""")
print(
    f"{'SIZE(MB)':>13} {'RSS(MB)':>13} {'Private(MB)':>13} {'USER':<8} COMMAND"
)

entries = []

for pid in os.listdir("/proc"):
    if not pid.isdigit():
        continue

    statm_path = f"/proc/{pid}/statm"

    try:
        with open(statm_path) as f:
            parts = f.read().split()

            if len(parts) < 6:
                continue

            size = int(parts[0])
            rss = int(parts[1])
            shared = int(parts[2])
            data = int(parts[5])

            size_mb = (data * PAGESIZE) / 1048576
            rss_mb = (rss * PAGESIZE) / 1048576
            priv_mb = ((rss - shared) * PAGESIZE) / 1048576
    except:
        continue

    # cmdline
    cmd = "[?]"
    try:
        with open(f"/proc/{pid}/cmdline", "rb") as f:
            raw = f.read().replace(b"\x00", b" ").strip()
            if raw:
                cmd = raw.decode(errors="ignore")[:80]
            else:
                continue
    except:
        continue

    # uid → username
    user = "?"
    try:
        with open(f"/proc/{pid}/status") as f:
            for line in f:
                if line.startswith("Uid:"):
                    uid = line.split()[1]
                    break
            else:
                uid = None

        if uid:
            try:
                import pwd
                user = pwd.getpwuid(int(uid)).pw_name
            except:
                user = uid
    except:
        pass

    entries.append(
        (rss_mb,
         f"{size_mb:13.2f} {rss_mb:13.2f} {priv_mb:13.2f} {user:<8} {cmd}"))

# sort by RSS descending
entries.sort(key=lambda x: x[0], reverse=True)

for _, line in entries:
    print(line)

# Kernel memory usage
total = 0
try:
    with open("/proc/meminfo") as f:
        for line in f:
            if any(k in line for k in ("KernelStack", "Slab", "PageTables",
                                       "VmallocUsed")):
                total += int(line.split()[1])
except:
    pass

print(f"\nKernel RAM usage: {total / 1024:.2f} MB")
