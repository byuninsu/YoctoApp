#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include "libpush_button.h"

uint8_t GetButtonState(void) {
    static int fd_chip = -1;
    static int fd_line = -1;
    static int initialized = 0;
    struct gpiohandle_request req;
    struct gpiohandle_data data;

    // 초기화되지 않은 경우만 gpiochip 열고 GPIO 핸들 요청
    if (!initialized) {
        fd_chip = open("/dev/gpiochip1", O_RDONLY);
        if (fd_chip < 0) {
            perror("Failed to open gpiochip1");
            return 2;
        }

        memset(&req, 0, sizeof(req));
        req.lineoffsets[0] = 127;
        req.flags = GPIOHANDLE_REQUEST_INPUT;
        req.lines = 1;
        strcpy(req.consumer_label, "GetButton");

        if (ioctl(fd_chip, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
            perror("Failed to get line handle");
            close(fd_chip);
            fd_chip = -1;
            return 2;
        }

        fd_line = req.fd;
        initialized = 1;
    }

    // 버튼 상태 읽기
    if (ioctl(fd_line, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) {
        perror("Failed to read line value");
        return 2;
    }

    if (data.values[0] == 0) {
        printf("ON (low)\n");
        return 1;
    } else {
        printf("OFF (high)\n");
        return 0;
    }
}




