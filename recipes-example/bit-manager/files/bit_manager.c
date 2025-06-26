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
#include <termios.h>
#include <linux/nvme_ioctl.h>
#include <sys/mount.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
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

#include <net/if.h>
#include <linux/sockios.h>
#include "mii.h"

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


int mdio_read_internal(int skfd, const char *ifname, int phy_addr, int page, int location) {
    struct ifreq ifr;
    struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    // 페이지 설정 (페이지 레지스터 0x16 / 22번)
    if (page >= 0) {
        mii->phy_id = phy_addr;
        mii->reg_num = 22;
        mii->val_in = page;
        if (ioctl(skfd, SIOCSMIIREG, &ifr) < 0) {
            fprintf(stderr, "Page set (reg 22) failed: %s\n", strerror(errno));
            return -1;
        }
    }

    // 원하는 레지스터 읽기
    mii->phy_id = phy_addr;
    mii->reg_num = location;
    if (ioctl(skfd, SIOCGMIIREG, &ifr) < 0) {
        fprintf(stderr, "Read failed: %s\n", strerror(errno));
        return -1;
    }

    return mii->val_out;
}

void sendRS232Message(const char* message) {
    if(isRS232Available()) return;
    if (!message) return;

    int fd = open("/dev/ttyTHS0", O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("[RS232] Failed to open");
        return;
    }

    struct termios tty;
    memset(&tty, 0, sizeof tty);

    if (tcgetattr(fd, &tty) != 0) {
        perror("[RS232] Failed to get attributes");
        close(fd);
        return;
    }

    // 기본 설정 (115200 8N1)
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 1;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("[RS232] Failed to set attributes");
        close(fd);
        return;
    }

    write(fd, message, strlen(message));
    write(fd, "\r\n", 2);
    tcdrain(fd);     
    close(fd);
}

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
    "Ethernet Switch"
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

const char* GetCbitItemName(uint32_t mItem) {
    if (mItem < GetItemCountofCBIT()) {
        return cBitItemNames[mItem];
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
    char lastLine[1024] = {0};

    // 마지막 비어있지 않은 줄 찾기
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        // 공백/엔터만 있는 줄은 무시
        char *trim = buffer;
        while (*trim == ' ' || *trim == '\t' || *trim == '\r' || *trim == '\n') trim++;
        if (*trim != '\0') {
            strcpy(lastLine, buffer);
        }
    }
    fclose(file);

    if (lastLine[0] == '\0') {
        return 1;
    }

    // 마지막 줄에서 Sequence 추출
    char *seqStr = strstr(lastLine, "\"Sequence\":");
    uint32_t lastSequence = 1;
    if (seqStr) {
        sscanf(seqStr, "\"Sequence\":%u,", &lastSequence);
        lastSequence++; // 다음 시퀀스 계산
    }
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

        for (uint32_t i = 0; i < itemCount; i++) {
        uint32_t bitValue = (bitStatus >> i) & 1;  // Extract the bit value (0 or 1)
        fprintf(logFile, "\"%s\":%u", GetCbitItemName(i), bitValue);
        if (i < itemCount - 1) {
            fprintf(logFile, ",");
        }
    }
        
    } else {
        itemCount = GetItemCount();

        for (uint32_t i = 0; i < itemCount; i++) {
        uint32_t bitValue = (bitStatus >> i) & 1;  // Extract the bit value (0 or 1)
        fprintf(logFile, "\"%s\":%u", GetItemName(i), bitValue);
        if (i < itemCount - 1) {
            fprintf(logFile, ",");
        }
    }
    }
    
    // Write mtype and timestamp
    fprintf(logFile, ",\"mtype\":%u,\"timestamp\":\"%s\"}\n\n", mtype, timestamp);

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

    printf("\n\nStart SSD Test ......\n");

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

    printf("SSD check on %s completed successfully.", device_path);
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
        printf("SSD initialization failed.");
    }

    return result;
}



