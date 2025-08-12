#include "libnvram_control.h"
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>  
#include <linux/spi/spidev.h>
#include <string.h>
#include <stdlib.h>

static const char *device = "/dev/spidev2.0";
static uint32_t speed = 4500000;
static uint8_t bits = 8;
static uint8_t mode = 0;
static int fd; // device
static int lock_fd = -1;

#define SPI_LOCK_FILE "/var/lock/spi_nvram.lock"

    
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

    // printf("spi mode: 0x%x\n", mode);
    // printf("spi bits per word: %d\n", bits);
    // printf("spi max speed: %d Hz (%d KHz)\n", speed, speed/1000);

    return 0; // 초기화 성공
}

void nvram_spiLock(void) {
    if (lock_fd == -1) {
        lock_fd = open(SPI_LOCK_FILE, O_CREAT | O_RDWR, 0666);
        if (lock_fd < 0) {
            perror("lock file open failed");
            return;
        }
    }

    if (flock(lock_fd, LOCK_EX) < 0) {
        perror("flock LOCK_EX failed");
    }
}

void nvram_spiUnlock(void) {
    if (lock_fd != -1) {
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
        lock_fd = -1;
    }
}


uint32_t ReadNVRAMValue(uint32_t address) {
    nvram_spiLock();

    if (nvramInit() != 0) {
        printf("nvramInit Fail !\n");
        nvram_spiUnlock();
        return 1;
    }

    uint8_t tx_buf1[4];  // 명령어 및 주소 전송 버퍼
    uint8_t tx_buf2[1] = { 0x00 };  // 더미 데이터
    uint8_t rx_buf[1] = { 0 };
    uint32_t nvram_output = 0;

    tx_buf1[0] = NVRAM_SPI_CMD_READ;
    tx_buf1[1] = (address >> 16) & 0xFF;
    tx_buf1[2] = (address >> 8) & 0xFF;
    tx_buf1[3] = address & 0xFF;

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
        fd = -1;
        nvram_spiUnlock();
        return 1;
    }

    nvram_output = rx_buf[0];

    if (fd != -1) {
        close(fd);
        fd = -1;
    }

    nvram_spiUnlock();
    return nvram_output;
}

uint32_t WriteNVRAMValue(uint32_t address, uint8_t value) {
    nvram_spiLock();

    if (nvramInit() != 0) {
        printf("nvramInit Fail!\n");
        nvram_spiUnlock();
        return 1;
    }

    uint8_t tx_buf[1];
    uint8_t tx_command_buf[5];
    uint8_t rx_buf[5] = {0};
    uint32_t nvram_output = 0;

    // 1. WREN 명령 전송
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
        fd = -1;
        nvram_spiUnlock();
        return 1;
    }

    // 2. WRITE 명령 전송 (주소 + 값)
    tx_command_buf[0] = NVRAM_SPI_CMD_WRITE;
    tx_command_buf[1] = (address >> 16) & 0xFF;
    tx_command_buf[2] = (address >> 8) & 0xFF;
    tx_command_buf[3] = address & 0xFF;
    tx_command_buf[4] = value;

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
        fd = -1;
        nvram_spiUnlock();
        return 1;
    }

    nvram_output = rx_buf[0];

    // 3. WRDI 명령 전송
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
        fd = -1;
        nvram_spiUnlock();
        return 1;
    }

    if (fd != -1) {
        close(fd);
        fd = -1;
    }

    nvram_spiUnlock();
    return nvram_output;
}

