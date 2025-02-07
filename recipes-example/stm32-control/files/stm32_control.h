#ifndef STM32_CONTROL_H
#define STM32_CONTROL_H

#include <stdint.h>

typedef enum {
    STM32_SPI_REG_TEMP = 0x01,
    STM32_SPI_REG_CURRUNT = 0x02,
    STM32_SPI_REG_VOL = 0x03,
    STM32_SPI_BOOTCONDITION = 0x04,
    STM32_SPI_START_WATCHDOG = 0x11,
    STM32_SPI_STOP_WATCHDOG = 0x12,
    STM32_SPI_SEND_KEEPALIVE = 0x13,
    STM32_SPI_SEND_SETTIMEOUT = 0x14,
    STM32_SPI_REQUEST_HOLDUP_CC = 0x21,
    STM32_SPI_REQUEST_HOLDUP_PF = 0x22,
} stm32_spi_reg;



// 함수 원형 선언
int spiInit(void);
float sendCommand(stm32_spi_reg command);
float getTempSensorValue(void);
float getCurrentValue(void);
float getVoltageValue(void);
uint8_t sendStartWatchdog(void);
uint8_t sendStopWatchdog(void);
uint8_t sendKeepAlive(void);
uint8_t sendCommandTimeout(uint8_t timeout);
uint8_t sendSetTimeout(stm32_spi_reg command, uint8_t timeout);
uint8_t sendOnlyOne(stm32_spi_reg command);
void sendBootCondition(void);
uint8_t sendCommandForResponseOneByte(stm32_spi_reg command);
uint8_t sendRequestHoldupPF(void);
uint8_t sendRequestHoldupCC(void);

#endif // STM32_CONTROL_H

