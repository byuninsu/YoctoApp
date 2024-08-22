#include "optic_control.h"
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <string.h>

#define TRANSCEIVER_ADDR 0x6b // 970-00402 의 I2C 주소

static int i2c_fd = -1;

typedef enum {
    TRANSCEIVER_TEST_RESISTER = 0x07
} transceiver_i2c_commands;

static uint8_t i2cInit() {
    // I2C 버스가 열려있지 않으면 열기
    if (i2c_fd < 0) {
        i2c_fd = open("/dev/i2c-0", O_RDWR);
        if (i2c_fd < 0) {
            perror("Failed to open I2C bus");
            return 1;
        }

        if (ioctl(i2c_fd, I2C_SLAVE, TRANSCEIVER_ADDR) < 0) {
            perror("Failed to select I2C device");
            close(i2c_fd);
            i2c_fd = -1;
            return 1;
        }
    }
    
    return 0;
}

// I2C 장치에서 바이트를 읽는 함수
static int i2c_read_byte(unsigned char reg, unsigned char *value) {
    if (write(i2c_fd, &reg, 1) != 1) {
        perror("Failed to write to the i2c bus");
        return -1;
    }
    if (read(i2c_fd, value, 1) != 1) {
        perror("Failed to read from the i2c bus");
        return -1;
    }
    return 0;
}

// I2C 장치에 바이트를 쓰는 함수
static int i2c_write_byte(unsigned char reg, unsigned char value) {
    unsigned char buffer[2];
    buffer[0] = reg;
    buffer[1] = value;
    if (write(i2c_fd, buffer, 2) != 2) {
        perror("Failed to write to the i2c bus");
        return -1;
    }
    return 0;
}

uint32_t GPU_API getOpticTestRegister(void) {

    uint32_t currentValue = 0;

    i2cInit();

    if (i2c_read_byte(TRANSCEIVER_TEST_RESISTER, &currentValue) != 0) {
        fprintf(stderr, "Failed to read from I2C device\n");
        return 1;
    }

    printf("Current TRANSCEIVER_TEST_RESISTER Value : %d", currentValue);

    return currentValue;
}


uint8_t setOpticPort(void) {
    const char *scripts[] = {
        "/usr/bin/port9.sh",
        "/usr/bin/port10.sh",
        "/usr/bin/port2.sh",
        "/usr/bin/port3.sh",
        "/usr/bin/port4.sh",
        "/usr/bin/port5.sh",
        "/usr/bin/port6.sh"
    };

    size_t num_scripts = sizeof(scripts) / sizeof(scripts[0]);

    for (size_t i = 0; i < num_scripts; i++) {
        int status = system(scripts[i]);
        if (status == -1) {
            printf("Failed to execute %s\n", scripts[i]);
            return 1;
        } else {
            printf("Executed %s successfully\n", scripts[i]);
            return 0;
        }
    }
}

