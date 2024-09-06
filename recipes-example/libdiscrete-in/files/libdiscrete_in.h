#ifndef SPI_UTILS_H
#define SPI_UTILS_H

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <stdint.h>


/**
 * @brief SPI를 통해 SENSE<15:0> 레지스터 값을 읽는 함수
 * @return 레지스터 값
 */
uint32_t GetDiscreteState(void);
uint32_t GetDiscreteState15to8(void);
uint32_t GetDiscreteState7to0(void);
uint8_t ReadProgramSenseBanks(void);
void WriteProgramSenseBanks(uint8_t bank_settings);

#endif // SPI_UTILS_H

