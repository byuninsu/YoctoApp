#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <gpiod.h>

#define GPIO_CHIP "/dev/gpiochip1"  // GPIO 컨트롤러를 gpiochip1으로 설정
#define GPIO_PIN  126               // 감시하려는 GPIO 핀 번호를 126으로 설정

int main() {
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    struct gpiod_line_event event;
    int ret;

    // GPIO 칩 열기
    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("Failed to open GPIO chip");
        return 1;
    }

    // GPIO 라인(핀) 요청
    line = gpiod_chip_get_line(chip, GPIO_PIN);
    if (!line) {
        perror("Failed to get GPIO line");
        gpiod_chip_close(chip);
        return 1;
    }

    // 이벤트 대기를 위해 GPIO 라인 요청 (FALLING_EDGE 감지)
    ret = gpiod_line_request_falling_edge_events(line, "gpio-event-handler");
    if (ret < 0) {
        perror("Failed to request falling edge events");
        gpiod_chip_close(chip);
        return 1;
    }

    printf("Waiting for GPIO events on GPIO %d...\n", GPIO_PIN);

    while (1) {
        // 이벤트 대기
        ret = gpiod_line_event_wait(line, NULL);
        if (ret < 0) {
            perror("Error waiting for GPIO event");
            break;
        }

        // 이벤트 읽기
        ret = gpiod_line_event_read(line, &event);
        if (ret < 0) {
            perror("Error reading GPIO event");
            break;
        }

        if (event.event_type == GPIOD_LINE_EVENT_FALLING_EDGE) {
            printf("GPIO %d went LOW!\n", GPIO_PIN);
        }
    }

    // 자원 정리
    gpiod_chip_close(chip);
    return 0;
}
