#include "librs232_control.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define SERIAL_DEVICE "3100000.serial"
#define SYSFS_PATH "/sys/bus/platform/drivers/serial-tegra/"
#define RS232_SUCCESS 0x00
#define RS232_FAILURE 0x01

uint8_t ActivateRS232(void) {
    char command[256];
    snprintf(command, sizeof(command), "echo %s > %sbind", SERIAL_DEVICE, SYSFS_PATH);
    if (system(command) == -1) {
        perror("Failed to enable UART");
        return RS232_FAILURE;
    } else {
        printf("UART enabled successfully.\n");
        return RS232_SUCCESS;
    }
}

uint8_t DeactivateRS232(void) {
    char command[256];
    snprintf(command, sizeof(command), "echo %s > %sunbind", SERIAL_DEVICE, SYSFS_PATH);
    if (system(command) == -1) {
        perror("Failed to disable UART");
        return RS232_FAILURE;
    } else {
        printf("UART disabled successfully.\n");
        return RS232_SUCCESS;
    }
}

int isRS232Available(void) {
    if (access("/dev/ttyTHS0", F_OK) == 0) {
        // 디바이스 파일이 존재함
        return 0;
    } else {
        // 존재하지 않음
        return 1;
    }
}
