#ifndef OPTIC_CONTROL_H
#define OPTIC_CONTROL_H

#include <stdint.h>


/**
 * @brief Set the state of a GPIO pin.
 * @param gpio The GPIO pin to set.
 * @param value The value to set the GPIO pin to (0 or 1).
 * @return 0 on success, 1 on failure.
 */
uint32_t getOpticTestRegister(void) ;
void setOpticPort(void);

#endif // OPTIC_CONTROL_H

