#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include "libstm32_control.h"

static const char *device = "/dev/spidev1.0";
static uint32_t speed = 1000000;
static uint8_t bits = 8;
static uint8_t mode = 0;
static int fd;

int spiInit() {

    int ret = 0;

    // SPI 장치 열기
    fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("Failed to open SPI device");
        return 1;
    }

    // SPI 모드 설정
    ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if (ret == -1)
        perror("can't set spi mode");

    ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
    if (ret == -1)
        perror("can't get spi mode");

    // bits per word 설정
    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1)
        perror("can't set bits per word");

    ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1)
        perror("can't get bits per word");

    // max speed hz 설정
    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1)
        perror("can't set max speed hz");

    ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret == -1)
        perror("can't get max speed hz");

    return 0; // 초기화 성공
}

float sendCommand(stm32_spi_reg command) {

    spiInit();

    uint8_t tx_buf[2] = { command, 0x00 };
	uint8_t tx_buf2[4] = { 0xFF,0xFF,0xFF,0xFF };
    uint8_t spi_rx_buf[4] = { 0 }; 
    float data;

    struct spi_ioc_transfer xfer[2] = {
        {
            .tx_buf = (uintptr_t)tx_buf,
            .rx_buf = 0,
            .len = sizeof(tx_buf),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        },
        {
            .tx_buf = (uintptr_t)tx_buf2,
            .rx_buf = (uintptr_t)spi_rx_buf,
            .len = sizeof(tx_buf2),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        }
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer[0]) < 0) {
        perror("Failed to send command");
        return -1;
    }

	usleep(10000);

	if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer[1]) < 0) {
        perror("Failed to receive data");
        return -1;
    }

    //rx_buf 내용 출력
    for (int i = 0; i < sizeof(spi_rx_buf); i++) {
        printf("rx_buf[%d]: %02X\n", i, spi_rx_buf[i]);
    }

    //memcpy(&data, spi_rx_buf, sizeof(data));

    // 바이트 배열을 float로 변환
    data = *(float*)spi_rx_buf;

    if (fd != -1) {
        close(fd);   
        fd = -1;     
    }

    return data;
}

uint8_t sendOnlyOne(stm32_spi_reg command) {

    spiInit();

    uint8_t tx_buf[2] = { command, 0x00 };
    float data;

    struct spi_ioc_transfer xfer[1] = {
        {
            .tx_buf = (uintptr_t)tx_buf,
            .rx_buf = 0,
            .len = sizeof(tx_buf),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        },
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer[0]) < 0) {
        perror("Failed to spi sendOnlyOne");
        return 1;
    }

    if (fd != -1) {
        close(fd);   
        fd = -1;     
    }
    return 0;
}


uint8_t sendSetTimeout(stm32_spi_reg command, uint8_t timeout) {

    spiInit();

    uint8_t tx_buf[1] = { command };
	uint8_t tx_buf2[1] = { timeout };

    struct spi_ioc_transfer xfer[2] = {
        {
            .tx_buf = (uintptr_t)tx_buf,
            .rx_buf = 0,
            .len = sizeof(tx_buf),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        },
        {
            .tx_buf = (uintptr_t)tx_buf2,
            .rx_buf = 0,
            .len = sizeof(tx_buf2),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        }
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer[0]) < 0) {
        perror("Failed to send command");
        return 1;
    }
	if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer[1]) < 0) {
        perror("Failed to send  Setting data");
        return 1;
    }

    printf("sendSetTimeout Command : %x Timeout : %x",tx_buf[0], tx_buf2[0]);

    if (fd != -1) {
        close(fd);   
        fd = -1;     
    }

    return 0;

}


float  getTempSensorValue(void){
    return sendCommand(STM32_SPI_REG_TEMP);
}

float  getCurrentValue(void){
    return sendCommand(STM32_SPI_REG_CURRUNT);
}

float  getVoltageValue(void){
    return sendCommand(STM32_SPI_REG_VOL);
}

uint8_t  sendStartWatchdog(void){
    return sendOnlyOne(STM32_SPI_START_WATCHDOG);
}

uint8_t  sendStopWatchdog(void){
    return sendOnlyOne(STM32_SPI_STOP_WATCHDOG);
}

uint8_t  sendKeepAlive(void){
    return sendOnlyOne(STM32_SPI_SEND_KEEPALIVE);
}

uint8_t  sendCommandTimeout(uint8_t timeout){
    return sendSetTimeout(STM32_SPI_SEND_SETTIMEOUT, timeout);
}





