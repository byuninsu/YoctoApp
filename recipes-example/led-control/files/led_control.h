#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdint.h>

// GPU_API 매크로가 정의되지 않은 경우 정의

#define GPU_API


// 함수 선언들
uint32_t GPU_API setLedState(uint8_t gpio, uint16_t value);
uint8_t GPU_API getGpioState(uint8_t gpio);
uint32_t GPU_API setGpioConf(uint8_t port, uint8_t value);
uint32_t GPU_API getConfState(uint8_t port, uint8_t *value);
uint32_t GPU_API setDiscreteOut(uint8_t gpio, uint16_t value);
uint8_t GPU_API getDiscreteOut(uint8_t gpio);
uint32_t GPU_API setDiscreteConf(uint8_t port, uint8_t value);
uint32_t GPU_API getDiscreteConf(uint8_t port, uint8_t *value);

#endif // LED_CONTROL_H
