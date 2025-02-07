#include "libnvram_control.h"
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <stdlib.h>

static const char *device = "/dev/spidev2.0";
static uint32_t speed = 4500000;
static uint8_t bits = 8;
static uint8_t mode = 0;
static int fd; // device

//nvram command
typedef enum {

    NVRAM_SPI_CMD_WRITE = 0x02,

    NVRAM_SPI_CMD_READ = 0x03,

    NVRAM_SPI_CMD_WREN = 0x06,

	NVRAM_SPI_CMD_WRDI = 0x04,

	NVRAM_SPI_CMD_GETID= 0x9F,

} nvram_spi_commands_t;


// NVRAM 메모리 맵
typedef enum {
    //1 byte addr
    NVRAM_POWER_ON_COUNT_ADDR = 0x00,
    NVRAM_MAINTENANCE_MODE_COUNT_ADDR = 0x04,
    NVRAM_NORMAL_MODE_COUNT_ADDR = 0x08,
    NVRAM_CONTAINER_START_COUNT_ADDR = 0x0C,
    NVRAM_RESET_BY_WATCHDOG_COUNT_ADDR = 0x10,
    NVRAM_RESET_BY_BUTTON_COUNT_ADDR = 0x14,
    NVRAM_ACTIVATED_TEST = 0x18,
    NVRAM_MAINTENANCE_MODE = 0x1C,
    NVRAM_BOOTING_CONDITION = 0x20,
    NVRAM_UNSAFETY_SHUTDOWN_COUNT_ADDR = 0x3C,
    NVRAM_CUMULATIVE_TIME_ADDR = 0x40,
    NVRAM_BOOT_MODE = 0x44,

    //SSD PART NO ADDR (15BYTE)
    NVRAM_SUPPLIER_PART_NO = 0x48,
    NVRAM_SSD0_MODEL_NO = 0x57,
    NVRAM_SSD0_SERIAL_NO = 0x66,
    NVRAM_SSD1_MODEL_NO = 0x75,
    NVRAM_SSD1_SERIAL_NO = 0x84,
    NVRAM_SW_PART_NO = 0x88,

    //4 byte addr
    NVRAM_BIT_RESULT_POWERON = 0x24,
    NVRAM_BIT_RESULT_CONTINUOUS = 0x2C,
    NVRAM_BIT_RESULT_INITIATED = 0x34
} NvramAddress;

    
uint32_t nvramInit(void) {
    int ret = 0;

    // SPI 장치 파일 열기
    fd = open(device, O_RDWR);
    if (fd < 0) {
        printf("Error: Failed to open SPI device file.\n");
        return 1; // 오류 코드 반환
    }

    // SPI 모드 설정
    ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if (ret == -1) {
        perror("can't set spi mode");
        return 1;
    }

    ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
    if (ret == -1) {
        perror("can't get spi mode");
        return 1;
    }

    // bits per word 설정
    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1) {
        perror("can't set bits per word");
        return 1;
    }

    ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1) {
        perror("can't get bits per word");
        return 1;
    }

    // max speed hz 설정
    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1) {
        perror("can't set max speed hz");
        return 1;
    }

    ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret == -1) {
        perror("can't get max speed hz");
        return 1;
    }

    printf("spi mode: 0x%x\n", mode);
    printf("spi bits per word: %d\n", bits);
    printf("spi max speed: %d Hz (%d KHz)\n", speed, speed/1000);

    return 0; // 초기화 성공
}


