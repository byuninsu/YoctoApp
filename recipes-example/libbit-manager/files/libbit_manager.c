#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <arpa/inet.h>
#include <time.h>
#include <time.h>
#include <linux/nvme_ioctl.h>
#include "cJSON.h"
#include "libbit_manager.h"
#include "usb_control.h"
#include "stm32_control.h"
#include "rs232_control.h"
#include "push_button.h"
#include "nvram_control.h"
#include "led_control.h"
#include "ethernet_control.h"
#include "discrete_in.h"
#include "optic_control.h"

#define LOG_FILE_DIR "/dev/dataSSD"
#define LOG_FILE_NAME "BitErrorLog.json"
#define LOG_BUFFER_SIZE 4096
#define SMART_LOG_SIZE 202
#define NVME_DEVICE_DATA "/dev/nvme0n1"
#define NVME_DEVICE_OS "/dev/nvme1n1"


char iface[20] = {0};
uint32_t powerOnBitResult = 0; 
uint32_t ContinuousBitResult = 0; 
uint32_t initiatedBitResult = 0; 

const char* itemNames[] = {
    "GPU module",
    "SSD (Data store)",
    "GPIO Expander",
    "Ethernet MAC/PHY",
    "NVRAM",
    "Discrete Input",
    "Discrete Output",
    "RS232 Transceiver",
    "SSD (Boot OS)",
    "10G Ethernet Switch",
    "Optic Transceiver",
    "Temperature Sensor",
    "Power Monitor"
};

int get_nvme_smart_log() {
    FILE *fp;
    char command[256];
    char buffer[256];
    char key[128], value[128];

    // SMART 데이터 변수 선언
    int critical_warning = 0;
    int temperature = 0;
    int available_spare = 0;
    int available_spare_threshold = 0;
    int percentage_used = 0;
    uint64_t data_units_read = 0;
    uint64_t data_units_written = 0;
    uint64_t host_read_commands = 0;
    uint64_t host_write_commands = 0;
    uint64_t controller_busy_time = 0;
    uint64_t power_cycles = 0;
    uint64_t power_on_hours = 0;
    uint64_t unsafe_shutdowns = 0;
    uint64_t media_errors = 0;
    int warning_temperature_time = 0;
    int critical_temperature_time = 0;
    int temperature_sensor_1 = 0;

    // 명령어 구성
    snprintf(command, sizeof(command), "nvme smart-log %s", NVME_DEVICE_DATA);

    // 명령어 실행
    fp = popen(command, "r");
    if (fp == NULL) {
        perror("Failed to run command");
        return 1;
    }

    // 각 줄을 읽어와서 키와 값을 파싱
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        // 공백을 트리밍
        char *trimmed_value = NULL;
        if (sscanf(buffer, "%[^:]:%[^\n]", key, value) == 2) {
            // 공백 제거
            for (char *p = value; *p == ' '; ++p) {
                trimmed_value = p;
            }
            if (trimmed_value == NULL) {
                trimmed_value = value;
            }

            // 각 키에 따른 값을 변수에 저장
            if (strcmp(key, "critical_warning") == 0) {
                critical_warning = atoi(trimmed_value);
            } else if (strcmp(key, "temperature") == 0) {
                temperature = atoi(trimmed_value);
            } else if (strcmp(key, "available_spare") == 0) {
                available_spare = atoi(trimmed_value);
            } else if (strcmp(key, "available_spare_threshold") == 0) {
                available_spare_threshold = atoi(trimmed_value);
            } else if (strcmp(key, "percentage_used") == 0) {
                percentage_used = atoi(trimmed_value);
            } else if (strcmp(key, "data_units_read") == 0) {
                data_units_read = strtoull(trimmed_value, NULL, 10);
            } else if (strcmp(key, "data_units_written") == 0) {
                data_units_written = strtoull(trimmed_value, NULL, 10);
            } else if (strcmp(key, "host_read_commands") == 0) {
                host_read_commands = strtoull(trimmed_value, NULL, 10);
            } else if (strcmp(key, "host_write_commands") == 0) {
                host_write_commands = strtoull(trimmed_value, NULL, 10);
            } else if (strcmp(key, "controller_busy_time") == 0) {
                controller_busy_time = strtoull(trimmed_value, NULL, 10);
            } else if (strcmp(key, "power_cycles") == 0) {
                power_cycles = strtoull(trimmed_value, NULL, 10);
            } else if (strcmp(key, "power_on_hours") == 0) {
                power_on_hours = strtoull(trimmed_value, NULL, 10);
            } else if (strcmp(key, "unsafe_shutdowns") == 0) {
                unsafe_shutdowns = strtoull(trimmed_value, NULL, 10);
            } else if (strcmp(key, "media_errors") == 0) {
                media_errors = strtoull(trimmed_value, NULL, 10);
            } else if (strcmp(key, "Warning Temperature Time") == 0) {
                warning_temperature_time = atoi(trimmed_value);
            } else if (strcmp(key, "Critical Composite Temperature Time") == 0) {
                critical_temperature_time = atoi(trimmed_value);
            } else if (strcmp(key, "Temperature Sensor 1") == 0) {
                temperature_sensor_1 = atoi(trimmed_value);
            }
        }
    }

    // 프로세스 종료
    pclose(fp);

    // 최종 출력
    printf("NVMe SMART Log for device: %s\n", NVME_DEVICE_DATA);
    printf("Critical Warning: %d\n", critical_warning);
    printf("Temperature: %d C\n", temperature);
    printf("Available Spare: %d%%\n", available_spare);
    printf("Available Spare Threshold: %d%%\n", available_spare_threshold);
    printf("Percentage Used: %d%%\n", percentage_used);
    printf("Data Units Read: %llu\n", data_units_read);
    printf("Data Units Written: %llu\n", data_units_written);
    printf("Host Read Commands: %llu\n", host_read_commands);
    printf("Host Write Commands: %llu\n", host_write_commands);
    printf("Controller Busy Time: %llu\n", controller_busy_time);
    printf("Power Cycles: %llu\n", power_cycles);
    printf("Power On Hours: %llu\n", power_on_hours);
    printf("Unsafe Shutdowns: %llu\n", unsafe_shutdowns);
    printf("Media Errors: %llu\n", media_errors);
    printf("Warning Temperature Time: %d\n", warning_temperature_time);
    printf("Critical Composite Temperature Time: %d\n", critical_temperature_time);
    printf("Temperature Sensor 1: %d C\n", temperature_sensor_1);

    return 0;
}

