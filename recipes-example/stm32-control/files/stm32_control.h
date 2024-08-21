#ifndef STM32_CONTROL_H
#define STM32_CONTROL_H

#include <stdint.h>

#define GPU_API

// stm32 command 열거형 정의
typedef enum {
    STM32_SPI_REG_TEMP = 0x01,
    STM32_SPI_REG_CURRUNT = 0x02,
    STM32_SPI_REG_VOL = 0x03,
} stm32_spi_reg;


// 함수 원형 선언
int spiInit(void);
float sendCommand(stm32_spi_reg command);
float GPU_API getTempSensorValue(void);
float GPU_API getCurrentValue(void);
float GPU_API getVoltageValue(void);

#endif // STM32_CONTROL_H