uint32_t ReadNVRAMValue(uint32_t address) {
    uint8_t tx_buf1[4];  // 명령어 및 주소 전송 버퍼
    uint8_t tx_buf2[1] = { 0x00 };  // 더미 데이터 전송 버퍼
    uint8_t rx_buf[1] = { 0 };  // 데이터 수신 버퍼
    uint32_t nvram_output = 0;

    if(nvramInit() != 0) {
        printf("nvramInit Fail !");
        return 1;
    }

    // 읽기 명령 및 주소 설정
    tx_buf1[0] = NVRAM_SPI_CMD_READ;
    tx_buf1[1] = (address >> 16) & 0xFF; // 상위 주소 비트 (A16)
    tx_buf1[2] = (address >> 8) & 0xFF;  // 중간 8비트 (A15-A8)
    tx_buf1[3] = address & 0xFF;         // 하위 8비트 (A7-A0)

    // 명령과 주소 전송
    struct spi_ioc_transfer readXfer[2] = {
        {
            .tx_buf = (uintptr_t)tx_buf1,
            .rx_buf = 0,
            .len = sizeof(tx_buf1),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        },
        {
            .tx_buf = (uintptr_t)tx_buf2,
            .rx_buf = (uintptr_t)rx_buf,
            .len = sizeof(tx_buf2),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        }
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(2), readXfer) < 0) {
        perror("Failed to send read command and address");
        close(fd);
        return 1;
    }

    printf("read rx_buf[0] = 0x%02X\n", rx_buf[0]);

    // 수신된 데이터 처리 (1바이트 읽음)
    nvram_output = rx_buf[0];

    // SPI 디바이스 닫기
    close(fd);

    return nvram_output;
}

uint32_t WriteNVRAMValue(uint32_t address, uint8_t value) {
    printf("WriteNVRAMValue input value = 0x%02x at ADdr = 0x%08x\n", value, address);

    uint8_t tx_buf[1];
    uint8_t tx_command_buf[5];
    uint8_t rx_buf[5];
    uint32_t nvram_output = 0;

    if (nvramInit() != 0) {
        printf("nvramInit Fail!\n");
        return 1;
    }

    // WREN 명령 전송
    tx_buf[0] = NVRAM_SPI_CMD_WREN;

    struct spi_ioc_transfer writeXfer = {
        .tx_buf = (uintptr_t)tx_buf,
        .rx_buf = 0,  // 읽어올 필요가 없음
        .len = 1,
        .delay_usecs = 0,
        .speed_hz = speed,
        .bits_per_word = bits
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &writeXfer) < 0) {
        perror("Failed to send WREN command");
        close(fd);
        return 1;
    }

    for (int i = 0; i < sizeof(tx_buf); i++) {
        printf("write wren tx_buf[%d] = 0x%02X\n", i, tx_buf[i]);
    }

    // 쓰기 명령 전송
    tx_command_buf[0] = NVRAM_SPI_CMD_WRITE;
    tx_command_buf[1] = (address >> 16) & 0xFF; // 상위 주소 비트 (A16)
    tx_command_buf[2] = (address >> 8) & 0xFF;  // 중간 8비트 (A15-A8)
    tx_command_buf[3] = address & 0xFF;         // 하위 8비트 (A7-A0)
    tx_command_buf[4] = value;                  // 데이터 바이트

    struct spi_ioc_transfer writeXfer2 = {
        .tx_buf = (uintptr_t)tx_command_buf,
        .rx_buf = (uintptr_t)rx_buf,
        .len = sizeof(tx_command_buf),
        .delay_usecs = 0,
        .speed_hz = speed,
        .bits_per_word = bits
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &writeXfer2) < 0) {
        perror("Failed to send WRITE command");
        close(fd);
        return 1;
    }

    nvram_output = rx_buf[0];

    // WRDI 명령 전송
    tx_buf[0] = NVRAM_SPI_CMD_WRDI;

    struct spi_ioc_transfer writeXfer3 = {
        .tx_buf = (uintptr_t)tx_buf,
        .rx_buf = 0,  // 읽어올 필요가 없음
        .len = 1,
        .delay_usecs = 0,
        .speed_hz = speed,
        .bits_per_word = bits
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &writeXfer3) < 0) {
        perror("Failed to send WRDI command");
        close(fd);
        return 1;
    }

    // SPI 디바이스 닫기
    close(fd);

    return nvram_output;
}