size_t GetItemCount() {
    return sizeof(itemNames) / sizeof(itemNames[0]);
}

const char* GetItemName(uint32_t mItem) {
    if (mItem < GetItemCount()) {
        return itemNames[mItem];
    } else {
        return "Unknown";
    }
}

int  WriteBitErrorData(uint32_t bitStatus, uint32_t mtype) {
    FILE *logFile;
    time_t now;
    struct tm *timeinfo;
    char timestamp[20];
    char logFilePath[128];

    // Get the current time
    time(&now);
    timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Set the log file path (fixed name)
    snprintf(logFilePath, sizeof(logFilePath), "%s%s", LOG_FILE_DIR, LOG_FILE_NAME);

    // Open the log file for appending
    logFile = fopen(logFilePath, "a");
    if (logFile == NULL) {
        perror("Unable to open log file");
        return 1;
    }

    // Start writing the JSON object
    fprintf(logFile, "{");

    // Iterate through each bit of bitStatus using the dynamic item count
    size_t itemCount = GetItemCount();
    for (uint32_t i = 0; i < itemCount; i++) {
        uint32_t bitValue = (bitStatus >> i) & 1;  // Extract the bit value (0 or 1)
        fprintf(logFile, "\"%s\":%u", GetItemName(i), bitValue);
        if (i < itemCount - 1) {
            fprintf(logFile, ",");
        }
    }

    // Write mtype and timestamp
    fprintf(logFile, ",\"mtype\":%u,\"timestamp\":\"%s\"}\n", mtype, timestamp);

    // Close the log file
    fclose(logFile);

    return 0;
}

