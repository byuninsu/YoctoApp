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
#include "ethernet_control.h"

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

uint32_t getOpticTestRegister(void) {
    unsigned char byteValue = 0; 
    uint32_t currentValue = '0';

    i2cInit();

    if (i2c_read_byte(TRANSCEIVER_TEST_RESISTER, &byteValue) != 0) {
        fprintf(stderr, "Failed to read from I2C device\n");
        return 1;
    }

    currentValue = (uint32_t)byteValue;

    printf("Current TRANSCEIVER_TEST_RESISTER Value : %d", currentValue);

    return currentValue;
}


uint8_t setOpticPort(void) {
    const char *optic_scripts[] = {
        "/usr/bin/port9.sh",
        "/usr/bin/port10.sh",
        "/usr/bin/port2.sh",
        "/usr/bin/port3.sh",
        "/usr/bin/port4.sh",
        "/usr/bin/port5.sh",
        "/usr/bin/port6.sh",
        "enable_port.sh"
    };

    size_t num_scripts = sizeof(optic_scripts) / sizeof(optic_scripts[0]);
    uint8_t result = 0; // 0: 성공, 1: 실패

    char *iface = checkEthernetInterface();

    if (iface == NULL) {
        printf("No valid Ethernet interface found.\n");
        return 1; // 실패
    }

    for (size_t i = 0; i < num_scripts; i++) {
        char command[256];
        snprintf(command, sizeof(command), "%s %s", optic_scripts[i], iface);

        int status = system(command);
        if (status == -1 || WEXITSTATUS(status) != 0) {
            // 스크립트 실행 실패 처리
            printf("Failed to execute %s\n", optic_scripts[i]);
            result = 1; // 실패 상태로 업데이트
        } else {
            // 스크립트 실행 성공 처리
            printf("Executed %s successfully\n", optic_scripts[i]);
        }
    }

    return result;
}

uint8_t setDefaultPort(void) {
    const char *script = "/usr/bin/copper_setting.sh";
    "enable_port.sh"
    uint8_t result = 0; // 0: 성공, 1: 실패

    char *iface = checkEthernetInterface();

    if (iface == NULL) {
        printf("No valid Ethernet interface found.\n");
        return 1; // 실패
    }

    char command[256];
    snprintf(command, sizeof(command), "%s %s", script, iface);

    int status = system(command);
    if (status == -1 || WEXITSTATUS(status) != 0) {
        // 스크립트 실행 실패 처리
        printf("Failed to execute %s\n", script);
        result = 1; // 실패 상태로 업데이트
    } else {
        // 스크립트 실행 성공 처리
        printf("Executed %s successfully\n", script);
    }

    return result;
}