uint32_t ReadNVRAM4ByteValue(uint32_t address) {
    uint8_t tx_buf1[4];  // 명령어 및 주소 전송 버퍼
    uint8_t tx_buf2[1] = { 0x00 };  // 더미 데이터 전송 버퍼
    uint8_t rx_buf[1] = { 0 };  // 데이터 수신 버퍼
    uint32_t nvram_output = 0;

    if (nvramInit() != 0) {
        printf("nvramInit Fail !");
        return 1;
    }

    for (int i = 0; i < 4; i++) {
        // 읽기 명령 및 주소 설정
        tx_buf1[0] = NVRAM_SPI_CMD_READ;
        tx_buf1[1] = ((address + i) >> 16) & 0xFF; // 상위 주소 비트 (A16)
        tx_buf1[2] = ((address + i) >> 8) & 0xFF;  // 중간 8비트 (A15-A8)
        tx_buf1[3] = (address + i) & 0xFF;         // 하위 8비트 (A7-A0)

        // 명령과 주소 전송
        struct spi_ioc_transfer readXfer[2] = {
            {
                .tx_buf = (uintptr_t)tx_buf1,
                .rx_buf = 0,
                .len = sizeof(tx_buf1),
                .delay_usecs = 0,
                .speed_hz = speed,
                .bits_per_word = bits
            },
            {
                .tx_buf = (uintptr_t)tx_buf2,
                .rx_buf = (uintptr_t)rx_buf,
                .len = sizeof(tx_buf2),
                .delay_usecs = 0,
                .speed_hz = speed,
                .bits_per_word = bits
            }
        };

        if (ioctl(fd, SPI_IOC_MESSAGE(2), readXfer) < 0) {
            perror("Failed to send read command and address");
            close(fd);
            return 1;
        }


        printf("ReadNVRAM4ByteValue rx_buf = 0x%02X\n", rx_buf[0]);
    

        // 수신된 데이터 처리 (1바이트 읽음)
        nvram_output |= (rx_buf[0] << (8 * (3 - i))); // MSB부터 채움
    }

    // SPI 디바이스 닫기
    close(fd);

    return nvram_output;
}

uint32_t WriteNVRAM4ByteValue(uint32_t address, uint32_t value) {
    printf("WriteNVRAM4ByteValue input value = 0x%08x at ADdr = 0x%08x\n", value, address);

    uint8_t tx_buf[1];
    uint8_t tx_command_buf[5];
    uint8_t rx_buf[1];

    if (nvramInit() != 0) {
        printf("nvramInit Fail!\n");
        return 1;
    }

    // WREN 명령 전송
    tx_buf[0] = NVRAM_SPI_CMD_WREN;

    struct spi_ioc_transfer writeXfer = {
        .tx_buf = (uintptr_t)tx_buf,
        .rx_buf = 0,
        .len = 1,
        .delay_usecs = 0,
        .speed_hz = speed,
        .bits_per_word = bits
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &writeXfer) < 0) {
        perror("Failed to send WREN command");
        close(fd);
        return 1;
    }

    for (int i = 0; i < 4; i++) {
        // 쓰기 명령 전송
        tx_command_buf[0] = NVRAM_SPI_CMD_WRITE;
        tx_command_buf[1] = ((address + i) >> 16) & 0xFF; // 상위 주소 비트 (A16)
        tx_command_buf[2] = ((address + i) >> 8) & 0xFF;  // 중간 8비트 (A15-A8)
        tx_command_buf[3] = (address + i) & 0xFF;         // 하위 8비트 (A7-A0)
        tx_command_buf[4] = (value >> (8 * (3 - i))) & 0xFF; // MSB부터 분리

        struct spi_ioc_transfer writeXfer2 = {
            .tx_buf = (uintptr_t)tx_command_buf,
            .rx_buf = (uintptr_t)rx_buf,
            .len = sizeof(tx_command_buf),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        };

        if (ioctl(fd, SPI_IOC_MESSAGE(1), &writeXfer2) < 0) {
            perror("Failed to send WRITE command");
            close(fd);
            return 1;
        }
    }

    // WRDI 명령 전송
    tx_buf[0] = NVRAM_SPI_CMD_WRDI;

    struct spi_ioc_transfer writeXfer3 = {
        .tx_buf = (uintptr_t)tx_buf,
        .rx_buf = 0,
        .len = 1,
        .delay_usecs = 0,
        .speed_hz = speed,
        .bits_per_word = bits
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &writeXfer3) < 0) {
        perror("Failed to send WRDI command");
        close(fd);
        return 1;
    }

    // SPI 디바이스 닫기
    close(fd);

    return 0;
}