cJSON*  ReadBitErrorLog(void) {
    FILE *logFile;
    char logFilePath[128];
    char buffer[1024];
    cJSON *jsonLog = NULL;
    cJSON *jsonArray = cJSON_CreateArray();

    // 고정된 로그 파일 경로 생성
    snprintf(logFilePath, sizeof(logFilePath), "%s%s", LOG_FILE_DIR, LOG_FILE_NAME);

    // 로그 파일 열기
    logFile = fopen(logFilePath, "r");
    if (logFile == NULL) {
        perror("Unable to open log file");
        cJSON_Delete(jsonArray);  // 메모리 누수 방지를 위해 jsonArray 삭제
        return NULL;
    }

    // 파일에서 한 줄씩 읽어 cJSON 객체로 변환
    while (fgets(buffer, sizeof(buffer), logFile)) {
        jsonLog = cJSON_Parse(buffer);
        if (jsonLog == NULL) {
            perror("Error parsing JSON from log file");
            fclose(logFile);
            cJSON_Delete(jsonArray);  // 메모리 누수 방지를 위해 jsonArray 삭제
            return NULL;
        }
        cJSON_AddItemToArray(jsonArray, jsonLog);
    }

    // 파일 닫기
    fclose(logFile);

    return jsonArray;
}


int check_ssd(const char *ssd_path) {
    int status;

    // 경로를 구성하여 실제 디바이스 경로로 변환
    char device_path[512];

    if (strcmp(ssd_path, "os") == 0) {
        snprintf(device_path, sizeof(device_path), "/dev/osSSD/testfile");
    } else if (strcmp(ssd_path, "data") == 0) {
        snprintf(device_path, sizeof(device_path), "/dev/dataSSD/testfile");
    } else {
        printf("Error: Invalid SSD path. Use 'os' or 'data'.\n");
        return 1;
    }

    // 쓰기 테스트: 1MB 파일 생성
    printf("Creating a 1MB test file on %s...\n", device_path);
    char create_cmd[512];
    snprintf(create_cmd, sizeof(create_cmd), "dd if=/dev/zero of=%s bs=1M count=1", device_path);
    status = system(create_cmd);

    if (status != 0) {
        printf("Error: Failed to create test file on %s.\n", device_path);
        return 1;
    }

    // 읽기 테스트
    printf("Reading the test file on %s...\n", device_path);
    char read_cmd[512];
    snprintf(read_cmd, sizeof(read_cmd), "dd if=%s of=/dev/null bs=1M", device_path);
    status = system(read_cmd);

    if (status != 0) {
        printf("Error: Failed to read test file on %s.\n", device_path);
        return 1;
    }

    // 파일 삭제
    printf("Deleting the test file on %s...\n", device_path);
    char delete_cmd[512];
    snprintf(delete_cmd, sizeof(delete_cmd), "rm %s", device_path);
    status = system(delete_cmd);

    if (status != 0) {
        printf("Error: Failed to delete test file on %s.\n", device_path);
        return 1;
    }

    printf("SSD check on %s completed successfully.\n", device_path);
    return 0;
}

int check_gpio_expander() {
    int status;
    uint8_t readConfValue0 = 0;
    uint8_t readConfValue1 = 0;
    char command[512];
    uint32_t result = 0;


    // led-control Expander 0 setting

    status = setGpioConf(0, 0x00);
    if (status != 0) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    }

    status = setGpioConf(1, 0x00);
    if (status != 0) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    }

    result = getConfState(0, &readConfValue0);
    if (readConfValue0 == 1) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    } 

    result = getConfState(1, &readConfValue1);
    if (readConfValue1 == 1) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    } 

    if(readConfValue0 == 0 && readConfValue1 == 0 ) {
        printf(" GPIO Expander test passed.");
    }

    // Restore to original state after testing
    status = setGpioConf(0, 0xFF);
    if (status != 0) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    }

    status = setGpioConf(1, 0xFF);
    if (status != 0) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    }

    return 0;
}

int check_discrete_out() {
    int status;
    uint8_t readConfValue0 = 0;
    uint8_t readConfValue1 = 0;
    uint32_t result = 0;
    char command[512];


    // led-control Expander 0 setting

    status = setDiscreteConf(0, 0x00);
    if (status != 0) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    }

    status = setDiscreteConf(1, 0x00);
    if (status != 0) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    }

    result = getDiscreteConf(0, &readConfValue0);
    if (result == 1) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    } 

    result = getDiscreteConf(1, &readConfValue1);
    if (result == 1) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    } 

    if(readConfValue0 == 0 && readConfValue1 == 0 ) {
        printf(" GPIO Expander test passed.");
    }

    // Restore to original state after testing
    status = setGpioConf(0, 0xFF);
    if (status != 0) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    }

    status = setGpioConf(1, 0xFF);
    if (status != 0) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    }

    return 0;
}

