#ifndef OPTIC_CONTROL_H
#define OPTIC_CONTROL_H

#include <stdint.h>

// GPU_API 매크로 정의
#define GPU_API;


/**
 * @brief Set the state of a GPIO pin.
 * @param gpio The GPIO pin to set.
 * @param value The value to set the GPIO pin to (0 or 1).
 * @return 0 on success, 1 on failure.
 */
uint32_t GPU_API getOpticTestRegister(void) ;
uint8_t setOpticPort(void);

#endif // OPTIC_CONTROL_H

