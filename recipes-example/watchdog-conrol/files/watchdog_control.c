#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <linux/watchdog.h>  
#include "watchdog_control.h"

#define STATUS_SUCCESS 0x00
#define STATUS_ERROR   0x01

uint32_t GPU_API StartWatchDog(void) {
    int ret = system("sudo service watchdog start");
    if (ret == -1) {
        perror("Failed to start watchdog service");
        return STATUS_ERROR;
    } else {
        printf("Watchdog service started.\n");
        return STATUS_SUCCESS;
    }
}

uint32_t GPU_API StopWatchDog(void) {
    int ret = system("sudo service watchdog stop");
    if (ret == -1) {
        perror("Failed to stop watchdog service");
        return STATUS_ERROR;
    } else {
        printf("Watchdog service stopped.\n");
        return STATUS_SUCCESS;
    }
}

uint32_t SetWatchDogTimeout(int timeout) {
    int fd = open("/dev/watchdog", O_RDWR);
    if (fd == -1) {
        perror("Failed to open /dev/watchdog");
        return STATUS_ERROR;
    }

    if (ioctl(fd, WDIOC_SETTIMEOUT, &timeout) == -1) {
        perror("Failed to set watchdog timeout");
        close(fd);
        return STATUS_ERROR;
    }

    printf("Watchdog timeout set to %d seconds.\n", timeout);
    close(fd);
    return STATUS_SUCCESS;
}