int check_gpio_expander() {
    //printf("\n\nStart GPIO Expander Test ......\n");
    int status;
    uint8_t readConfValue0 = 0;
    uint8_t writeValue = 1;
    uint32_t result = 0;
    char command[512];

    status = setDiscreteInput(0, writeValue);
    if (status != 0) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    }

    result = getDiscreteInput(0, &readConfValue0);
    if (result == 1) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    } 

    if(readConfValue0 == writeValue  ) {
        //printf(" GPIO Expander test passed.");
    }
    
    setDiscreteInput(0, 0);
    return 0;
}

int check_discrete_out() {
    //printf("\n\nStart discrete_out Test ......\n");
    int status;
    uint8_t readConfValue0 = 0;
    uint8_t writeValue = 1;
    uint32_t result = 0;
    char command[512];

    status = setDiscreteInput(0, writeValue);
    if (status != 0) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    }

    result = getDiscreteInput(0, &readConfValue0);
    if (result == 1) {
        printf("Error: Failed to set configuration for GPIO Expander 0 using led-control.\n");
        return 1;
    } 

    if(readConfValue0 == writeValue  ) {
        //printf(" GPIO Expander test passed.");
    }
    
    setDiscreteInput(0, 0);
    return 0;
}

int checkDiscrete_in() {
    char writeCommand[512];
    char readCommand[512];
    char buffer[512];
    uint8_t writeValue = 0x08;  // SENSE 모드에 쓸 값
    uint8_t resetValue = 0x00;  // 값 복구
    uint8_t readSenseValue = 0;

    //printf("\n\nStart Discrete In Test ......\n");

    // SENSE 모드로 값 1을 쓰는 명령어 실행

    WriteProgramSenseBanks(writeValue);

    readSenseValue = ReadProgramSenseBanks();

    if(readSenseValue == 1){
        printf("Discrete_in test failed: could not read value.\n");
        return 1;
    } 

    if(writeValue == readSenseValue) {
        //printf("Discrete_in test passed. Value is 0x%02X", readSenseValue);
        return 0;  // 값이 일치하면 0 리턴
    }

    WriteProgramSenseBanks(resetValue);

    // 값을 찾지 못한 경우
    printf("Discrete_in test failed: not match writeValue, readSenseValue .\n");
    return 1;
}

int checkEthernet() {
    int sock;
    struct ifreq ifr;
    struct ethtool_value edata;
    char* iface;

    //printf("\n\nStart Ethernet Link Test ......\n");

    iface = checkEthernetInterface();

    // 네트워크 인터페이스를 위한 소켓 생성
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

    // ethtool을 사용하여 링크 상태 확인
    edata.cmd = ETHTOOL_GLINK;
    ifr.ifr_data = (caddr_t)&edata;

    if (ioctl(sock, SIOCETHTOOL, &ifr) == -1) {
        perror("ioctl SIOCETHTOOL");
        close(sock);
        return -1;
    }

    if (edata.data) {
        //printf("Link detected on %s\n", iface);
        close(sock);
        return 0; // 링크 감지됨, 정상
    } else {
        printf("No link detected on %s\n", iface);
        close(sock);
        return 1; // 링크 없음, 비정상
    }
}


int checkGPU() {
    // 'deviceQuery' 프로그램의 절대 경로 지정
    const char *cmd = "/usr/bin/cuda-samples/deviceQuery";
    char buffer[512];
    FILE *pipe;

    printf("\n\nStart GPU Test ......\n");

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

    //printf("\n\nStart NVRAM Test ......\n");

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
       // printf("NVRAM test passed.");
        return 0;  // 값이 일치하면 0 리턴
    } else {
        printf("NVRAM test failed: read 0x%x, expected 0x%x.", readValue, writeValue);
        return 1;
    }

}

int checkRs232() {
    char buffer[512];

    //printf("\n\nStart RS232 Test ......\n");

    if(isRS232Available() == 0){
        //printf("RS232 check passed.");
        return 0;
    }
    return 1;
}

