#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdint.h>

// 함수 선언들
uint8_t  setLedState(uint8_t gpio, uint8_t value);
uint8_t  getGpioState(uint8_t gpio);
uint8_t  setGpioConf(uint8_t port, uint8_t value);
uint8_t  getConfState(uint8_t port);
uint32_t  setDiscreteOut(uint8_t gpio, uint16_t value);
uint8_t  getDiscreteOut(uint8_t gpio);
uint16_t  getDiscreteOutAll(void);
uint32_t  setDiscreteConf(uint8_t port, uint8_t value);
uint32_t  getDiscreteConf(uint8_t port, uint8_t *value);
uint32_t setDiscreteOutAll(uint16_t value);
uint16_t getDiscreteOutAll(void);

#endif // LED_CONTROL_H
