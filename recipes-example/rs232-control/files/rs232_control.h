#ifndef RS232_CONTROL_H
#define RS232_CONTROL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t ActivateRS232(void);
uint8_t DeactivateRS232(void);
int isRS232Available(void);

#ifdef __cplusplus
}
#endif

#endif // RS232_CONTROL_H