int checkEthernetSwitch() {
    int skfd = -1;
    int result;
    char *iface = checkEthernetInterface(); // 예: "eth0"

    if (!iface || iface[0] == '\0') {
        fprintf(stderr, "No valid Ethernet interface found\n");
        return -1;
    }

    skfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (skfd < 0) {
        perror("socket");
        return -1;
    }

    // phy_addr = 0x02, reg = 0x03, page = -1 (기본 페이지)
    result = mdio_read_internal(skfd, iface, 0x02, -1, 0x03);
    close(skfd);

    if (result < 0) {
        fprintf(stderr, "Failed to read from Ethernet switch\n");
        return -1;
    }

    if (result == 0x0a11) {
        //printf("Ethernet switch test passed (Product ID: 0x%04x)\n", result);
        return 0;
    } else {
        printf("Ethernet switch test failed (Read: 0x%04x)\n", result);
        return 1;
    }
}

int checkTempSensor() {
    char buffer[512];
    float temperature = 0.0;

    printf("\n\nStart TempSensor Test ......\n");

    // stm32-control temp
    temperature = getTempSensorValue();
    
    if(temperature == -1) {
        printf("Error: Failed to stm32-control temp command.\n");
        return 1;
    } else {
        printf("Temperature sensor check passed. Temperature: %.2f", temperature);
        return 0;
    }

}

int checkPowerMonitor() {
    char buffer[512];
    float voltage = 0.0;
    float current = 0.0;

    printf("\n\nStart Power Monitor Test ......\n");

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
    printf("Power monitor check passed. Voltage: %.2f, Current: %.2f", voltage, current);
    return 0;
}

int checkUsb(void){

    printf("\n\nStart USB Test ......\n");

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

    printf("\n\nStart Optic Test ......\n");

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
    struct hwCompatInfo currentInfo;

    printf("\n\nStart HW Info Test ......\n");


    // NVRAM 정보 읽기
    if (ReadHwCompatInfoFromNVRAM(&nvramInfo) != 0) {
        printf("Error reading NVRAM");

        return 1;
    }

    // NVRAM에서 고정값 항목 복사
    strncpy(currentInfo.supplier_part_no, nvramInfo.supplier_part_no, sizeof(currentInfo.supplier_part_no));
    strncpy(currentInfo.supplier_serial_no, nvramInfo.supplier_serial_no, sizeof(currentInfo.supplier_serial_no));
    strncpy(currentInfo.sw_part_number, nvramInfo.sw_part_number, sizeof(currentInfo.sw_part_number));

    // SSD0 정보 읽기
    if (read_nvme_info("/dev/nvme1n1", currentInfo.ssd0_model_no, sizeof(currentInfo.ssd0_model_no),
                                      currentInfo.ssd0_serial_no, sizeof(currentInfo.ssd0_serial_no)) != 0) {
        printf("Error reading nvme1n1 info\n");
        return 1;
    }

    // SSD1 정보 읽기
    if (read_nvme_info("/dev/nvme0n1", currentInfo.ssd1_model_no, sizeof(currentInfo.ssd1_model_no),
                                      currentInfo.ssd1_serial_no, sizeof(currentInfo.ssd1_serial_no)) != 0) {
        printf("Error reading nvme0n1 info\n");
        return 1;
    }

    // 로그 출력
    printf("SSD CurrentInfo:\n");
    printf(" Supplier Part No  : %s\n", currentInfo.supplier_part_no);
    printf(" Supplier Serial No: %s\n", currentInfo.supplier_serial_no);
    printf(" SSD0 Model No     : %s\n", currentInfo.ssd0_model_no);
    printf(" SSD0 Serial No    : %s\n", currentInfo.ssd0_serial_no);
    printf(" SSD1 Model No     : %s\n", currentInfo.ssd1_model_no);
    printf(" SSD1 Serial No    : %s\n", currentInfo.ssd1_serial_no);
    printf(" SW Part Number    : %s\n", currentInfo.sw_part_number);

    // SSD 정보만 비교
    if (strncmp(currentInfo.ssd0_model_no, nvramInfo.ssd0_model_no, sizeof(currentInfo.ssd0_model_no)) != 0 ||
        strncmp(currentInfo.ssd0_serial_no, nvramInfo.ssd0_serial_no, sizeof(currentInfo.ssd0_serial_no)) != 0 ||
        strncmp(currentInfo.ssd1_model_no, nvramInfo.ssd1_model_no, sizeof(currentInfo.ssd1_model_no)) != 0 ||
        strncmp(currentInfo.ssd1_serial_no, nvramInfo.ssd1_serial_no, sizeof(currentInfo.ssd1_serial_no)) != 0) {
        return 1; // SSD 정보 불일치
    }

    return 0; // 일치
}

