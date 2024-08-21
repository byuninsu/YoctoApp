#include "usb_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// Internal function to set USB port state
static uint8_t set_usb_port(const char* port, int enable) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/authorized", port);

    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("Failed to open authorized file");
        return 1; // Return error code
    }

    const char* value = enable ? "1" : "0";
    if (write(fd, value, strlen(value)) == -1) {
        perror("Failed to write to authorized file");
        close(fd);
        return 1; // Return error code
    }

    close(fd);
    return 0; // Return success code
}

uint8_t GPU_API ActivateUSB(void) {
    uint8_t result1 = set_usb_port("usb1", 1);
    uint8_t result2 = set_usb_port("usb2", 1);
    return result1 || result2;
}

uint8_t GPU_API DeactivateUSB(void) {
    uint8_t result1 = set_usb_port("usb1", 0);
    uint8_t result2 = set_usb_port("usb2", 0);
    return result1 || result2;
}



