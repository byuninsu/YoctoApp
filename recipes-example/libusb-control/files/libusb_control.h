#ifndef USB_CONTROL_H
#define USB_CONTROL_H

#include <stdint.h>

#define GPU_API

// Function prototypes
uint8_t GPU_API ActivateUSB(void);
uint8_t GPU_API DeactivateUSB(void);

#endif // USB_CONTROL_H