int checkDiscrete_in() {
    char writeCommand[512];
    char readCommand[512];
    char buffer[512];
    uint8_t writeValue = 0x02;  // SENSE 모드에 쓸 값
    uint8_t readSenseValue = 0;

    // SENSE 모드로 값 1을 쓰는 명령어 실행

    WriteProgramSenseBanks(writeValue);

    readSenseValue = ReadProgramSenseBanks();

    if(readSenseValue == 1){
        printf("Discrete_in test failed: could not read value.\n");
        return 1;
    }

    if(writeValue == readSenseValue) {
        printf("Discrete_in test passed. Value is 0x%02X\n", readSenseValue);
        return 0;  // 값이 일치하면 0 리턴
    }

    // 값을 찾지 못한 경우
    printf("Discrete_in test failed: not match writeValue, readSenseValue .\n");
    return 1;
}

int checkEthernet() {
    int sock;
    struct ifreq ifr;

    char mac_addr[18];
    struct ethtool_value edata;

    // 네트워크 인터페이스를 위한 소켓 생성
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    // eth0과 eth1 인터페이스를 순차적으로 확인
    for (int i = 0; i < 2; i++) {
        snprintf(iface, sizeof(iface), "eth%d", i);
        strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

        // MAC 주소 가져오기
        if (ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
            perror("ioctl SIOCGIFHWADDR");
            close(sock);
            return -1;
        }

        // MAC 주소를 문자열로 변환
        snprintf(mac_addr, sizeof(mac_addr), "%02x:%02x:%02x:%02x:%02x:%02x",
                 (unsigned char)ifr.ifr_hwaddr.sa_data[0],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[1],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[2],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[3],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[4],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[5]);

        // MAC 주소의 첫 바이트가 48(0x30)로 시작하는지 확인
        if ((unsigned char)ifr.ifr_hwaddr.sa_data[0] == 0x48) {
            printf("Interface %s has a MAC address starting with 48: %s\n", iface, mac_addr);

            // ethtool을 사용하여 링크 상태 확인
            edata.cmd = ETHTOOL_GLINK;
            ifr.ifr_data = (caddr_t)&edata;

            if (ioctl(sock, SIOCETHTOOL, &ifr) == -1) {
                perror("ioctl SIOCETHTOOL");
                close(sock);
                return -1;
            }

            // edata.data가 1이면 링크가 감지됨
            if (edata.data) {
                printf("Link detected on %s\n", iface);
                close(sock);
                return 0; // 링크 감지됨, 정상 작동
            } else {
                printf("No link detected on %s\n", iface);
            }
        }
    }
    
    close(sock);
    return 1; // 링크 감지되지 않음, 비정상 작동
}

int checkGPU() {
    // 'deviceQuery' 프로그램의 절대 경로 지정
    const char *cmd = "/usr/bin/cuda-samples/deviceQuery";
    char buffer[512];
    FILE *pipe;

    // 시스템 명령어 실행 및 출력 읽기
    pipe = popen(cmd, "r");
    if (pipe == NULL) {
        printf("Failed to run deviceQuery.\n");
        return -1;
    }

    // 명령어 출력 중 "Result = PASS" 문자열 검색
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        if (strstr(buffer, "Result = PASS")) {
            printf("GPU is functioning correctly.\n");
            pclose(pipe);
            return 0;  // GPU가 정상적으로 작동
        }
    }

    // "Result = PASS"를 찾지 못한 경우
    printf("GPU check failed. 'Result = PASS' not found.\n");
    pclose(pipe);
    return -1;
}

