#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include "stm32_control.h"
#include "nvram_control.h"
#include <pthread.h>

static pthread_mutex_t spi_mutex = PTHREAD_MUTEX_INITIALIZER;
static const char *device = "/dev/spidev1.0";
static uint32_t speed = 1000000;
static uint8_t bits = 8;
static uint8_t mode = 0;
static int fd = -1;

int spiInit(void) {

    int ret = 0;

    if (fd != -1) {
        return 0; // 이미 초기화됨
    }

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

void spiClose(void) {
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
}

__attribute__((destructor))
static void auto_spi_close(void) {
    spiClose();
}

float sendCommandThreadSafe(stm32_spi_reg cmd) {
    pthread_mutex_lock(&spi_mutex);
    float result = sendCommand(cmd);
    pthread_mutex_unlock(&spi_mutex);
    return result;
}

float sendCommand(stm32_spi_reg command) {
    pthread_mutex_lock(&spi_mutex);

    spiInit();

    uint8_t tx_buf[4] = { command, 0x00, 0x00, 0x00 };
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

    memcpy(&data, spi_rx_buf, sizeof(data));

    // 바이트 배열을 float로 변환
    data = *(float*)spi_rx_buf;
    
    pthread_mutex_unlock(&spi_mutex);
    return data;
}

uint8_t sendOnlyOne(stm32_spi_reg command) {
    
    pthread_mutex_lock(&spi_mutex);
    spiInit();

    uint8_t tx_buf[4] = { command, 0x00, 0x00, 0x00 };
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
    
    pthread_mutex_unlock(&spi_mutex);
    return 0;
}

void sendBootCondition(void) {

    pthread_mutex_lock(&spi_mutex);
    spiInit();

    uint8_t tx_buf[4] = { STM32_SPI_BOOTCONDITION, 0x00, 0x00, 0x00 };
	uint8_t tx_buf2[1] = { 0xFF };
    uint8_t spi_rx_buf[1] = { 0 }; 

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
    }

	usleep(10000);

	if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer[1]) < 0) {
        perror("Failed to receive data");
    }

    //rx_buf 내용 출력
    printf("rx_buf[0]: %02X\n", spi_rx_buf[0]);

    //MCU로 인한 재부팅처리 NVRAM에 기록
    if(spi_rx_buf[0] == 0x03){
        WriteBootCondition(0x03);
    } else if (spi_rx_buf[0] == 0x05){
        WriteBootCondition(0x05);
    } else {
        WriteBootCondition(0x02);
    }
    pthread_mutex_unlock(&spi_mutex);
    
}


uint8_t sendSetTimeout(stm32_spi_reg command, uint8_t timeout) {

    pthread_mutex_lock(&spi_mutex);
    spiInit();

    uint8_t tx_buf[4] = { command, timeout, 0x00, 0x00 };

    struct spi_ioc_transfer xfer = {
        .tx_buf = (uintptr_t)tx_buf,
        .rx_buf = 0,
        .len = sizeof(tx_buf),
        .delay_usecs = 0,
        .speed_hz = speed,
        .bits_per_word = bits
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer) < 0) {
        perror("Failed to send command and timeout");
        return 1;
    }

    printf("sendSetTimeout Command : %x Timeout : %x\n", tx_buf[0], tx_buf[1]);

    pthread_mutex_unlock(&spi_mutex);

    return 0;
}

uint8_t sendCommandForResponseOneByte(stm32_spi_reg command) {

    pthread_mutex_lock(&spi_mutex);
    spiInit();

    uint8_t tx_buf[4] = { command, 0x00, 0x00, 0x00 };
	uint8_t tx_buf2[2] = { 0xFF, 0xFF };
    uint8_t spi_rx_buf[1] = { 0 }; 
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
    pthread_mutex_unlock(&spi_mutex);

    return spi_rx_buf[0];
}

uint8_t sendFourByte(stm32_spi_reg command, uint8_t firstVaule, uint8_t secondVaule, uint8_t thirdVaule ) {

    pthread_mutex_lock(&spi_mutex);
    spiInit();

    uint8_t tx_buf[4] = { command, firstVaule, secondVaule, thirdVaule };

    printf("[SPI TX] CMD: 0x%02X, PARAM1: 0x%02X, PARAM2: 0x%02X, PARAM3: 0x%02X\n",
        tx_buf[0], tx_buf[1], tx_buf[2], tx_buf[3]);

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
        perror("Failed to spi sendFourByte");
        return 1;
    }

    pthread_mutex_unlock(&spi_mutex);

    return 0;
}

uint8_t sendFourByteForResponse(stm32_spi_reg command, uint8_t firstVaule) {

    pthread_mutex_lock(&spi_mutex);

    spiInit();

    uint8_t tx_buf[4] = { command, firstVaule, 0x00, 0x00 };
	uint8_t tx_buf2[2] = { 0xFF, 0xFF };
    uint8_t spi_rx_buf[1] = { 0 }; 

    printf("[SPI TX] CMD: 0x%02X, PARAM1: 0x%02X, PARAM2: 0x%02X, PARAM3: 0x%02X\n",
        tx_buf[0], tx_buf[1], tx_buf[2], tx_buf[3]);

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

    pthread_mutex_unlock(&spi_mutex);

    return spi_rx_buf[0];
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

uint8_t sendWatchdogRemainTime(void){
    return sendCommandForResponseOneByte(STM32_SPI_WATCHDOG_REMAIN_TIME);
}

uint8_t sendRequestHoldupPF(void){
    return sendCommandForResponseOneByte(STM32_SPI_REQUEST_HOLDUP_PF);
}

uint8_t sendRequestHoldupCC(void){
    return sendCommandForResponseOneByte(STM32_SPI_REQUEST_HOLDUP_CC);
}

uint8_t sendPowerStatus(void){
    return sendCommandForResponseOneByte(STM32_SPI_REQUEST_POWER_STATUS);
}

uint8_t sendStm32Status(void){
    return sendCommandForResponseOneByte(STM32_SPI_REQUEST_STM32_STATUS);
}

uint8_t  sendJetsonBootComplete(void){
    return sendOnlyOne(STM32_SPI_SEND_BOOT_COMPLETE);
}

uint8_t sendSetLedState(uint8_t gpio, uint8_t value){
    return sendFourByte(STM32_SPI_SEND_LED_SET_VALUE, gpio, value, 0x00);
}

uint8_t sendGetLedState(uint8_t gpio){
    return sendFourByteForResponse(STM32_SPI_SEND_LED_GET_VALUE, gpio);
}

uint8_t sendConfSetLedState(uint8_t gpio, uint8_t value){
    return sendFourByte(STM32_SPI_SEND_LED_CONF_SET_VALUE, gpio, value, 0x00);
}

uint8_t sendConfGetLedState(uint8_t gpio){
    return sendFourByteForResponse(STM32_SPI_SEND_LED_CONF_GET_VALUE, gpio);
}




