#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/gpio.h> 
#include "libpush_button.h"

// #define GPIO_CHIP "gpiochip1"
// #define GPIO_PIN "127"

// uint8_t  GetButtonState(void) {
//     int ret;
//     char command[64];
//     char value[2] = {0};

//     // gpioget command
//     snprintf(command, sizeof(command), "gpioget %s %s", GPIO_CHIP, GPIO_PIN);

//     // Run the command and get the result
//     FILE *fp = popen(command, "r");
//     if (fp == NULL) {
//         perror("Unable to execute gpioget command");
//         return -1;
//     }

//     if (fgets(value, sizeof(value), fp) == NULL) {
//         perror("Unable to read gpioget output");
//         pclose(fp);
//         return -1;
//     }

//     pclose(fp);

//     // Convert '0' or '1' to integer
//     return (value[0] == '1') ? 1 : 0;
// }

uint8_t GetButtonState(void) {
    int fd_chip;
    struct gpiohandle_request req;
    struct gpiohandle_data data;

    // gpiochip1 열기
    fd_chip = open("/dev/gpiochip1", O_RDONLY);
    if (fd_chip < 0) {
        perror("Failed to open gpiochip1");
        return 2;  // 오류 시 2 반환
    }

    // 127번 GPIO 라인 요청
    req.lineoffsets[0] = 127;  // PT.05에 해당하는 GPIO 번호
    req.flags = GPIOHANDLE_REQUEST_INPUT;
    req.lines = 1;

    if (ioctl(fd_chip, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
        perror("Failed to get line handle");
        close(fd_chip);
        return 2;  // 오류 시 2 반환
    }

    // GPIO 상태 읽기
    if (ioctl(req.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) {
        perror("Failed to read line value");
        close(req.fd);
        close(fd_chip);
        return 2;  // 오류 시 2 반환
    }

    // GPIO 상태에 따른 출력 및 반환
    if (data.values[0] == 0) {
        printf("ON (low)\n");
        close(req.fd);
        close(fd_chip);
        return 1;  // ON (low) 상태 시 1 반환
    } else {
        printf("OFF (high)\n");
        close(req.fd);
        close(fd_chip);
        return 0;  // OFF (high) 상태 시 0 반환
    }
}


