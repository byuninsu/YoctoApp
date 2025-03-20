#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <gpiod.h>
#include <sys/stat.h>
#include "stm32_control.h"
#include "nvram_control.h"

#define GPIO_CHIP "/dev/gpiochip1"  // GPIO 컨트롤러를 gpiochip1으로 설정
#define GPIO_PIN  125               // 감시하려는 GPIO 핀 번호를 125으로 설정
#define HOLDUP_LOG_FILE_DIR "/mnt/dataSSD/"
#define HOLDUP_LOG_FILE_NAME "HoldupLog.json"
#define LOG_FILE_PATH HOLDUP_LOG_FILE_DIR HOLDUP_LOG_FILE_NAME

int writeHoldupState(void){
    usleep(10000);
    uint8_t holdupPFvalue = sendRequestHoldupPF(); //1 이 전원정상
    usleep(10000);
    uint8_t holdupCCvalue = sendRequestHoldupCC(); //0 이 충전완료

    int X;
    if ( holdupPFvalue == 1 && holdupCCvalue == 1 ) {
        X = 0;
    } else if (holdupPFvalue == 0 && holdupCCvalue == 1) {
        X = 1; // 캐패시터 방전, 전원 비정상
    } else if (holdupPFvalue == 1 && holdupCCvalue == 0) {
        X = 2; // 캐패시터 충전, 전원 정상
    } else if (holdupPFvalue == 0 && holdupCCvalue == 0) {
        X = 3; // 캐패시터 충전, 전원 비정상
    } else {
        return -1; // 예외 상황 (정의되지 않은 상태)
    }

    // 현재 시간 가져오기
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[20]; // "YYYYMMDDHHMMSS"

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", timeinfo);

    // 파일 경로 확인 및 디렉터리 생성 (없으면 생성)
    struct stat st = {0};
    if (stat(HOLDUP_LOG_FILE_DIR, &st) == -1) {
        mkdir(HOLDUP_LOG_FILE_DIR, 0777);
    }

    // JSON 파일에 데이터 기록
    FILE *file = fopen(LOG_FILE_PATH, "a");
    if (file == NULL) {
        printf("Error: Could not open log file %s\n", LOG_FILE_PATH);
        return -1;
    }

    // JSON 형식으로 기록
    fprintf(file, "{\"timestamp\": \"%d_%s\", \"holdup_state\": %d}\n", X, timestamp, X);

    // 캐시 플러시 및 디스크 동기화
    fflush(file);  // 캐시 플러시
    fsync(fileno(file)); // 디스크 동기화

    fclose(file);

    printf("Holdup state logged: %d_%s\n", X, timestamp);
    return 0;

}

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
            printf("Hold up Module Interrupt Received !!, Write current Holdup Module State\n");
            writeHoldupState();
        }
    }

    // 자원 정리
    gpiod_chip_close(chip);
    return 0;
}