uint32_t ReadNVRAM4ByteValue(uint32_t address) {
    //printf("ReadNVRAM4ByteValue ++++++++++++");

    pid_t pid = getpid();
    //printf("[WriteNVRAM4ByteValue] Called from PID: %d\n", pid);

    if (nvramInit() != 0) {
        printf("nvramInit Fail!\n");
        return 1;
    }

    uint8_t tx_buf1[4];
    uint8_t tx_buf2[1] = {0x00};
    uint8_t rx_buf[1] = {0};
     uint8_t read_bytes[4] = {0};
    uint32_t nvram_output = 0;

    for (int i = 0; i < 4; i++) {
        tx_buf1[0] = NVRAM_SPI_CMD_READ;
        tx_buf1[1] = ((address + i) >> 16) & 0xFF;
        tx_buf1[2] = ((address + i) >> 8) & 0xFF;
        tx_buf1[3] = (address + i) & 0xFF;

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
            fd = -1;
            return 1;
        }

         read_bytes[i] = rx_buf[0];  
        nvram_output |= (rx_buf[0] << (8 * (3 - i)));
    }

    if (fd != -1) {
        close(fd);
        fd = -1;
    }

    // printf("[DEBUG] Read 0x%06X: ", address);
    // for (int i = 0; i < 4; i++) {
    //     printf("0x%02X ", read_bytes[i]);
    // }
    // printf("Combined: 0x%08X (%u)\n", nvram_output, nvram_output);

    //printf("ReadNVRAM4ByteValue ---------");

    return nvram_output;
}

uint32_t WriteNVRAM4ByteValue(uint32_t address, uint32_t value) {
    //printf("WriteNVRAM4ByteValue ++++++++++++");

    pid_t pid = getpid();
   // printf("[WriteNVRAM4ByteValue] Called from PID: %d\n", pid);

    // printf("[DEBUG] WriteNVRAM4ByteValue: address=0x%06X, bytes = [0x%02X 0x%02X 0x%02X 0x%02X]  value = 0x%08X (%u)\n",
    //     address,
    //     (value >> 24) & 0xFF,
    //     (value >> 16) & 0xFF,
    //     (value >> 8)  & 0xFF,
    //     value & 0xFF,
    //     value,
    //     value);

    if (nvramInit() != 0) {
        printf("nvramInit Fail!\n");
        return 1;
    }

    // 1. Write Enable (WREN)
    uint8_t wren_cmd = NVRAM_SPI_CMD_WREN;
    struct spi_ioc_transfer wren = {
        .tx_buf = (uintptr_t)&wren_cmd,
        .rx_buf = 0,
        .len = 1,
        .delay_usecs = 0,
        .speed_hz = speed,
        .bits_per_word = bits
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &wren) < 0) {
        perror("Failed to send WREN command");
        close(fd);
        fd = -1;
        return 1;
    }

    // 2. Write 4 bytes (opcode + address + 4 data bytes)
    uint8_t tx_buf[8] = {
        NVRAM_SPI_CMD_WRITE,
        (address >> 16) & 0xFF,
        (address >> 8) & 0xFF,
        address & 0xFF,
        (value >> 24) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 8) & 0xFF,
        value & 0xFF
    };

    struct spi_ioc_transfer write = {
        .tx_buf = (uintptr_t)tx_buf,
        .rx_buf = 0,
        .len = sizeof(tx_buf),
        .delay_usecs = 0,
        .speed_hz = speed,
        .bits_per_word = bits
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &write) < 0) {
        perror("Failed to send WRITE command");
        close(fd);
        fd = -1;
        return 1;
    }

    // 3. Write Disable (WRDI)
    uint8_t wrdi_cmd = NVRAM_SPI_CMD_WRDI;
    struct spi_ioc_transfer wrdi = {
        .tx_buf = (uintptr_t)&wrdi_cmd,
        .rx_buf = 0,
        .len = 1,
        .delay_usecs = 0,
        .speed_hz = speed,
        .bits_per_word = bits
    };
    ioctl(fd, SPI_IOC_MESSAGE(1), &wrdi);  // 실패해도 무시

    if (fd != -1) {
        close(fd);
        fd = -1;
    }

    //printf("WriteNVRAM4ByteValue ------------");

    return 0;
}

uint32_t getNVRAMId(void) {
    nvram_spiLock();

    if (nvramInit() != 0) {
        printf("nvramInit Fail!\n");
        nvram_spiUnlock();
        return 1;
    }

    uint8_t tx_buf1[1] = { NVRAM_SPI_CMD_GETID };
    uint8_t tx_buf2[4] = { 0x00, 0x00, 0x00, 0x00 };
    uint8_t rx_buf[4] = { 0 };
    uint32_t nvram_output = 0;

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
        fd = -1;
        nvram_spiUnlock();
        return 1;
    }

    // for (int i = 0; i < sizeof(rx_buf); i++) {
    //     printf("getNVRAMId rx_buf[%d] = 0x%02X\n", i, rx_buf[i]);
    // }

    nvram_output = (rx_buf[0] << 24) | (rx_buf[1] << 16) | (rx_buf[2] << 8) | rx_buf[3];

    if (fd != -1) {
        close(fd);
        fd = -1;
    }

    nvram_spiUnlock();
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

