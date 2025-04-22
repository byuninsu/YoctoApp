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
    STM32_SPI_WATCHDOG_REMAIN_TIME = 0x15,
    STM32_SPI_REQUEST_HOLDUP_CC = 0x21,
    STM32_SPI_REQUEST_HOLDUP_PF = 0x22,
    STM32_SPI_REQUEST_POWER_STATUS = 0x23,
    STM32_SPI_REQUEST_STM32_STATUS = 0x29,
    STM32_SPI_SEND_BOOT_COMPLETE = 0x31,
    STM32_SPI_SEND_LED_SET_VALUE = 0x32,
    STM32_SPI_SEND_LED_GET_VALUE = 0x33,
    STM32_SPI_SEND_LED_CONF_SET_VALUE = 0x34,
    STM32_SPI_SEND_LED_CONF_GET_VALUE = 0x35
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
uint8_t sendWatchdogRemainTime(void);
uint8_t sendOnlyOne(stm32_spi_reg command);
void sendBootCondition(void);
uint8_t sendCommandForResponseOneByte(stm32_spi_reg command);
uint8_t sendRequestHoldupPF(void);
uint8_t sendRequestHoldupCC(void);
uint8_t sendPowerStatus(void);
uint8_t sendStm32Status(void);
uint8_t sendJetsonBootComplete(void);
uint8_t sendSetLedState(uint8_t gpio, uint8_t value);
uint8_t sendGetLedState(uint8_t gpio);
uint8_t sendConfSetLedState(uint8_t gpio, uint8_t value);
uint8_t sendConfGetLedState(uint8_t gpio);

#endif // STM32_CONTROL_H