int checkHoldupModule(void){

    printf("\n\nStart Holdup Module Test ......\n");

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

    printf("\n\nStart STM32 Test ......\n");

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

    printf("\n\nStart Power Status Test ......\n");

    if ( powerStatus & (overvoltage_mask | undervoltage_mask | overcurrent_mask ) ) {
        return 1;
    } else {
        return 0;
    }

}

int checkLan7800(void){

    printf("\n\nStart LAN7800 Test ......\n");


    char* otherIface = checkEthernetInterface();
    char* iface;

    if(strcmp( otherIface, "eth0") == 0 ){
        iface = "eth1";
    } else {
        iface = "eth0";
    }

    // 해당 인터페이스가 존재하는지만 확인
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/net/%s", iface);
    if (access(path, F_OK) == 0) {
        return 0;  // 정상
    } else {
        return 1;  // 비정상
    }
}

// USB1 상태 확인 함수
int checkUSBc(void) {
    printf("\n\nStart USB C Test ......\n");


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
    printf("\n\nStart USB A Test ......\n");

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
        return 0; // 정상
    } else {
        return 1; // 비정상
    }
}



void RequestBit(uint32_t mtype) {
    uint32_t bitStatus = 0;
    char rs232Result[128] = {0};

    // DEBUG: 초기화 메시지 출력
    printf("\n\n Starting RequestBit function, mtype = 0x%08X\n", mtype);

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
    printf("\n\n [ PBIT Test Results ]\n");
    printf("%-20s = %u\n", "GPU",                 (bitStatus >> 0)  & 1);
    printf("%-20s = %u\n", "SSD(Data)",           (bitStatus >> 1)  & 1);
    printf("%-20s = %u\n", "GPIO Expander",       (bitStatus >> 2)  & 1);
    printf("%-20s = %u\n", "Ethernet",            (bitStatus >> 3)  & 1);
    printf("%-20s = %u\n", "NVRAM",               (bitStatus >> 4)  & 1);
    printf("%-20s = %u\n", "Discrete In",         (bitStatus >> 5)  & 1);
    printf("%-20s = %u\n", "Discrete Out",        (bitStatus >> 6)  & 1);
    printf("%-20s = %u\n", "RS232",               (bitStatus >> 7)  & 1);
    printf("%-20s = %u\n", "SSD(OS)",             (bitStatus >> 8)  & 1);
    printf("%-20s = %u\n", "Ethernet Switch",     (bitStatus >> 9)  & 1);
    printf("%-20s = %u\n", "Optic Transceiver",   (bitStatus >> 10) & 1);
    printf("%-20s = %u\n", "Temperature Sensor",  (bitStatus >> 11) & 1);
    printf("%-20s = %u\n", "Power Monitor",       (bitStatus >> 12) & 1);
    printf("%-20s = %u\n", "Holdup Module",       (bitStatus >> 13) & 1);
    printf("%-20s = %u\n", "STM32 Status",        (bitStatus >> 14) & 1);
    printf("%-20s = %u\n", "LAN7800 Status",      (bitStatus >> 15) & 1);
    printf("%-20s = %u\n", "AC/DC Status",        (bitStatus >> 16) & 1);
    printf("%-20s = %u\n", "DC/DC Status",        (bitStatus >> 17) & 1);
    printf("%-20s = %u\n", "USB A Status",        (bitStatus >> 18) & 1);
    printf("%-20s = %u\n", "USB C Status",        (bitStatus >> 19) & 1);

    // 에러 비트 확인
    if (bitStatus & 0x7FFFFFFF) {
        printf("\n bit error occurred, bitStatus = 0x%08X, mtype = 0x%08X.\n", bitStatus, mtype);
        WriteBitErrorData(bitStatus, mtype);
    }

    // 결과 저장
    WriteBitResult(mtype, bitStatus);

    // 성공/실패 출력
    if (bitStatus & 0x7FFFFFFF) {
        printf("\n[ PBIT FAIL ]  \n");
        snprintf(rs232Result, sizeof(rs232Result), "\n[ PBIT FAIL ] ");
    } else {
        printf("\n[ PBIT SUCCESS] \n");
        snprintf(rs232Result, sizeof(rs232Result), "\n[ PBIT SUCCESS ] ");
    }

    sendRS232Message(rs232Result); //Rs232 전송
}