uint32_t WriteCumulativeTime(uint32_t time) {
    //printf("WriteCumulativeTime +++++++++++++");
    nvram_spiLock();  

    uint32_t val = ReadCumulativeTime();  
    //printf("[DEBUG] Current cumulative time read from NVRAM: %u (0x%08X)\n", val, val);

    uint32_t minutes = val % 100;
    uint32_t hours = val / 100;
    //printf("[DEBUG] Parsed as: %u hours, %u minutes\n", hours, minutes);

    minutes += time;
    //printf("[DEBUG] After adding %u minutes  total: %u hours, %u minutes (before adjustment)\n", time, hours, minutes);

    if (minutes >= 60) {
        hours += minutes / 60;
        minutes = minutes % 60;
        //printf("[DEBUG] Adjusted for overflow: %u hours, %u minutes\n", hours, minutes);
    }

    uint32_t writeValue = hours * 100 + minutes;
    //printf("[DEBUG] Final value to write to NVRAM: %u (0x%08X)\n", writeValue, writeValue);

    WriteNVRAM4ByteValue(NVRAM_CUMULATIVE_TIME_ADDR, writeValue);

    nvram_spiUnlock();  
    //printf("WriteCumulativeTime --------------");
    return writeValue;
}

uint32_t  ReadCumulativeTime(void) {

    return ReadNVRAM4ByteValue(NVRAM_CUMULATIVE_TIME_ADDR);
}

uint32_t  ReWriteCumulativeTime(uint32_t time) {

    uint32_t writeValue = time;

    return WriteNVRAM4ByteValue(NVRAM_CUMULATIVE_TIME_ADDR, writeValue);
}

