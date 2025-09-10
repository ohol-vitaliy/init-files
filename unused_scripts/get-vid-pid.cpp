#include <endian.h>
#include <fcntl.h>
#include <linux/usb/ch9.h>
#include <linux/usbdevice_fs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifndef USB_DT_DEVICE
#define USB_DT_DEVICE 0x01
#endif

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s /dev/bus/usb/BBB/DDD\n", argv[0]);
        return 2;
    }

    const char* path = argv[1];
    int fd = open(path, O_RDWR);

    if (fd < 0)
    {
        perror("open");
        return 1;
    }

    struct usbdevfs_ctrltransfer ctrl;
    unsigned char buf[18];
    memset(&ctrl, 0, sizeof ctrl);
    memset(buf, 0, sizeof buf);

    ctrl.bRequestType = 0x80; /* IN, standard, device */
    ctrl.bRequest = 0x06;     /* GET_DESCRIPTOR */
    ctrl.wValue = (USB_DT_DEVICE << 8) | 0;
    ctrl.wIndex = 0;
    ctrl.wLength = sizeof buf;
    ctrl.timeout = 1000;
    ctrl.data = buf;

    if (ioctl(fd, USBDEVFS_CONTROL, &ctrl) < 0)
    {
        perror("ioctl");
        close(fd);
        return 1;
    }

    const uint16_t idVendor = le16toh(*(uint16_t*)(buf + 8));
    const uint16_t idProduct = le16toh(*(uint16_t*)(buf + 10));
    printf("%04x:%04x\n", idVendor, idProduct);

    close(fd);
    return 0;
}