uint32_t getNVRAMId(void) {
    uint8_t tx_buf1[1] = { NVRAM_SPI_CMD_GETID }; // 명령어 전송 버퍼
    uint8_t tx_buf2[4] = { 0x00, 0x00, 0x00, 0x00 }; // 더미 데이터 전송 버퍼
    uint8_t rx_buf[4] = { 0 }; // 수신 버퍼
    uint32_t nvram_output = 0;

    if (nvramInit() != 0) {
        printf("nvramInit Fail!\n");
        return 1;
    }

    // RDID 명령 전송
    struct spi_ioc_transfer getIdXfer[2] = {
        {
            .tx_buf = (uintptr_t)tx_buf1,
            .rx_buf = 0,
            .len = sizeof(tx_buf1),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        },
        {
            .tx_buf = (uintptr_t)tx_buf2,
            .rx_buf = (uintptr_t)rx_buf,
            .len = sizeof(tx_buf2),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        }
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(2), getIdXfer) < 0) {
        perror("Failed to send RDID command and read data");
        close(fd);
        return 1;
    }

    for (int i = 0; i < sizeof(rx_buf); i++) {
        printf("getNVRAMId rx_buf[%d] = 0x%02X\n", i, rx_buf[i]);
    }

    // 수신된 데이터 처리 (4바이트 ID 읽기)
    nvram_output = (rx_buf[0] << 24) | (rx_buf[1] << 16) | (rx_buf[2] << 8) | rx_buf[3];

    // SPI 디바이스 닫기
    close(fd);

    return nvram_output;
}

// NVRAM 전체 초기화 함수
uint32_t InitializeNVRAMToZero(void) {
    uint32_t result = 0;

    printf("Initializing NVRAM to 0x00000000...\n");

    // 4바이트 주소 초기화
    if (WriteNVRAM4ByteValue(NVRAM_POWER_ON_COUNT_ADDR, 0x00000000)) result = 1;
    if (WriteNVRAM4ByteValue(NVRAM_MAINTENANCE_MODE_COUNT_ADDR, 0x00000000)) result = 1;
    if (WriteNVRAM4ByteValue(NVRAM_NORMAL_MODE_COUNT_ADDR, 0x00000000)) result = 1;
    if (WriteNVRAM4ByteValue(NVRAM_CONTAINER_START_COUNT_ADDR, 0x00000000)) result = 1;
    if (WriteNVRAM4ByteValue(NVRAM_RESET_BY_WATCHDOG_COUNT_ADDR, 0x00000000)) result = 1;
    if (WriteNVRAM4ByteValue(NVRAM_RESET_BY_BUTTON_COUNT_ADDR, 0x00000000)) result = 1;
    if (WriteNVRAM4ByteValue(NVRAM_UNSAFETY_SHUTDOWN_COUNT_ADDR, 0x00000000)) result = 1;

    if (WriteNVRAM4ByteValue(NVRAM_ACTIVATED_TEST, 0x00000000)) result = 1;
    if (WriteNVRAM4ByteValue(NVRAM_MAINTENANCE_MODE, 0x00000000)) result = 1;
    if (WriteNVRAM4ByteValue(NVRAM_BOOTING_CONDITION, 0x00000000)) result = 1;
    if (WriteNVRAM4ByteValue(NVRAM_CUMULATIVE_TIME_ADDR, 0x00000000)) result = 1;
    if (WriteNVRAM4ByteValue(NVRAM_BOOT_MODE, 0x00000000)) result = 1;

    // 추가 4바이트 주소 초기화
    if (WriteNVRAM4ByteValue(NVRAM_BIT_RESULT_POWERON, 0x00000000)) result = 1;
    if (WriteNVRAM4ByteValue(NVRAM_BIT_RESULT_CONTINUOUS, 0x00000000)) result = 1;
    if (WriteNVRAM4ByteValue(NVRAM_BIT_RESULT_INITIATED, 0x00000000)) result = 1;

    if (result == 0) {
        printf("NVRAM initialization to 0x00000000 completed successfully.\n");
    } else {
        printf("NVRAM initialization to 0x00000000 failed.\n");
    }

    return result;
}

