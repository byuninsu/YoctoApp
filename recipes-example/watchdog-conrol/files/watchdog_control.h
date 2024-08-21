#ifndef WATCHDOG_CONTROL_H
#define WATCHDOG_CONTROL_H

#include <stdint.h>

#define STATUS_SUCCESS 0x00
#define STATUS_ERROR   0x01

// GPU_API 매크로 정의
#ifndef GPU_API
#define GPU_API  // 빈 정의로 설정
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 함수 선언
uint32_t GPU_API StartWatchDog(void);
uint32_t GPU_API StopWatchDog(void);
uint32_t SetWatchDogTimeout(int timeout);

#ifdef __cplusplus
}
#endif

#endif // WATCHDOG_CONTROL_H

