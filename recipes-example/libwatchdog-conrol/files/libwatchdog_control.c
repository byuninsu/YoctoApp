#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <linux/watchdog.h>  
#include "libwatchdog_control.h"

#define STATUS_SUCCESS 0x00
#define STATUS_ERROR   0x01
#define CONFIG_FILE "/etc/watchdog.conf" 

uint32_t StartWatchDog(void) {
    int ret = system("sudo service watchdog start");
    if (ret == -1) {
        perror("Failed to start watchdog service");
        return STATUS_ERROR;
    } else {
        printf("Watchdog service started.\n");
        return STATUS_SUCCESS;
    }
}

uint32_t ReStartWatchDog(void) {
    int ret = system("sudo service watchdog restart");
    if (ret == -1) {
        perror("Failed to restart watchdog service");
        return STATUS_ERROR;
    } else {
        printf("Watchdog service restarted.\n");
        return STATUS_SUCCESS;
    }
}

uint32_t StopWatchDog(void) {
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

    StopWatchDog();

    FILE *file = fopen(CONFIG_FILE, "r+");
    if (file == NULL) {
        perror("Failed to open config file");
        return STATUS_ERROR;
    }

    char line[256];
    long pos;
    int found = 0;

    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "watchdog-timeout") != NULL) {
            found = 1;
            pos = ftell(file) - strlen(line); // 현재 파일 포인터 위치에서 해당 라인의 시작 위치 계산
            fseek(file, pos, SEEK_SET); // 해당 위치로 파일 포인터 이동
            fprintf(file, "watchdog-timeout=%d\n", timeout); // 새로운 timeout 값으로 덮어쓰기
            break;
        }
    }

    if (!found) {
        // 파일의 끝에 추가
        fseek(file, 0, SEEK_END);
        fprintf(file, "watchdog-timeout=%d\n", timeout);
    }

    fclose(file);

    printf("Watchdog timeout in config set to %d seconds.\n", timeout);

    StartWatchDog();

    return STATUS_SUCCESS;
}