uint32_t  WriteBootModeStatus(uint8_t bootmode) {
    return WriteNVRAMValue(NVRAM_BOOT_MODE, bootmode);
}

uint32_t  ReadBootModeStatus(void) {
    return ReadNVRAMValue(NVRAM_BOOT_MODE);
}

uint32_t  WriteMaintenanceModeStatus(uint8_t maintenanceModeState) {
    return WriteNVRAMValue(NVRAM_MAINTENANCE_MODE, maintenanceModeState);
}

uint32_t  ReadMaintenanceModeStatus(void) {
    return ReadNVRAMValue(NVRAM_MAINTENANCE_MODE);
}

uint32_t  WriteBootCondition(uint8_t bootingConditionState) {
    return WriteNVRAMValue(NVRAM_BOOTING_CONDITION, bootingConditionState);
}

uint32_t  ReadBootCondition(void) {
    return ReadNVRAMValue(NVRAM_BOOTING_CONDITION);
}

uint32_t  WriteCumulativeTime(uint32_t time) {

    uint32_t readValue = ReadCumulativeTime();
    uint32_t writeValue = readValue + time;

    return WriteNVRAM4ByteValue(NVRAM_CUMULATIVE_TIME_ADDR, writeValue);
}

uint32_t  ReadCumulativeTime(void) {

    return ReadNVRAM4ByteValue(NVRAM_CUMULATIVE_TIME_ADDR);
}

uint32_t  WriteBitResult(uint32_t address, uint32_t bitResult) {
    printf("WriteBitResult input address = 0x%08x bitResult = 0x%08x\n", address, bitResult);
    switch (address) {
        case 2 :
            printf("WriteNVRAM4ByteValue(NVRAM_BIT_RESULT_POWERON \n");
            return WriteNVRAM4ByteValue(NVRAM_BIT_RESULT_POWERON, bitResult);
        case 3 :
            printf("WriteNVRAM4ByteValue(NVRAM_BIT_RESULT_CONTINUOUS \n");
            return WriteNVRAM4ByteValue(NVRAM_BIT_RESULT_CONTINUOUS, bitResult);
        case 4 :
            printf("WriteNVRAM4ByteValue(NVRAM_BIT_RESULT_INITIATED \n");
            return WriteNVRAM4ByteValue(NVRAM_BIT_RESULT_INITIATED, bitResult);
        case 12 :
            printf("WriteNVRAM4ByteValue(NVRAM_ACTIVATED_TEST \n");
            return WriteNVRAM4ByteValue(NVRAM_ACTIVATED_TEST, bitResult);
    }

    return 1;
}

uint32_t  ReadBitResult(uint32_t address) {

    printf("ReadBitResult++ \n");

    switch (address) {
        case 2 :
            return ReadNVRAM4ByteValue(NVRAM_BIT_RESULT_POWERON);
        case 3 :
            return ReadNVRAM4ByteValue(NVRAM_BIT_RESULT_CONTINUOUS);
        case 4 :
            return ReadNVRAM4ByteValue(NVRAM_BIT_RESULT_INITIATED);
        case 12 :
            return ReadNVRAM4ByteValue(NVRAM_ACTIVATED_TEST);
    }

    return 1;
}

