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
#include <linux/nvme_ioctl.h>
#include <sys/mount.h>
#include "cJSON.h"
#include "bit_manager.h"
#include "usb_control.h"
#include "stm32_control.h"
#include "rs232_control.h"
#include "push_button.h"
#include "nvram_control.h"
#include "led_control.h"
#include "ethernet_control.h"
#include "discrete_in.h"
#include "optic_control.h"

#define LOG_FILE_DIR "/mnt/dataSSD/"
#define LOG_FILE_NAME "BitErrorLog.json"
#define LOG_BUFFER_SIZE 4096
#define SMART_LOG_SIZE 202
#define NVME_DEVICE_DATA "/dev/nvme0n1"
#define NVME_DEVICE_OS "/dev/nvme1n1"
#define NVME_MOUNT_POINT_DATA "/mnt/dataSSD"
#define USB1_PATH "/sys/bus/usb/devices/usb1/authorized"
#define USB2_PATH "/sys/bus/usb/devices/usb2/authorized"

char iface[20] = {0};
uint32_t powerOnBitResult = 0; 
uint32_t ContinuousBitResult = 0; 
uint32_t initiatedBitResult = 0; 

// 고정된 하드웨어 정보
const struct hwCompatInfo myInfo = {
    .supplier_part_no   = "22-00155668",
    .supplier_serial_no = "UB-AAIS001",
    .sw_part_number     = "HSC-OESG-IRIS"
};

// nvme 장치로부터 정보 읽기 함수
int read_nvme_info(const char* nvme_dev, char* model_no, size_t model_size, char* serial_no, size_t serial_size) {
    char cmd[128], line[512];

    snprintf(cmd, sizeof(cmd), "nvme list | grep '%s'", nvme_dev);

    FILE* fp = popen(cmd, "r");
    if (!fp) return -1;

    if (!fgets(line, sizeof(line), fp)) {
        pclose(fp);
        return -1;
    }
    pclose(fp);

    // nvme 출력에서 Node, SN, Model 만 파싱 (다른 항목 무시)
    char node[32], sn[64], model[128];
    if (sscanf(line, "%31s %63s %127s", node, sn, model) != 3)
        return -1;

    strncpy(model_no, model, model_size);
    model_no[model_size - 1] = '\0';
    strncpy(serial_no, sn, serial_size);
    serial_no[serial_size - 1] = '\0';

    return 0;
}

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
    "1G Ethernet Switch",
    "Optic Transceiver",
    "Temperature Sensor",
    "Power Monitor",
    "Holdup Module",
    "STM32 Status",
    "Lan 7800 Status",
    "AC/DC Status",
    "DC/DC Status",
    "USB A Status",
    "USB C Status"
};

const char* cBitItemNames[] = {
    "GPIO Expander",
    "Ethernet PHY",
    "NVRAM",
    "Discrete Input",
    "Discrete Output",
    "RS232 Transceiver",
    "Ethernet Switch",
    "Temperature Sensor",
    "Power Monitor",
    "Holdup Module",
    "STM32 Status"
};


size_t GetItemCount() {
    return sizeof(itemNames) / sizeof(itemNames[0]);
}

size_t GetItemCountofCBIT() {
    return sizeof(cBitItemNames) / sizeof(cBitItemNames[0]);
}

const char* GetItemName(uint32_t mItem) {
    if (mItem < GetItemCount()) {
        return itemNames[mItem];
    } else {
        return "Unknown";
    }
}

uint32_t GetLastSequence(const char *filePath) {
    FILE *file = fopen(filePath, "r");
    if (file == NULL) {
        return 1; // 첫 파일 생성 시 Sequence는 1
    }

    char buffer[1024];
    char *lastLine = NULL;

    // 파일 끝까지 읽어 마지막 줄 추출
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        lastLine = strdup(buffer); // 가장 마지막 줄을 복사
    }
    fclose(file);

    if (lastLine == NULL) {
        return 1; // 빈 파일이면 Sequence는 1
    }

    // 마지막 줄에서 Sequence 추출
    char *seqStr = strstr(lastLine, "\"Sequence\":");
    uint32_t lastSequence = 1;
    if (seqStr) {
        sscanf(seqStr, "\"Sequence\":%u,", &lastSequence);
        lastSequence++; // 다음 시퀀스 계산
    }
    free(lastLine);
    return lastSequence;
}

