#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <cjson/cJSON.h>
#include "nvram_control.h"

#define JSON_FILE_PATH "/mnt/dataSSD/ssdLog_test.json"
#define SSD_DEVICE "/dev/nvme0n1"
#define LOG_INTERVAL 900 

// 시간 계산 함수
void update_time(int *hours, int *minutes, int *seconds) {
    printf("update_time ++++ hours :%d , minutes :%d, seconds :%d\n", *hours, *minutes, *seconds);
    *seconds += LOG_INTERVAL;
    *minutes += *seconds / 60;
    *seconds %= 60;
    
    if (*minutes >= 60) {
        *hours += *minutes / 60;
        *minutes %= 60;
    }
}

// 파일에서 마지막 기록된 시간 값을 읽어 초기화
void initialize_time(int *hours, int *minutes, int *seconds) {
    FILE *file = fopen(JSON_FILE_PATH, "r");
    if (!file) {
        *hours = 0;
        *minutes = 0;
        *seconds = 0;
        return;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    if (filesize == 0) {
        fclose(file);
        *hours = 0;
        *minutes = 0;
        *seconds = 0;
        return;
    }

    long pos = filesize - 1;
    char ch;
    int found = 0;

    // 거꾸로 가면서 \n을 찾되, 마지막 줄이 \n 없이 끝났을 경우 고려
    while (pos >= 0) {
        fseek(file, pos, SEEK_SET);
        fread(&ch, 1, 1, file);
        if (ch == '\n' && pos != filesize - 1) {  // 파일 끝에 바로 \n이 있을 경우는 무시
            found = 1;
            pos++;
            break;
        }
        pos--;
    }

    if (pos < 0) pos = 0;  // 파일 처음까지 갔다면 pos를 0으로

    fseek(file, pos, SEEK_SET);

    char line[1024] = {0};  // 넉넉히 버퍼 잡자
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        *hours = 0;
        *minutes = 0;
        *seconds = 0;
        return;
    }
    fclose(file);

    // 읽은 line 디버그로 찍어보자 (초기 디버깅용)
    printf("[DEBUG] last line read: %s\n", line);

    cJSON *json = cJSON_Parse(line);
    if (json) {
        cJSON *time_item = cJSON_GetObjectItem(json, "time");
        if (cJSON_IsString(time_item) && time_item->valuestring != NULL) {
            sscanf(time_item->valuestring, "%4d%2d%2d", hours, minutes, seconds);
        }
        cJSON_Delete(json);
    } else {
        printf("[ERROR] Failed to parse JSON line: %s\n", line);
        *hours = 0;
        *minutes = 0;
        *seconds = 0;
    }
}


char* get_ssd_available_capacity() {
    char command[256];
    char buffer[128];
    int used_percentage = -1;

    snprintf(command, sizeof(command), "df %s | awk 'NR==2 {print $5}' | tr -d '%%'", SSD_DEVICE); // Remove '%'
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        perror("popen failed");
        return NULL;
    }

    if (fgets(buffer, sizeof(buffer) - 1, pipe)) {
        buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline
        if (strlen(buffer) > 0) {
            used_percentage = atoi(buffer);
        } else {
            fprintf(stderr, "Unexpected empty buffer from command\n");
        }
    }
    pclose(pipe);

    if (used_percentage >= 0) {
        int available_percentage = 100 - used_percentage;
        char *result = malloc(4); // Allocate memory for the result (3 digits + null terminator)
        if (result == NULL) {
            perror("malloc failed");
            return NULL;
        }
        snprintf(result, 4, "%03d", available_percentage); // Format as 3 characters with leading zeros if necessary
        return result;
    } else {
        return NULL; // Error case
    }
}

void log_data(const char *time_elapsed, const char *ssd_capacity) {
    FILE *file = fopen(JSON_FILE_PATH, "a");
    if (!file) {
        perror("Failed to open log file");
        fprintf(stderr, "Ensure the path %s exists and is writable.\n", JSON_FILE_PATH);
        return;
    }

    if (fprintf(file, "{\"time\": \"%s\", \"ssdCapacity\": \"%s\"}\n", time_elapsed, ssd_capacity) < 0) {
        perror("Failed to write to log file");
    }

    fclose(file);
}

int main() {
    int firstTime = 0;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;


    // 파일에서 초기 시간값 설정
    initialize_time(&hours, &minutes, &seconds);

    printf("check_timestack ++ , initialize_time  hours: %d, minutes : %d \n",hours, minutes);
    fflush(stdout);

    while (1) {

        if ( firstTime == 0 ){
            firstTime = 1;
            sleep(LOG_INTERVAL); 
        } 

        update_time(&hours, &minutes, &seconds); // 시간 업데이트

        char formatted_time[9];
        snprintf(formatted_time, sizeof(formatted_time), "%04d%02d%02d", hours, minutes, 0); // HHHHMMSS format

        char *ssd_capacity = get_ssd_available_capacity();
        if (ssd_capacity == NULL) {
            fprintf(stderr, "Failed to get SSD capacity\n");
        } else {
            log_data(formatted_time, ssd_capacity);
        }

        WriteCumulativeTime(LOG_INTERVAL/60);

        sleep(LOG_INTERVAL); // Wait for LOG_INTERVAL second
    }

    return 0;
}