uint32_t   ReadSystemLog(uint32_t nvramAddress) {
    switch (nvramAddress) {
        case 2:
            return ReadNVRAM4ByteValue(NVRAM_POWER_ON_COUNT_ADDR);
        case 3:
            return ReadNVRAM4ByteValue(NVRAM_MAINTENANCE_MODE_COUNT_ADDR);
        case 4:           
            return ReadNVRAM4ByteValue(NVRAM_NORMAL_MODE_COUNT_ADDR);
        case 5:           
            return ReadNVRAM4ByteValue(NVRAM_CONTAINER_START_COUNT_ADDR);
        case 6:           
            return ReadNVRAM4ByteValue(NVRAM_RESET_BY_WATCHDOG_COUNT_ADDR);
        case 7:          
            return ReadNVRAM4ByteValue(NVRAM_RESET_BY_BUTTON_COUNT_ADDR);
        case 8:          
            return ReadNVRAM4ByteValue(NVRAM_UNSAFETY_SHUTDOWN_COUNT_ADDR);
        case 12:          
            return ReadNVRAM4ByteValue(NVRAM_ACTIVATED_TEST);
        default:
            return STATUS_ERROR;
    }
}

uint32_t  ReadSystemLogReasonCountCustom(uint32_t addr) {

    ReadNVRAMValue(addr);
}




uint32_t  WriteSystemLogReasonCountCustom(uint32_t resetReason, uint32_t value) {

    WriteNVRAMValue(resetReason, value);
}

uint32_t  WriteSystemLogTest(uint32_t resetReason, uint32_t value) {

       switch (resetReason) {
            case 2:                 
                 return (NVRAM_POWER_ON_COUNT_ADDR, value);
            case 3:                
                 return WriteNVRAM4ByteValue(NVRAM_MAINTENANCE_MODE_COUNT_ADDR, value);
            case 4:                
                 return WriteNVRAM4ByteValue(NVRAM_NORMAL_MODE_COUNT_ADDR, value);
            case 5:                 
                 return WriteNVRAM4ByteValue(NVRAM_CONTAINER_START_COUNT_ADDR, value);
            case 6:                
                 return WriteNVRAM4ByteValue(NVRAM_RESET_BY_WATCHDOG_COUNT_ADDR, value);
            case 7: 
                 return WriteNVRAM4ByteValue(NVRAM_RESET_BY_BUTTON_COUNT_ADDR, value); 
            case 12: 
                 return WriteNVRAM4ByteValue(NVRAM_ACTIVATED_TEST, value);
            default:
                 return STATUS_ERROR;
        }
}

uint32_t  WriteSystemLogReasonCount(uint32_t resetReason) {

    uint32_t readValue = ReadSystemLog(resetReason);
    uint32_t writeValue = readValue + 1;

       switch (resetReason) {
            case 2:                 
                 return WriteNVRAM4ByteValue(NVRAM_POWER_ON_COUNT_ADDR, writeValue);
            case 3:                
                 return WriteNVRAM4ByteValue(NVRAM_MAINTENANCE_MODE_COUNT_ADDR, writeValue);
            case 4:                
                 return WriteNVRAM4ByteValue(NVRAM_NORMAL_MODE_COUNT_ADDR, writeValue);
            case 5:                 
                 return WriteNVRAM4ByteValue(NVRAM_CONTAINER_START_COUNT_ADDR, writeValue);
            case 6:                
                 return WriteNVRAM4ByteValue(NVRAM_RESET_BY_WATCHDOG_COUNT_ADDR, writeValue);
            case 7: 
                 return WriteNVRAM4ByteValue(NVRAM_RESET_BY_BUTTON_COUNT_ADDR, writeValue);
            case 8: 
                 return WriteNVRAM4ByteValue(NVRAM_UNSAFETY_SHUTDOWN_COUNT_ADDR, writeValue);  
            case 12: 
                 return WriteNVRAM4ByteValue(NVRAM_ACTIVATED_TEST, writeValue);
            default:
                 return STATUS_ERROR;
        }
}

