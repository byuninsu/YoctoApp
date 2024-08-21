#ifndef RS232_CONTROL_H
#define RS232_CONTROL_H

#include <stdint.h>

#define GPU_API


#ifdef __cplusplus
extern "C" {
#endif

uint8_t GPU_API ActivateRS232(void);
uint8_t GPU_API DeactivateRS232(void);

#ifdef __cplusplus
}
#endif

#endif // RS232_CONTROL_H

