#!/usr/bin/env sh

get_command() {
	PATH="/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin" command -v "$1"
}

CAT="$(get_command cat)"
GREP="$(get_command grep)"
ID="$(get_command id)"
LOGGER="$(get_command logger) -t $0"
PRINTF="$(get_command printf)"
SLEEP="$(get_command sleep)"
TEE="$(get_command tee)"

# /sys/devices/system/cpu/intel_pstate/max_perf_pct
# /sys/devices/system/cpu/intel_pstate/min_perf_pct
# /sys/devices/system/cpu/intel_pstate/no_turbo

require_sudo() {
	if [ "$($ID -u)" != 0 ]; then
		$PRINTF "Requested operation requires superuser privilege\n"
		exit 1
	fi
}

on_ac() {
	$PRINTF "Running on AC...\n"
	$PRINTF 0 >/proc/sys/vm/laptop_mode
	$PRINTF 500 >/proc/sys/vm/dirty_writeback_centisecs
	$PRINTF 5 >/proc/sys/vm/dirty_background_ratio
	$PRINTF 50 >/proc/sys/vm/dirty_ratio

	$PRINTF performance >/sys/module/pcie_aspm/parameters/policy
	$PRINTF on >/sys/block/sda/device/power/control
	$PRINTF 0 >/sys/module/block/parameters/events_dfl_poll_msecs
	$PRINTF 0 >/sys/module/snd_hda_intel/parameters/power_save
	$PRINTF 1 >/proc/sys/kernel/nmi_watchdog
	$PRINTF disabled | $TEE /sys/bus/usb/devices/usb*/power/wakeup >/dev/null
	$PRINTF on | $TEE /sys/bus/i2c/devices/i2c-*/device/power/control >/dev/null
	$PRINTF max_performance | $TEE /sys/class/scsi_host/host*/link_power_management_policy >/dev/null
	$PRINTF on | $TEE /sys/bus/pci/devices/0000:00:17.0/ata*/power/control >/dev/null
	$PRINTF on | $TEE /sys/bus/pci/devices/*/power/control >/dev/null
	$PRINTF performance | $TEE /sys/devices/system/cpu*/cpufreq/policy*/energy_performance_preference >/dev/null
	$PRINTF powersave | $TEE /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null

	$PRINTF 100 >/sys/devices/system/cpu/intel_pstate/max_perf_pct
	$PRINTF 60 >/sys/devices/system/cpu/intel_pstate/min_perf_pct
	$PRINTF 128 | $TEE /sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference >/dev/null
}

on_batt() {
	$PRINTF "Running on battery...\n"
	$PRINTF 5 >/proc/sys/vm/laptop_mode
	$PRINTF 6000 >/proc/sys/vm/dirty_writeback_centisecs
	$PRINTF 10 >/proc/sys/vm/dirty_background_ratio
	$PRINTF 20 >/proc/sys/vm/dirty_ratio

	$PRINTF powersave >/sys/module/pcie_aspm/parameters/policy
	$PRINTF auto >/sys/block/sda/device/power/control
	$PRINTF 0 >/sys/module/block/parameters/events_dfl_poll_msecs
	$PRINTF 1 >/sys/module/snd_hda_intel/parameters/power_save
	$PRINTF 0 >/proc/sys/kernel/nmi_watchdog
	$PRINTF disabled | $TEE /sys/bus/usb/devices/usb*/power/wakeup >/dev/null
	$PRINTF auto | $TEE /sys/bus/i2c/devices/i2c-*/device/power/control >/dev/null
	$PRINTF med_power_with_dipm | $TEE /sys/class/scsi_host/host*/link_power_management_policy >/dev/null
	$PRINTF auto | $TEE /sys/bus/pci/devices/0000:00:17.0/ata*/power/control >/dev/null
	$PRINTF auto | $TEE /sys/bus/pci/devices/*/power/control >/dev/null
	$PRINTF power | $TEE /sys/devices/system/cpu*/cpufreq/policy*/energy_performance_preference >/dev/null
	$PRINTF powersave | $TEE /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null

	$PRINTF 60 >/sys/devices/system/cpu/intel_pstate/max_perf_pct
	$PRINTF 20 >/sys/devices/system/cpu/intel_pstate/min_perf_pct
	$PRINTF 225 | $TEE /sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference >/dev/null
}

get_ac_state() {
	if $CAT /sys/class/power_supply/AC*/online | $GREP -q 1; then
		$PRINTF "1"
	else
		$PRINTF "0"
	fi
}

require_sudo | $LOGGER -p user.err
last_state=""

while true; do
	current_state=$(get_ac_state)

	if [ "$current_state" != "$last_state" ]; then
		if [ "$current_state" = "1" ]; then
			on_ac | $LOGGER
		else
			on_batt | $LOGGER
		fi
		last_state=$current_state
	fi

	$SLEEP 10
done