// NVRAM에 구조체 저장 함수
uint32_t WriteHwCompatInfoToNVRAM(const struct hwCompatInfo *info) {
    uint32_t result = 0;

    // `supplier_part_no` 저장 (11 bytes + null terminator)
    for (size_t i = 0; i < sizeof(info->supplier_part_no); i++) {
        result |= WriteNVRAMValue(NVRAM_SUPPLIER_PART_NO + i, info->supplier_part_no[i]);
    }

    // `ssd0_model_no` 저장 (13 bytes + null terminator)
    for (size_t i = 0; i < sizeof(info->ssd0_model_no); i++) {
        result |= WriteNVRAMValue(NVRAM_SSD0_MODEL_NO + i, info->ssd0_model_no[i]);
    }

    // `ssd0_serial_no` 저장 (10 bytes + null terminator)
    for (size_t i = 0; i < sizeof(info->ssd0_serial_no); i++) {
        result |= WriteNVRAMValue(NVRAM_SSD0_SERIAL_NO + i, info->ssd0_serial_no[i]);
    }

    // `ssd1_model_no` 저장 (12 bytes + null terminator)
    for (size_t i = 0; i < sizeof(info->ssd1_model_no); i++) {
        result |= WriteNVRAMValue(NVRAM_SSD1_MODEL_NO + i, info->ssd1_model_no[i]);
    }

    // `ssd1_serial_no` 저장 (14 bytes + null terminator)
    for (size_t i = 0; i < sizeof(info->ssd1_serial_no); i++) {
        result |= WriteNVRAMValue(NVRAM_SSD1_SERIAL_NO + i, info->ssd1_serial_no[i]);
    }

    // `sw_part_number` 저장 (11 bytes + null terminator)
    for (size_t i = 0; i < sizeof(info->sw_part_number); i++) {
        result |= WriteNVRAMValue(NVRAM_SW_PART_NO + i, info->sw_part_number[i]);
    }

    return result;
}

// NVRAM에서 구조체 데이터를 읽어오는 함수
uint32_t ReadHwCompatInfoFromNVRAM(struct hwCompatInfo *info) {
    uint32_t result = 0;

    // `supplier_part_no` 읽기 (11 bytes + null terminator)
    for (size_t i = 0; i < sizeof(info->supplier_part_no); i++) {
        uint8_t value = ReadNVRAMValue(NVRAM_SUPPLIER_PART_NO + i);
        info->supplier_part_no[i] = value;
        if (value == '\0') break; // Null terminator 찾으면 종료
    }

    // `ssd0_model_no` 읽기 (13 bytes + null terminator)
    for (size_t i = 0; i < sizeof(info->ssd0_model_no); i++) {
        uint8_t value = ReadNVRAMValue(NVRAM_SSD0_MODEL_NO + i);
        info->ssd0_model_no[i] = value;
        if (value == '\0') break; // Null terminator 찾으면 종료
    }

    // `ssd0_serial_no` 읽기 (10 bytes + null terminator)
    for (size_t i = 0; i < sizeof(info->ssd0_serial_no); i++) {
        uint8_t value = ReadNVRAMValue(NVRAM_SSD0_SERIAL_NO + i);
        info->ssd0_serial_no[i] = value;
        if (value == '\0') break; // Null terminator 찾으면 종료
    }

    // `ssd1_model_no` 읽기 (12 bytes + null terminator)
    for (size_t i = 0; i < sizeof(info->ssd1_model_no); i++) {
        uint8_t value = ReadNVRAMValue(NVRAM_SSD1_MODEL_NO + i);
        info->ssd1_model_no[i] = value;
        if (value == '\0') break; // Null terminator 찾으면 종료
    }

    // `ssd1_serial_no` 읽기 (14 bytes + null terminator)
    for (size_t i = 0; i < sizeof(info->ssd1_serial_no); i++) {
        uint8_t value = ReadNVRAMValue(NVRAM_SSD1_SERIAL_NO + i);
        info->ssd1_serial_no[i] = value;
        if (value == '\0') break; // Null terminator 찾으면 종료
    }

    // `sw_part_number` 읽기 (11 bytes + null terminator)
    for (size_t i = 0; i < sizeof(info->sw_part_number); i++) {
        uint8_t value = ReadNVRAMValue(NVRAM_SW_PART_NO + i);
        info->ssd1_serial_no[i] = value;
        if (value == '\0') break; // Null terminator 찾으면 종료
    }

    return result;
}