int checkNvram() {
    int address = 12;        // NVRAM 주소
    srand(time(NULL));       // 랜덤 시드 설정
    int writeValue = rand() & 0xFF;   // 0x00 - 0xFF 범위의 랜덤 값 생성
    char command[512];
    char buffer[512];
    uint32_t readValue = 1;

    // NVRAM에 값을 쓰는 명령어 실행

    if(WriteSystemLogTest(address, writeValue) == 1) {
        printf("Failed to write to NVRAM.\n");
        return 1;
    } 

    readValue = ReadSystemLog(address);

    if( readValue == 1) {
        printf("Failed to write to NVRAM.\n");
        return 1;
    } 

    if (readValue == writeValue) {
        printf("NVRAM test passed.\n");
        return 0;  // 값이 일치하면 0 리턴
    } else {
        printf("NVRAM test failed: read 0x%x, expected 0x%x.\n", readValue, writeValue);
        return 1;
    }

}

int checkRs232() {
    char buffer[512];

    if(ActivateRS232() == 0){
        if(DeactivateRS232() == 0){
            printf("RS232 check passed.\n");
            return 0;
        } 
    }
    return 1;
}

int checkEthernetSwitch() {
    char command[512];
    int status = 0;
    int port = 1;
    uint32_t result = 0;

    if (iface[0] == '\0') {
        checkEthernet();
    }

    setEthernetPort(port, status);
    result = getEthernetPort(port);

    if(result == 0x0078) {
        printf("ethernet port 1 disable success");
    }

    status = 1;

    setEthernetPort(port, status);
    result = getEthernetPort(port);

    if(result == 0x007f) {
        printf("ethernet port 1 enable success");
    }

    printf("Ethernet switch port check completed successfully.\n");
    return 0;
}

int checkTempSensor() {
    char buffer[512];
    float temperature = 0.0;

    // stm32-control temp
    temperature = getTempSensorValue();
    
    if(temperature == -1) {
        printf("Error: Failed to stm32-control temp command.\n");
        return 1;
    } else {
        printf("Temperature sensor check passed. Temperature: %.2f\n", temperature);
        return 0;
    }

}

int checkPowerMonitor() {
    char buffer[512];
    float voltage = 0.0;
    float current = 0.0;

    // 전압 값을 읽기 위해 stm32-control vol 명령어 실행

    voltage = getVoltageValue();
    
    if(voltage == -1) {
        printf("Error: Failed to stm32-control voltage command.\n");
        return 1;
    } else {
        printf("PowerMonitor check passed. voltage: %.2f\n", voltage);
        return 0;
    }

    current = getCurrentValue();
    
    if(voltage == -1) {
        printf("Error: Failed to stm32-control current command.\n");
        return 1;
    } else {
        printf("PowerMonitor check passed. current: %.2f\n", current);
        return 0;
    }

    // 전압과 전류가 모두 정상 범위임을 확인
    printf("Power monitor check passed. Voltage: %.2f, Current: %.2f\n", voltage, current);
    return 0;
}

int checkUsb(void){
    if(ActivateUSB()){
        printf("ActivateUSB Failed!");
        return 1;
    } else {
        printf("ActivateUSB success!");
    }

    if(DeactivateUSB()){
        printf("ActivateUSB Failed!");
        return 1;
    } else {
        printf("ActivateUSB success!");
    }

    printf("checkUsb passed !");
    return 0;
}

int checkOptic(void){

    uint32_t result = getOpticTestRegister();

    if(result == 0x10){
        printf("checkOptic passed !");
        return 0;
    } else {
        return 1;
    }
    
}

