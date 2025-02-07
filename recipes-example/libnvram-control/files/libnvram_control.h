#ifndef NVRAM_CONTROL_H
#define NVRAM_CONTROL_H

#include <stdint.h>

#define STATUS_SUCCESS 0
#define STATUS_ERROR   1

// NVRAM 관련 정의
#define NVRAM_CAPACITY 1024 // NVRAM의 용량 (바이트)

// 구조체 정의
struct hwCompatInfo {
    char supplier_part_no[12]; // 11 bytes data + 1 null terminator
    char ssd0_model_no[14];    // 13 bytes data + 1 null terminator
    char ssd0_serial_no[11];   // 10 bytes data + 1 null terminator
    char ssd1_model_no[13];    // 12 bytes data + 1 null terminator
    char ssd1_serial_no[15];   // 14 bytes data + 1 null terminator
    char sw_part_number[12];   // 11 bytes data + 1 null terminator
};

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


#endif  // NVRAM_CONTROL_H