void RequestCBIT(uint32_t mtype) {
    uint32_t bitStatus = 0;
    char rs232Result[128] = {0};

    // DEBUG: 초기화 메시지 출력
    //printf("\n\nStarting RequestCBIT function, mtype = 0x%08X\n", mtype);

    // CBIT 항목 상태 체크
    bitStatus |= ((check_gpio_expander()    != 0) << 0);   // GPIO Expander 상태
    bitStatus |= ((checkEthernet()          != 0) << 1);   // Ethernet PHY 상태
    bitStatus |= ((checkNvram()             != 0) << 2);   // NVRAM 상태
    bitStatus |= ((checkDiscrete_in()       != 0) << 3);   // Discrete In 상태
    bitStatus |= ((check_discrete_out()     != 0) << 4);   // Discrete Out 상태
    bitStatus |= ((checkEthernetSwitch()    != 0) << 5);   // Ethernet Switch 상태

    // 플래그 비트 설정 (31번 비트)
    bitStatus |= (1 << 31);

    // 결과 출력
    // printf("\n     [ CBIT Test Results ]\n");
    // printf("%-20s : %u\n", "GPIO Expander",     (bitStatus >> 0)  & 1);
    // printf("%-20s : %u\n", "Ethernet PHY",      (bitStatus >> 1)  & 1);
    // printf("%-20s : %u\n", "NVRAM",             (bitStatus >> 2)  & 1);
    // printf("%-20s : %u\n", "Discrete In",       (bitStatus >> 3)  & 1);
    // printf("%-20s : %u\n", "Discrete Out",      (bitStatus >> 4)  & 1);
    // printf("%-20s : %u\n", "Ethernet Switch",   (bitStatus >> 5)  & 1);

    // 에러 비트 확인
    if (bitStatus & 0x7FFFFFFF) {
        //printf("CBIT error occurred, bitStatus = 0x%08X, mtype = 0x%08X.\n", bitStatus, mtype);
        WriteBitErrorData(bitStatus, mtype);
    }

    // 결과 저장
    WriteBitResult(mtype, bitStatus);

    // 성공/실패 출력
    if (bitStatus & 0x7FFFFFFF) {
        //printf("\n[CBIT FAIL ]   \n");
        //snprintf(rs232Result, sizeof(rs232Result), "\n     [CBIT FAIL] ");
    } else {
        //printf("\n[CBIT SUCCESS] \n");
        //snprintf(rs232Result, sizeof(rs232Result), "\n     [CBIT SUCCESS] ");
    }
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

