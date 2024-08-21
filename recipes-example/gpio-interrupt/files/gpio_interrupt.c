#include <stdio.h>
#include <gpiod.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define GPIO_CHIP "/dev/gpiochip1" // GPIO 칩 경로 (gpiochip1)
#define GPIO_PIN 462 // 사용할 GPIO 핀 번호

volatile sig_atomic_t stop;

void handle_signal(int signal) {
    stop = 1;
}

int main(int argc, char **argv)
{
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    struct gpiod_line_event event;
    int ret;

    // 신호 처리기 설정
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // GPIO 칩 열기
    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("Open chip failed\n");
        return -1;
    }

    // GPIO 라인 설정
    line = gpiod_chip_get_line(chip, GPIO_PIN);
    if (!line) {
        perror("Get line failed\n");
        gpiod_chip_close(chip);
        return -1;
    }

    // GPIO 라인 요청 (상승 엣지에서 인터럽트 발생)
    ret = gpiod_line_request_rising_edge_events(line, "gpio_interrupt");
    if (ret < 0) {
        perror("Request line events failed\n");
        gpiod_chip_close(chip);
        return -1;
    }

    printf("Waiting for GPIO events on line %d...\n", GPIO_PIN);

    // 이벤트 루프
    while (!stop) {
        ret = gpiod_line_event_wait(line, NULL);
        if (ret < 0) {
            perror("Wait event failed\n");
            break;
        }

        if (ret == 0) {
            // 타임아웃 처리 (여기서는 타임아웃 사용하지 않음)
            continue;
        }

        ret = gpiod_line_event_read(line, &event);
        if (ret < 0) {
            perror("Read event failed\n");
            break;
        }

        if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE) {
            printf("Rising edge event detected\n");
        }
    }

    // GPIO 라인 및 칩 해제
    gpiod_line_release(line);
    gpiod_chip_close(chip);

    return 0;
}