uint32_t  WriteBitResult(uint32_t address, uint32_t bitResult) {
    //printf("WriteBitResult input address = 0x%08x bitResult = 0x%08x\n", address, bitResult);
    switch (address) {
        case 2 :
            printf("WriteNVRAM4ByteValue(NVRAM_BIT_RESULT_POWERON \n");
            return WriteNVRAM4ByteValue(NVRAM_BIT_RESULT_POWERON, bitResult);
        case 3 :
            //printf("WriteNVRAM4ByteValue(NVRAM_BIT_RESULT_CONTINUOUS \n");
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

    //printf("ReadBitResult++ \n");

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


// NVRAM에 구조체 저장 함수 (널문자 고려)
uint32_t WriteHwCompatInfoToNVRAM(const struct hwCompatInfo *info) {
    uint32_t result = 0;

    // 매크로로 반복되는 코드 처리
    #define WRITE_STRING_TO_NVRAM(base, field) \
        do { \
            size_t len = sizeof(info->field) - 1; \
            size_t i; \
            for (i = 0; i < len && info->field[i] != '\0'; i++) { \
                result |= WriteNVRAMValue((base) + i, info->field[i]); \
            } \
            result |= WriteNVRAMValue((base) + i, '\0'); \
        } while(0)

    WRITE_STRING_TO_NVRAM(NVRAM_SUPPLIER_PART_NO, supplier_part_no);
    WRITE_STRING_TO_NVRAM(NVRAM_SSD0_MODEL_NO, ssd0_model_no);
    WRITE_STRING_TO_NVRAM(NVRAM_SSD0_SERIAL_NO, ssd0_serial_no);
    WRITE_STRING_TO_NVRAM(NVRAM_SSD1_MODEL_NO, ssd1_model_no);
    WRITE_STRING_TO_NVRAM(NVRAM_SSD1_SERIAL_NO, ssd1_serial_no);
    WRITE_STRING_TO_NVRAM(NVRAM_SW_PART_NO, sw_part_number);
    WRITE_STRING_TO_NVRAM(NVRAM_SW_SERIAL_NO, supplier_serial_no);

    #undef WRITE_STRING_TO_NVRAM

    return result;
}

uint32_t WriteSerialInfoToNVRAM(int index, const char *value)
{
    uint32_t baseAddr = 0;
    size_t maxLen = 15; 

    switch (index) {
        case 1: baseAddr = NVRAM_SUPPLIER_PART_NO; break;
        case 2: baseAddr = NVRAM_SSD0_MODEL_NO;    break;
        case 3: baseAddr = NVRAM_SSD0_SERIAL_NO;   break;
        case 4: baseAddr = NVRAM_SSD1_MODEL_NO;    break;
        case 5: baseAddr = NVRAM_SSD1_SERIAL_NO;   break;
        case 6: baseAddr = NVRAM_SW_PART_NO;       break;
        case 7: baseAddr = NVRAM_SW_SERIAL_NO;     break;
        default:
            return STATUS_ERROR;
    }

    uint32_t result = 0;
    size_t i;
    for (i = 0; i < maxLen && value[i] != '\0'; i++) {
        result |= WriteNVRAMValue(baseAddr + i, value[i]);
    }

    // null 문자 추가
    result |= WriteNVRAMValue(baseAddr + i, '\0');

    return result;
}


// NVRAM에서 구조체 데이터를 읽어오는 함수 (널문자 명확히 처리)
uint32_t ReadHwCompatInfoFromNVRAM(struct hwCompatInfo *info) {
    uint32_t result = 0;

    #define READ_STRING_FROM_NVRAM(base, field) \
        do { \
            size_t len = sizeof(info->field) - 1; \
            size_t i; \
            for (i = 0; i < len; i++) { \
                uint8_t value = ReadNVRAMValue((base) + i); \
                info->field[i] = value; \
                if (value == '\0') break; \
            } \
            info->field[len] = '\0'; \
        } while(0)

    READ_STRING_FROM_NVRAM(NVRAM_SUPPLIER_PART_NO, supplier_part_no);
    READ_STRING_FROM_NVRAM(NVRAM_SSD0_MODEL_NO, ssd0_model_no);
    READ_STRING_FROM_NVRAM(NVRAM_SSD0_SERIAL_NO, ssd0_serial_no);
    READ_STRING_FROM_NVRAM(NVRAM_SSD1_MODEL_NO, ssd1_model_no);
    READ_STRING_FROM_NVRAM(NVRAM_SSD1_SERIAL_NO, ssd1_serial_no);
    READ_STRING_FROM_NVRAM(NVRAM_SW_PART_NO, sw_part_number);
    READ_STRING_FROM_NVRAM(NVRAM_SW_SERIAL_NO, supplier_serial_no);

    #undef READ_STRING_FROM_NVRAM

    return result;
}

uint32_t WriteQtTestValueToNVRAM(const char *value) {
    uint32_t result = 0;

    for (int i = 0; ; i++) {
    WriteNVRAMValue(NVRAM_QT_TEST_ADDR + i, value[i]);
    if (value[i] == '\0') break;
    }

    return result;
}

uint32_t ReadQtTestValueFromNVRAM(char *outBuffer) {
    if (!outBuffer) return 1;

    for (int i = 0; i < 20; i++) {
        char c = ReadNVRAMValue(NVRAM_QT_TEST_ADDR + i);

        // 문자열 종료 조건
        if (c == '\0') {
            outBuffer[i] = '\0';
            return 0;  // 정상 종료
        }

        outBuffer[i] = c;
    }

    // 20바이트를 넘도록 \0을 못 찾았으면 잘못된 값 → 에러
    return 2;
}

uint32_t WriteSWversion(const char *value) {
    for (int i = 0; i < 5; i++) {
        if (WriteNVRAMValue(NVRAM_SW_VERSION_INFO_ADDR + i, value[i]) != 0) {
            return 1;  // 실패
        }
    }
    return 0;  // 성공
}

uint32_t ReadSWversion(char *outValue) {
    for (int i = 0; i < 5; i++) {
        int result = ReadNVRAMValue(NVRAM_SW_VERSION_INFO_ADDR + i);
        if (result < 0) {
            return 1;  // 실패
        }
        outValue[i] = (char)result;
    }
    outValue[5] = '\0';
    return 0;  // 성공
}








