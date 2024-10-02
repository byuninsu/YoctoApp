#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include "push_button.h"

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



