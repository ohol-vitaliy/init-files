My bundle of init script and other usefull things. 
For sure will not work on all devices. Consider it as a template or skeleton that should be adjusted for your needs

Tested on `Debian` and `Alpine Linux`

## Notes
- `custom-init-script init` - init script
- `custom-init-script shutdown` - cleanup before shutdown/reboot
- `custom-init-script once` - things that should be done once after init. Like connecting to WIFI, setting display brighness, setting volume, cleaning cache, etc.
- `power-manager` - checks if laptop on battery and applies some energy saving options, checks if lid closed or power button pressed and puts device into sleep
- `syncdate.sh` - synchronize date and time. Used by `udhcpc.script` when network connection established
- `00-input.conf` - touchpad and keyboard configs for Xorg, as `mdev` is used instead of `udev` or similar
- `inittab` - config for `busybox init`
- `mdev.conf` - config for `mdev`
- `input-link.sh` - used in `mdev` to do some actions on devices (currently, creating special symlinks for keyboard, touchpad, trackpoint)
- `auto-getty.sh` - used in `inittab` to run `agetty` by default and use `getty` as fallback on `tty1-4`
- `scripts/` - directory with scripts that I wrote for my setup
- `playground/` - some ideas or concepts. Potentially broken
- `unused_scripts/` - old scripts. Preserved as archive

## Install
To install just run `sudo make install` and it will put all scripts and configs in the right places

## Using busybox init
Put `init=/usr/bin/busybox init` in the grub entry

## Brighness fn keys not working
Try to put `acpi_osi=` in the grub entry (yes, there are nothing after equals)

## `doas` instead of `sudo`
- Create `/etc/doas.conf`
- Put `permit persist keepenv [YOUR_USERNAME] as root` in it
- Don't forget to `chown root:root /etc/doas.conf` and `chmod 0400 /etc/doas.conf`
