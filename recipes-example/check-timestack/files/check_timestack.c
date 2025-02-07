#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define JSON_FILE_PATH "/mnt/dataSSD/ssdLog_test.json"
#define SSD_DEVICE "/dev/nvme0n1"
#define LOG_INTERVAL 900 // 1 minute in seconds

// 시간 계산 함수
void update_time(int *hours, int *minutes) {
    // 분 단위로 변환하여 추가
    *minutes = *minutes + LOG_INTERVAL / 60;

    // 남은 초를 고려하여 시간 계산
    if (*minutes >= 60) {
        *hours += *minutes / 60;    // 시간 증가
        *minutes %= 60;             // 남은 분
    }
}

// 파일에서 마지막 기록된 시간 값을 읽어 초기화
void initialize_time(int *hours, int *minutes) {
    FILE *file = fopen(JSON_FILE_PATH, "r");
    if (!file) {
        // 파일이 없으면 0시간 0분부터 시작
        *hours = 0;
        *minutes = 0;
        return;
    }

    char line[256];
    char last_time[9] = "00000000"; // 기본값
    while (fgets(line, sizeof(line), file)) {
        // 읽은 줄에서 "time": "HHMMSS"를 찾아 시간 값 추출
        char *time_ptr = strstr(line, "\"time\": \"");
        if (time_ptr) {
            sscanf(time_ptr + 9, "%8s", last_time); // "time": "HHMMSS" 값만 저장
        }
    }
    fclose(file);

    // 마지막 시간 값에서 hours, minutes, seconds 파싱
    int seconds = 0;
    sscanf(last_time, "%4d%2d%2d", hours, minutes, &seconds);
    *minutes += seconds / 60; // 초를 분으로 변환
    *hours += *minutes / 60;  // 분을 시간으로 변환
    *minutes %= 60;           // 초과된 분 정리
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


    // 파일에서 초기 시간값 설정
    initialize_time(&hours, &minutes);

    printf("check_timestack ++ , initialize_time  hours: %d, minutes : %d",hours, minutes);
    fflush(stdout);

    while (1) {

        if ( firstTime == 0 ){
            firstTime = 1;
            sleep(LOG_INTERVAL); 
        } 

        update_time(&hours, &minutes); // 시간 업데이트

        char formatted_time[9];
        snprintf(formatted_time, sizeof(formatted_time), "%04d%02d%02d", hours, minutes, 0); // HHHHMMSS format

        char *ssd_capacity = get_ssd_available_capacity();
        if (ssd_capacity == NULL) {
            fprintf(stderr, "Failed to get SSD capacity\n");
        } else {
            log_data(formatted_time, ssd_capacity);
        }

        sleep(LOG_INTERVAL); // Wait for LOG_INTERVAL second
    }

    return 0;
}
