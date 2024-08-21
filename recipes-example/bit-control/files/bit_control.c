#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include "cJSON.h"
#include "bit_control.h"
//#include "bit_manager.h"

#define STATUS_SUCCESS 0x00
#define STATUS_ERROR 0x01
#define LOG_FILE_DIR "/dev/dataSSD/"  // SSD 상에 로그를 저장할 디렉토리


const char *GetItemName(uint32_t mItem) {
    switch (mItem) {
        case 1: return "GPU module";
        case 2: return "SSD (Data store)";
        case 3: return "GPIO Expander";
        case 4: return "Ethernet MAC/PHY";
        case 5: return "NVRAM";
        case 6: return "Discrete Input";
        case 7: return "Discrete Output";
        case 8: return "RS232 Transceiver";
        case 9: return "SSD (Boot OS)";
        case 10: return "Ethernet PHY";
        case 11: return "10G Ethernet Switch";
        case 12: return "Optic Transceiver";
        case 13: return "System Monitor";
        case 14: return "Temperature Sensor";
        case 15: return "Power Monitor";
        case 16: return "DC/DC Converter x 2";
        case 17: return "Hold-up Module";
        case 18: return "AC/DC Converter";
        default: return "Unknown";
    }
}

uint32_t GPU_API WriteBitErrorData(uint32_t *mItemStates, uint32_t mtype) {
    FILE *logFile;
    time_t now;
    struct tm *timeinfo;
    char timestamp[20];
    char logFileName[64];
    char logFilePath[128];

    // Get the current time
    time(&now);
    timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    strftime(logFileName, sizeof(logFileName), "%Y%m%d_BitErrorLog.json", timeinfo);

    // Set the log file path
    snprintf(logFilePath, sizeof(logFilePath), "%s%s", LOG_FILE_DIR, logFileName);

    // Open the log file for appending
    logFile = fopen(logFilePath, "a");
    if (logFile == NULL) {
        perror("Unable to open log file");
        return STATUS_ERROR;
    }

    // Start writing the JSON object
    fprintf(logFile, "{");

    // Write all mItem states
    for (uint32_t i = 1; i <= 18; i++) {
        fprintf(logFile, "\"%s\":%u", GetItemName(i), mItemStates[i-1]);
        if (i < 18) {
            fprintf(logFile, ",");
        }
    }

    // Write mtype and timestamp
    fprintf(logFile, ",\"mtype\":%u,\"timestamp\":\"%s\"}\n", mtype, timestamp);

    // Close the log file
    fclose(logFile);

    return STATUS_SUCCESS;
}

cJSON* GPU_API ReadBitErrorLog(const char* date) {
    FILE *logFile;
    char logFilePath[128];
    char buffer[1024];
    cJSON *jsonLog = NULL;
    cJSON *jsonArray = cJSON_CreateArray();

    // 날짜에 맞는 로그 파일 경로 생성
    snprintf(logFilePath, sizeof(logFilePath), "%s%s_BitErrorLog.json", LOG_FILE_DIR, date);

    // 로그 파일 열기
    logFile = fopen(logFilePath, "r");
    if (logFile == NULL) {
        perror("Unable to open log file");
        return NULL;
    }

    // 파일에서 한 줄씩 읽어 cJSON 객체로 변환
    while (fgets(buffer, sizeof(buffer), logFile)) {
        jsonLog = cJSON_Parse(buffer);
        if (jsonLog == NULL) {
            perror("Error parsing JSON from log file");
            fclose(logFile);
            return NULL;
        }
        cJSON_AddItemToArray(jsonArray, jsonLog);
    }

    // 파일 닫기
    fclose(logFile);

    return jsonArray;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s [start | read YYYYMMDD]\n", argv[0]);
        return -1;
    }

    if (strcmp(argv[1], "start") == 0) {
        if (argc == 2) {
            uint32_t mItemStates[18] = {1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0};
            //checkBitAll(mItemStates); 
            uint32_t mtype = 3;  // continuous 테스트를 나타내는 값
            WriteBitErrorData(mItemStates, mtype);
        } else {
            printf("Invalid command. Usage: %s start\n", argv[0]);
            return -1;
        }
    } else if (strcmp(argv[1], "read") == 0) {
        if (argc == 3) {
            ReadBitErrorLog(argv[2]);
        } else {
            printf("Invalid command. Usage: %s read YYYYMMDD\n", argv[0]);
            return -1;
        }
    } else {
        printf("Invalid command. Usage: %s [start | read YYYYMMDD]\n", argv[0]);
        return -1;
    }

    return 0;
}



