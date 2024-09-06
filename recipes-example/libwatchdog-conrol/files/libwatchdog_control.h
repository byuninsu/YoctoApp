#ifndef WATCHDOG_CONTROL_H
#define WATCHDOG_CONTROL_H

#include <stdint.h>

#define STATUS_SUCCESS 0x00
#define STATUS_ERROR   0x01


#ifdef __cplusplus
extern "C" {
#endif

// 함수 선언
uint32_t StartWatchDog(void);
uint32_t StopWatchDog(void);
uint32_t SetWatchDogTimeout(int timeout);
uint32_t SetWatchDog0Timeout(int timeout);

#ifdef __cplusplus
}
#endif

#endif // WATCHDOG_CONTROL_H

