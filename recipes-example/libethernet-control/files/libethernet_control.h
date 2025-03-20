#ifndef ETHERNET_CONTROL_H
#define ETHERNET_CONTROL_H

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
char * checkEthernetInterface();
uint8_t  setEthernetPort(int port, int state);
uint32_t  getEthernetPort(int port);
uint8_t setVlanStp(void);

#ifdef __cplusplus
}
#endif

#endif // ETHERNET_CONTROL_H

