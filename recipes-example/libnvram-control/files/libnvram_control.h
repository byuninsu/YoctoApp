#ifndef NVRAM_CONTROL_H
#define NVRAM_CONTROL_H

#include <stdint.h>

#define STATUS_SUCCESS 0
#define STATUS_ERROR   1

// NVRAM 관련 정의
#define NVRAM_CAPACITY 1024 // NVRAM의 용량 (바이트)

// 구조체 정의
struct hwCompatInfo {
    char supplier_part_no[16]; 
    char ssd0_model_no[16];    
    char ssd0_serial_no[16];   
    char ssd1_model_no[16];    
    char ssd1_serial_no[16];   
    char sw_part_number[16];   
    char supplier_serial_no[16]; 
};

// NVRAM 메모리 맵 (16바이트 간격 권장)
typedef enum {
    NVRAM_POWER_ON_COUNT_ADDR           = 0x00,
    NVRAM_MAINTENANCE_MODE_COUNT_ADDR   = 0x04,
    NVRAM_NORMAL_MODE_COUNT_ADDR        = 0x08,
    NVRAM_CONTAINER_START_COUNT_ADDR    = 0x0C,
    NVRAM_RESET_BY_WATCHDOG_COUNT_ADDR  = 0x10,
    NVRAM_RESET_BY_BUTTON_COUNT_ADDR    = 0x14,
    NVRAM_ACTIVATED_TEST                = 0x18,
    NVRAM_MAINTENANCE_MODE              = 0x1C,
    NVRAM_BOOTING_CONDITION             = 0x20,

    NVRAM_BIT_RESULT_POWERON            = 0x24,
    NVRAM_BIT_RESULT_CONTINUOUS         = 0x2C,
    NVRAM_BIT_RESULT_INITIATED          = 0x34,

    NVRAM_UNSAFETY_SHUTDOWN_COUNT_ADDR  = 0x3C,
    NVRAM_BOOT_MODE                     = 0x44,

    NVRAM_SUPPLIER_PART_NO              = 0x48,   // 0x48 ~ 0x57
    NVRAM_SSD0_MODEL_NO                 = 0x58,   // 0x58 ~ 0x67
    NVRAM_SSD0_SERIAL_NO                = 0x68,   // 0x68 ~ 0x77
    NVRAM_SSD1_MODEL_NO                 = 0x78,   // 0x78 ~ 0x87
    NVRAM_SSD1_SERIAL_NO                = 0x88,   // 0x88 ~ 0x97
    NVRAM_SW_PART_NO                    = 0x98,   // 0x98 ~ 0xA7
    NVRAM_SW_SERIAL_NO                  = 0xA8,   // 0xA8 ~ 0xB7

    NVRAM_QT_TEST_ADDR                  = 0xB8,    
    NVRAM_CUMULATIVE_TIME_ADDR          = 0xC0,
    NVRAM_SW_VERSION_INFO_ADDR          = 0xC8
} NvramAddress;


// NVRAM 명령어 정의
typedef enum {
    NVRAM_SPI_CMD_WRITE  = 0x02,
    NVRAM_SPI_CMD_READ   = 0x03,
    NVRAM_SPI_CMD_WREN   = 0x06,
    NVRAM_SPI_CMD_WRDI   = 0x04,
    NVRAM_SPI_CMD_GETID  = 0x9F,
} nvram_spi_commands_t;


// 함수 선언
uint32_t ReadSystemLog(uint32_t nvramAddress);
uint32_t WriteSystemLogReasonCountCustom(uint32_t resetReason, uint32_t value);
uint32_t WriteSystemLogReasonCount(uint32_t resetReason);
uint32_t WriteMaintenanceModeStatus(uint8_t operationModeState);
uint32_t ReadMaintenanceModeStatus(void);
uint32_t WriteBootCondition(uint8_t bootingConditionState); 
uint32_t ReadBootCondition(void);
uint32_t getNVRAMId(void);
uint32_t WriteBitResult(uint32_t address, uint32_t bitResult);
uint32_t ReadBitResult(uint32_t address);
uint32_t ReadSystemLogReasonCountCustom(uint32_t addr);
uint32_t WriteSystemLogTest(uint32_t resetReason, uint32_t value);
uint32_t WriteCumulativeTime(uint32_t time);
uint32_t ReadCumulativeTime(void);
uint32_t WriteBootModeStatus(uint8_t bootmode);
uint32_t ReadBootModeStatus(void);
uint32_t InitializeNVRAMToZero(void);
uint32_t WriteHwCompatInfoToNVRAM(const struct hwCompatInfo *info);
uint32_t ReadHwCompatInfoFromNVRAM(struct hwCompatInfo *info);
uint32_t WriteQtTestValueToNVRAM(const char *value);
uint32_t ReadQtTestValueFromNVRAM(char *outBuffer);
uint32_t WriteSerialInfoToNVRAM(int index, const char *value);
uint32_t ReWriteCumulativeTime(uint32_t time);


#endif  // NVRAM_CONTROL_H



