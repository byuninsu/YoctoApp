#ifndef NVRAM_CONTROL_H
#define NVRAM_CONTROL_H

#include <stdint.h>

#define GPU_API

#define STATUS_SUCCESS 0
#define STATUS_ERROR   1

// NVRAM 관련 정의
#define NVRAM_CAPACITY 1024 // NVRAM의 용량 (바이트)

// 함수 선언
uint32_t GPU_API ReadSystemLog(uint32_t nvramAddress);
uint32_t GPU_API WriteSystemLogReasonCountCustom(uint32_t resetReason, uint32_t value);
uint32_t GPU_API WriteSystemLogReasonCount(uint32_t resetReason);
uint32_t GPU_API WriteOperationModeStatus(uint8_t operationModeState);
uint32_t GPU_API ReadMaintenanceModeStatus(void);
uint32_t GPU_API WriteBootCondition(uint8_t bootingConditionState); 
uint32_t GPU_API ReadBootCondition(void);
uint32_t getNVRAMId(void);


#endif  // NVRAM_CONTROL_H

