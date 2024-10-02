#ifndef ETHERNET_CONTROL_H
#define ETHERNET_CONTROL_H

#include <stdint.h>

#define STATUS_SUCCESS 0x00
#define STATUS_ERROR   0x01

#ifdef __cplusplus
extern "C" {
#endif

// 함수 선언
char * checkEthernetInterface();
uint8_t  setEthernetPort(int port, int state);
uint32_t  getEthernetPort(int port);
void setEthernetStp(int value);

#ifdef __cplusplus
}
#endif

#endif // ETHERNET_CONTROL_H