void  RequestBit(uint32_t mtype) {

    uint32_t bitStatus = 0;

    // 각 상태를 순서대로 비트에 저장
    bitStatus |= (checkGPU() << 0);                     // GPU 상태 (비트 0)
    bitStatus |= (check_ssd("data") << 1);              // 데이터 SSD 상태 (비트 1)
    bitStatus |= (check_gpio_expander() << 2);          // GPIO Expander 상태 (비트 2)
    bitStatus |= (checkEthernet() << 3);                // Ethernet 상태 (비트 3)
    bitStatus |= (checkNvram() << 4);                   // NVRAM 상태 (비트 4)
    bitStatus |= (checkDiscrete_in() << 5);             // Discrete In 상태 (비트 5)
    bitStatus |= (check_discrete_out() << 6);           // Discrete Out 상태 (비트 6)
    bitStatus |= (checkRs232() << 7);                   // RS232 상태 (비트 7)
    bitStatus |= (check_ssd("os") << 8);                // OS SSD 상태 (비트 8)
    bitStatus |= (checkEthernetSwitch() << 9);          // Ethernet Switch 상태 (비트 9)
    bitStatus |= (checkOptic() << 10);                  // Optic Transceiver 상태 (비트 10)
    bitStatus |= (checkTempSensor() << 11);             // 온도 센서 상태 (비트 11)
    bitStatus |= (checkPowerMonitor() << 12);           // 전력 모니터 상태 (비트 12)


    bitStatus |= (1 << 31); // flag setting


    // 각 상태를 출력
    printf("GPU module: %u\n", (bitStatus >> 0) & 1);
    printf("SSD (Data store): %u\n", (bitStatus >> 1) & 1);
    printf("GPIO Expander: %u\n", (bitStatus >> 2) & 1);
    printf("Ethernet MAC/PHY: %u\n", (bitStatus >> 3) & 1);
    printf("NVRAM: %u\n", (bitStatus >> 4) & 1);
    printf("Discrete Input: %u\n", (bitStatus >> 5) & 1);
    printf("Discrete Output: %u\n", (bitStatus >> 6) & 1);
    printf("RS232 Transceiver: %u\n", (bitStatus >> 7) & 1);
    printf("SSD (Boot OS): %u\n", (bitStatus >> 8) & 1);
    printf("10G Ethernet Switch: %u\n", (bitStatus >> 9) & 1);
    printf("Optic Transceiver: %u\n", (bitStatus >> 10) & 1);
    printf("Temperature Sensor: %u\n", (bitStatus >> 11) & 1);
    printf("Power Monitor: %u\n", (bitStatus >> 12) & 1);


    if((bitStatus >> 31) & 1 ){
        printf("bit error occurred, bitStatus: 0x%08X, mtype: 0x%08X.\n", bitStatus, mtype);
        WriteBitErrorData(bitStatus, mtype);
    }

    WriteBitResult(mtype, bitStatus);


}

uint32_t readtBitResult(uint32_t type){

    return ReadBitResult(type);
}


// int main(int argc, char *argv[]) {
//     if (argc < 2) {
//         printf("Usage: %s <gpio|ssd|discrete> <os|data|in|out>\n", argv[0]);
//         return 1;
//     }

//     const char *option = argv[1];

//     int result = 0;

//     if (strcmp(option, "gpio") == 0) {
//         result = check_gpio_expander();
//     } else if (strcmp(option, "ssd") == 0) {
//         char *option = argv[2];
//         if (strcmp(option, "os") == 0 ){
//             result = check_ssd("os");
//         }else if (strcmp(option, "data") == 0){
//             result = check_ssd("data");
//         }
//     } else if (strcmp(option, "discrete") == 0) {
//         char *option = argv[2];
//         if (strcmp(option, "out") == 0 ){
//             result = check_discrete_out();
//         }else if (strcmp(option, "in") == 0){
//             checkDiscrete_in();
//         }
//     } else if (strcmp(option, "ethernet") == 0){
//         checkEthernet();
//     } else if (strcmp(option, "gpu") == 0){
//         checkGPU();
//     } else if (strcmp(option, "nvram") == 0){
//         checkNvram();
//     } else if (strcmp(option, "rs232") == 0){
//         checkRs232();
//     } else if (strcmp(option, "switch") == 0){
//         checkEthernetSwitch();
//     } else if (strcmp(option, "temp") == 0){
//         checkTempSensor();
//     } else if (strcmp(option, "power") == 0){
//         checkPowerMonitor();
//     } else if (strcmp(option, "usb") == 0){
//         checkUsb();
//     } else if (strcmp(option, "usb") == 0){
//         checkOptic();
//     } else if (strcmp(option, "all") == 0){
//         uint32_t type = 4;
//         RequestBit(type);
//     } else {
//         printf("Invalid option. Use 'gpio', 'ssd', or 'discrete'.\n");
//         return 1;
//     }
//     if (result == 0) {
//         printf("Check completed successfully.\n");
//     } else {
//         printf("Check failed.\n");
//     }

//     return result;
// }