int  WriteBitErrorData(uint32_t bitStatus, uint32_t mtype) {
    FILE *logFile;
    time_t now;
    struct tm *timeinfo;
    char timestamp[20];
    char logFilePath[128];
    size_t itemCount;

    // Get the current time
    time(&now);
    timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Set the log file path (fixed name)
    snprintf(logFilePath, sizeof(logFilePath), "%s%s", LOG_FILE_DIR, LOG_FILE_NAME);

    // Get the last sequence
    uint32_t sequence = GetLastSequence(logFilePath);

    // Open the log file for appending
    logFile = fopen(logFilePath, "a");
    if (logFile == NULL) {
        perror("Unable to open log file");
        return 1;
    }

    // Start writing the JSON object
    fprintf(logFile, "{");

    // Write the Sequence
    fprintf(logFile, "\"Sequence\":%u,", sequence);

    // Iterate through each bit of bitStatus using the dynamic item count


    if(mtype == 3){
        itemCount = GetItemCountofCBIT();
    } else {
        itemCount = GetItemCount();
    }
    
    for (uint32_t i = 0; i < itemCount; i++) {
        uint32_t bitValue = (bitStatus >> i) & 1;  // Extract the bit value (0 or 1)
        fprintf(logFile, "\"%s\":%u", GetItemName(i), bitValue);
        if (i < itemCount - 1) {
            fprintf(logFile, ",");
        }
    }

    // Write mtype and timestamp
    fprintf(logFile, ",\"mtype\":%u,\"timestamp\":\"%s\"}\n", mtype, timestamp);

    // 캐시 플러시 및 디스크 동기화
    fflush(logFile);  // 캐시 플러시
    fsync(fileno(logFile)); // 디스크 동기화

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

    // 경로를 구성하여 마운트 경로로 변환
    char device_path[512];

    if (strcmp(ssd_path, "os") == 0) {
        snprintf(device_path, sizeof(device_path), "/mnt/osSSD/testfile");
    } else if (strcmp(ssd_path, "data") == 0) {
        snprintf(device_path, sizeof(device_path), "/mnt/dataSSD/testfile");
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

SSD_Status getSSDSmartLog(uint8_t ssd_type) {
    SSD_Status log = {0}; // 구조체 초기화
    FILE *fp = NULL;      // 파일 포인터 초기화

    // NVMe 디바이스 선택
    if (ssd_type == 2) {
        //os
        fp = popen("nvme smart-log /dev/nvme1", "r");
    } else if (ssd_type == 3) {
        //data
        fp = popen("nvme smart-log /dev/nvme0", "r");
    }

    if (fp == NULL) {
        perror("Failed to run nvme command");
        return log; // 실패 시 빈 구조체 반환
    }

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        sscanf(line, "temperature                             : %hhu C", &log.temperature);
        sscanf(line, "available_spare                         : %hhu%%", &log.available_spare);
        sscanf(line, "available_spare_threshold               : %hhu%%", &log.available_spare_threshold);
        sscanf(line, "percentage_used                         : %hhu%%", &log.percentage_used);
        sscanf(line, "data_units_read                         : %u", &log.data_units_read);
        sscanf(line, "data_units_written                      : %u", &log.data_units_written);
        sscanf(line, "host_read_commands                      : %u", &log.host_read_commands);
        sscanf(line, "host_write_commands                     : %u", &log.host_write_commands);
        sscanf(line, "controller_busy_time                    : %u", &log.controller_busy_time);
        sscanf(line, "power_cycles                            : %u", &log.power_cycles);
        sscanf(line, "power_on_hours                          : %u", &log.power_on_hours);
        sscanf(line, "unsafe_shutdowns                        : %u", &log.unsafe_shutdowns);
        sscanf(line, "media_and_data_errors                   : %u", &log.media_and_data_errors);
        sscanf(line, "num_err_log_entries                     : %u", &log.error_log_entries);
        sscanf(line, "Warning Temperature Time                : %u", &log.warning_temp_time);
        sscanf(line, "Critical Composite Temperature Time     : %u", &log.critical_temp_time);
    }

    pclose(fp);
    return log;
}

uint8_t initializeDataSSD(void) {
    char command[256];
    uint8_t result = 0; // 성공 여부를 저장

    printf("Starting SSD initialization...\n");

    // 1. 마운트 해제
    if (umount(NVME_MOUNT_POINT_DATA) == 0) {
        printf("Unmounted %s successfully.\n", NVME_MOUNT_POINT_DATA);
    } else {
        perror("Unmount failed");
        result = 1;
    }

    // 2. 디스크 초기화 (wipefs 사용)
    snprintf(command, sizeof(command), "wipefs -a %s", NVME_DEVICE_DATA);
    if (system(command) == 0) {
        printf("Disk %s wiped successfully.\n", NVME_DEVICE_DATA);
    } else {
        perror("Wipe failed");
        result = 1;
    }

    // 3. ext4 파일 시스템 생성
    snprintf(command, sizeof(command), "mkfs.ext4 %s", NVME_DEVICE_DATA);
    if (system(command) == 0) {
        printf("Formatted %s as ext4 successfully.\n", NVME_DEVICE_DATA);
    } else {
        perror("Format failed");
        result = 1;
    }

    // 4. 디스크 다시 마운트
    if (mount(NVME_DEVICE_DATA, NVME_MOUNT_POINT_DATA, "ext4", 0, NULL) == 0) {
        printf("Mounted %s to %s successfully.\n", NVME_DEVICE_DATA, NVME_MOUNT_POINT_DATA);
    } else {
        perror("Mount failed");
        result = 1;
    }

    if (result == 0) {
        printf("SSD initialization completed successfully.\n");
    } else {
        printf("SSD initialization failed.\n");
    }

    return result;
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
    int writeValue = rand() & 0xFFFFFFFF;   // 0x00 - 0xFFFFFFFF 범위의 랜덤 값 생성
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

    if(DeactivateRS232() == 0){
        if(ActivateRS232() == 0){
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

uint8_t CheckHwCompatInfo(void) {
    struct hwCompatInfo nvramInfo;
    struct hwCompatInfo currentInfo = myInfo;

    // NVRAM 정보 읽기
    if (ReadHwCompatInfoFromNVRAM(&nvramInfo) != 0) {
        printf("Error reading NVRAM\n");
        return 1;
    }

    // SSD0 정보 읽기
    if (read_nvme_info("/dev/nvme1n1", currentInfo.ssd0_model_no, sizeof(currentInfo.ssd0_model_no),
                                      currentInfo.ssd0_serial_no, sizeof(currentInfo.ssd0_serial_no)) != 0) {
        printf("Error reading nvme0n1 info\n");
        return 1;
    }

    // SSD1 정보 읽기
    if (read_nvme_info("/dev/nvme0n1", currentInfo.ssd1_model_no, sizeof(currentInfo.ssd1_model_no),
                                      currentInfo.ssd1_serial_no, sizeof(currentInfo.ssd1_serial_no)) != 0) {
        printf("Error reading nvme1n1 info\n");
        return 1;
    }

        printf("SSD CurrentInfo:\n");
        printf(" Supplier Part No  : %s\n", currentInfo.supplier_part_no);
        printf(" Supplier Serial No: %s\n", currentInfo.supplier_serial_no);
        printf(" SSD0 Model No     : %s\n", currentInfo.ssd0_model_no);
        printf(" SSD0 Serial No    : %s\n", currentInfo.ssd0_serial_no);
        printf(" SSD1 Model No     : %s\n", currentInfo.ssd1_model_no);
        printf(" SSD1 Serial No    : %s\n", currentInfo.ssd1_serial_no);
        printf(" SW Part Number    : %s\n", currentInfo.sw_part_number);

    // 7개 항목 모두 비교 (supplier 정보, sw_part_number는 고정값 비교)
    if (strncmp(currentInfo.supplier_part_no, nvramInfo.supplier_part_no, sizeof(currentInfo.supplier_part_no)) != 0 ||
        strncmp(currentInfo.supplier_serial_no, nvramInfo.supplier_serial_no, sizeof(currentInfo.supplier_serial_no)) != 0 ||
        strncmp(currentInfo.sw_part_number, nvramInfo.sw_part_number, sizeof(currentInfo.sw_part_number)) != 0 ||
        strncmp(currentInfo.ssd0_model_no, nvramInfo.ssd0_model_no, sizeof(currentInfo.ssd0_model_no)) != 0 ||
        strncmp(currentInfo.ssd0_serial_no, nvramInfo.ssd0_serial_no, sizeof(currentInfo.ssd0_serial_no)) != 0 ||
        strncmp(currentInfo.ssd1_model_no, nvramInfo.ssd1_model_no, sizeof(currentInfo.ssd1_model_no)) != 0 ||
        strncmp(currentInfo.ssd1_serial_no, nvramInfo.ssd1_serial_no, sizeof(currentInfo.ssd1_serial_no)) != 0) {
        return 1; // 하나라도 불일치 시 1 반환
    }

    return 0; // 모두 일치하면 0 반환
}

int checkHoldupModule(void){
    // STM32로 HoldupModule 에서 데이터 읽기
    //1이 정상
    uint8_t holdupPFvalue = sendRequestHoldupPF();
    //0 이 정상
    uint8_t holdupCCvalue = sendRequestHoldupCC();

    if ( holdupPFvalue == 1 && holdupCCvalue == 0) {
        return 0;
    } else {
        return 1;
    }
}

int checkStm32Status(void){
    uint8_t stm32Status = sendStm32Status();

    if ( stm32Status == 1 ) {
        return 0;
    } else {
        return 1;
    }
}

int checkPowerStatus(void){
    uint8_t powerStatus = sendPowerStatus();

    uint8_t overvoltage_mask = (1 << 0);
    uint8_t undervoltage_mask = (1 << 1);
    uint8_t overcurrent_mask = (1 << 2);

    if ( powerStatus & (overvoltage_mask | undervoltage_mask | overcurrent_mask ) ) {
        return 1;
    } else {
        return 0;
    }

}

int checkLan7800(void){
    char* otherIface = checkEthernetInterface();
    char* iface;

    if(strcmp( otherIface, "eth0") == 0 ){
        iface = "eth1";
    } else {
        iface = "eth0";
    }

    char command[256];
    snprintf(command, sizeof(command), "ip link show %s | grep -q 'state UP'", iface);

    int status = system(command);
    if (status == -1) {
        perror("Failed to execute command");
        return 1;  // 시스템 호출 실패 시 오류 반환
    }

    // system()의 반환값이 0이면  정상 작동
    if (status == 0) {
        printf(" LAN7800 (%s) is OK\n", iface);
        return 0; // 정상
    } else {
        printf(" LAN7800 (%s) is Not OK\n", iface);
        return 1; // 비정상
    }
}

// USB1 상태 확인 함수
int checkUSBc(void) {
    FILE *file = fopen(USB1_PATH, "r");
    if (!file) {
        perror("Failed to open USB1 status file");
        return 1; // 파일 접근 실패 시 비정상 처리
    }

    int authorized;
    if (fscanf(file, "%d", &authorized) != 1) {
        perror("Failed to read USB1 status");
        fclose(file);
        return 1; // 읽기 실패 시 비정상 처리
    }
    fclose(file);

    if (authorized == 1) {
        printf("USB1 is OK ");
        return 0; // 정상
    } else {
        printf("USB1 is Not OK");
        return 1; // 비정상
    }
}

// USB2 상태 확인 함수
int checkUSBa(void) {
    FILE *file = fopen(USB2_PATH, "r");
    if (!file) {
        perror("Failed to open USB2 status file");
        return 1; // 파일 접근 실패 시 비정상 처리
    }

    int authorized;
    if (fscanf(file, "%d", &authorized) != 1) {
        perror("Failed to read USB2 status");
        fclose(file);
        return 1; // 읽기 실패 시 비정상 처리
    }
    fclose(file);

    if (authorized == 1) {
        printf("USB2 정상 ");
        return 0; // 정상
    } else {
        printf("USB2 비정상 ");
        return 1; // 비정상
    }
}


void RequestBit(uint32_t mtype) {
    uint32_t bitStatus = 0;

    // DEBUG: 초기화 메시지 출력
    printf("Starting RequestBit function, mtype = 0x%08X\n", mtype);

    // 상태 체크 (bitStatus에 저장)
    bitStatus |= ((checkGPU() != 0) << 0);                     // GPU 상태
    bitStatus |= ((check_ssd("data") != 0) << 1);              // 데이터 SSD 상태
    bitStatus |= ((check_gpio_expander() != 0) << 2);          // GPIO Expander 상태
    bitStatus |= ((checkEthernet() != 0) << 3);                // Ethernet 상태
    bitStatus |= ((checkNvram() != 0) << 4);                   // NVRAM 상태
    bitStatus |= ((checkDiscrete_in() != 0) << 5);             // Discrete In 상태
    bitStatus |= ((check_discrete_out() != 0) << 6);           // Discrete Out 상태
    bitStatus |= ((checkRs232() != 0) << 7);                   // RS232 상태
    bitStatus |= ((check_ssd("os") != 0) << 8);                // OS SSD 상태
    bitStatus |= ((checkEthernetSwitch() != 0) << 9);          // Ethernet Switch 상태
    bitStatus |= ((checkOptic() != 0) << 10);                  // Optic Transceiver 상태
    bitStatus |= ((checkTempSensor() != 0) << 11);             // 온도 센서 상태
    bitStatus |= ((checkPowerMonitor() != 0) << 12);           // 전력 모니터 상태
    bitStatus |= ((checkHoldupModule() != 0) << 13);           // HoldupModule 상태
    bitStatus |= ((checkStm32Status() != 0) << 14);            // stm32 상태
    bitStatus |= ((checkLan7800() != 0) << 15);                // lan7800 상태
    bitStatus |= ((checkPowerStatus() != 0) << 16);            // ac/dc 상태
    bitStatus |= ((checkPowerStatus() != 0) << 17);            // dc/dc 상태
    bitStatus |= ((checkUSBa() != 0) << 18);                   // usba 상태
    bitStatus |= ((checkUSBc() != 0) << 19);                   // usbc 상태


    // 플래그 비트 설정 (31번 비트)
    bitStatus |= (1 << 31);

    // 결과 출력 (한 번에 출력)
    printf("Test Results:\n");
    printf("GPU = %u\n", (bitStatus >> 0) & 1);
    printf("SSD(Data) = %u\n", (bitStatus >> 1) & 1);
    printf("GPIO Expander = %u\n", (bitStatus >> 2) & 1);
    printf("Ethernet = %u\n", (bitStatus >> 3) & 1);
    printf("NVRAM = %u\n", (bitStatus >> 4) & 1);
    printf("Discrete In = %u\n", (bitStatus >> 5) & 1);
    printf("Discrete Out = %u\n", (bitStatus >> 6) & 1);
    printf("RS232 = %u\n", (bitStatus >> 7) & 1);
    printf("SSD(OS) = %u\n", (bitStatus >> 8) & 1);
    printf("Ethernet Switch = %u\n", (bitStatus >> 9) & 1);
    printf("Optic Transceiver = %u\n", (bitStatus >> 10) & 1);
    printf("Temperature Sensor = %u\n", (bitStatus >> 11) & 1);
    printf("Power Monitor = %u\n", (bitStatus >> 12) & 1);
    printf("Holdup Module = %u\n", (bitStatus >> 13) & 1);
    printf("STM32 Status = %u\n", (bitStatus >> 14) & 1);
    printf("Lan 7800 Status = %u\n", (bitStatus >> 15) & 1);
    printf("AC/DC Status = %u\n", (bitStatus >> 16) & 1);
    printf("DC/DC Status= %u\n", (bitStatus >> 17) & 1);
    printf("USB A Status= %u\n", (bitStatus >> 18) & 1);
    printf("USB C Status = %u\n", (bitStatus >> 19) & 1);

    // 에러 비트 확인
    if (bitStatus & 0x7FFFFFFF) {
        printf("bit error occurred, bitStatus = 0x%08X, mtype = 0x%08X.\n", bitStatus, mtype);
        WriteBitErrorData(bitStatus, mtype);
    }

    // 결과 저장
    WriteBitResult(mtype, bitStatus);
    printf("RequestBit function completed, final bitStatus = 0x%08X\n", bitStatus);
}

void RequestCBIT(uint32_t mtype) {
    uint32_t bitStatus = 0;

    // DEBUG: 초기화 메시지 출력
    printf("Starting RequestCBIT function, mtype = 0x%08X\n", mtype);

    // CBIT 항목 상태 체크
    bitStatus |= ((check_gpio_expander() != 0) << 0);          // GPIO Expander 상태
    bitStatus |= ((checkEthernet() != 0) << 1);               // Ethernet PHY 상태
    bitStatus |= ((checkNvram() != 0) << 2);                  // NVRAM 상태
    bitStatus |= ((checkDiscrete_in() != 0) << 3);            // Discrete In 상태
    bitStatus |= ((check_discrete_out() != 0) << 4);          // Discrete Out 상태
    bitStatus |= ((checkRs232() != 0) << 5);                  // RS232 상태
    bitStatus |= ((checkEthernetSwitch() != 0) << 6);         // Ethernet Switch 상태
    bitStatus |= ((checkTempSensor() != 0) << 7);             // 온도 센서 상태
    bitStatus |= ((checkPowerMonitor() != 0) << 8);           // 전력 모니터 상태
    bitStatus |= ((checkHoldupModule() != 0) << 9);           // Holdup Module 상태
    bitStatus |= ((checkStm32Status() != 0) << 10);           // STM32 상태

    // 플래그 비트 설정 (31번 비트)
    bitStatus |= (1 << 31);

    // 결과 출력
    printf("CBIT Test Results:\n");
    printf("GPIO Expander = %u\n", (bitStatus >> 0) & 1);
    printf("Ethernet PHY = %u\n", (bitStatus >> 1) & 1);
    printf("NVRAM = %u\n", (bitStatus >> 2) & 1);
    printf("Discrete In = %u\n", (bitStatus >> 3) & 1);
    printf("Discrete Out = %u\n", (bitStatus >> 4) & 1);
    printf("RS232 = %u\n", (bitStatus >> 5) & 1);
    printf("Ethernet Switch = %u\n", (bitStatus >> 6) & 1);
    printf("Temperature Sensor = %u\n", (bitStatus >> 7) & 1);
    printf("Power Monitor = %u\n", (bitStatus >> 8) & 1);
    printf("Holdup Module = %u\n", (bitStatus >> 9) & 1);
    printf("STM32 Status = %u\n", (bitStatus >> 10) & 1);

    // 에러 비트 확인
    if (bitStatus & 0x7FFFFFFF) {
        printf("CBIT error occurred, bitStatus = 0x%08X, mtype = 0x%08X.\n", bitStatus, mtype);
        WriteBitErrorData(bitStatus, mtype);
    }

    // 결과 저장
    WriteBitResult(mtype, bitStatus);
    printf("RequestCBIT function completed, final bitStatus = 0x%08X\n", bitStatus);
}

uint32_t readtBitResult(uint32_t type) {
    uint32_t bitResult = ReadBitResult(type);

    // 31번 비트를 확인
    if ((bitResult >> 31) & 1) {
        printf("New BIT result detected. Clearing the new value flag.\n");

        // 31번 비트를 0으로 초기화
        bitResult &= ~(1 << 31);

        // 초기화된 값을 저장
        WriteBitResult(type, bitResult);
    }

    return bitResult;
}

