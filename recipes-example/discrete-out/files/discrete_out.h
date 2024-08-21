#ifndef DISCRETE_OUT_H
#define DISCRETE_OUT_H

#include <stdint.h>

// GPU_API 매크로 정의
#define GPU_API __attribute__((visibility("default")))


/**
 * @brief Set the state of a GPIO pin.
 * @param gpio The GPIO pin to set.
 * @param value The value to set the GPIO pin to (0 or 1).
 * @return 0 on success, 1 on failure.
 */
uint32_t GPU_API setDiscreteOut(uint8_t port, uint16_t value);
uint8_t GPU_API getDiscreteOut(uint8_t gpio);
uint32_t GPU_API setDiscreteConf(uint8_t port, uint8_t value);
uint32_t GPU_API getDiscreteConf(uint8_t port, uint8_t *value);

#endif // GPIO_CONTROL_H

