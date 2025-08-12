#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <pthread.h>
#include "stm32_control.h"
#include "nvram_control.h"

static const char *device = "/dev/spidev1.0";
static uint32_t speed = 1000000;
static uint8_t bits = 8;
static uint8_t mode = 0;
static uint8_t startBit = 0xAA;
static uint8_t lasttBit = 0xBB;
static int fd = -1;        
static int lock_fd = -1;   
static pthread_mutex_t spi_mutex = PTHREAD_MUTEX_INITIALIZER;

// STM32용
#define SPI_LOCK_FILE "/var/lock/spi_stm32.lock"

int spiInit(void) {

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

void stm32_spiLock(void) {
    pthread_mutex_lock(&spi_mutex);

    if (lock_fd == -1) {
        lock_fd = open(SPI_LOCK_FILE, O_CREAT | O_RDWR, 0666);
        if (lock_fd < 0) {
            perror("lock file open failed");
            pthread_mutex_unlock(&spi_mutex);
            return;
        }
    }

    if (flock(lock_fd, LOCK_EX) < 0) {
        perror("flock LOCK_EX failed");
        close(lock_fd);
        lock_fd = -1;
        pthread_mutex_unlock(&spi_mutex);
        return;
    }
}

void stm32_spiUnlock(void) {
    if (lock_fd != -1) {
        if (flock(lock_fd, LOCK_UN) < 0) {
            perror("flock LOCK_UN failed");
        }
        close(lock_fd);
        lock_fd = -1;
    }

    pthread_mutex_unlock(&spi_mutex);
}

float sendCommand(stm32_spi_reg command) {
    stm32_spiLock();

    if (spiInit() != 0) {
        stm32_spiUnlock();
        return -1;
    }

    uint8_t tx_buf[6] = {startBit, command, 0x00, 0x00, 0x00, lasttBit};
    uint8_t tx_buf2[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t spi_rx_buf[4] = {0};
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

    // 1. 명령 전송
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer[0]) < 0) {
        perror("Failed to send command");
        stm32_spiUnlock();
        return -1;
    }

    usleep(10000);

    // 2. 응답 수신
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer[1]) < 0) {
        perror("Failed to receive data");
        stm32_spiUnlock();
        return -1;
    }

    memcpy(&data, spi_rx_buf, sizeof(data));

    // SPI 디바이스 닫기
    if (fd != -1) {
        close(fd);
        fd = -1;
    }

    stm32_spiUnlock();
    return data;
}

uint8_t sendOnlyOne(stm32_spi_reg command) {
    stm32_spiLock(); 

    if (spiInit() != 0) {
        stm32_spiUnlock();
        return 1;
    }

    uint8_t tx_buf[6] = {startBit, command, 0x00, 0x00, 0x00, lasttBit};

    struct spi_ioc_transfer xfer = {
        .tx_buf = (uintptr_t)tx_buf,
        .rx_buf = 0,
        .len = sizeof(tx_buf),
        .delay_usecs = 0,
        .speed_hz = speed,
        .bits_per_word = bits
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer) < 0) {
        perror("Failed to spi sendOnlyOne");
        stm32_spiUnlock();   
        return 1;
    }

    if (fd != -1) {
        close(fd);
        fd = -1;
    }

    stm32_spiUnlock();  
    return 0;
}

void sendBootCondition(void) {
    stm32_spiLock();

    if (spiInit() != 0) {
        stm32_spiUnlock();
        return;
    }

    uint8_t tx_buf[6] = { startBit, STM32_SPI_BOOTCONDITION, 0x00, 0x00, 0x00, lasttBit };
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
        stm32_spiUnlock();
        return;
    }

    usleep(10000);

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer[1]) < 0) {
        perror("Failed to receive data");
        stm32_spiUnlock();
        return;
    }

    if (spi_rx_buf[0] == 0x03) {
        WriteBootCondition(0x03);
    } else if (spi_rx_buf[0] == 0x05) {
        WriteBootCondition(0x05);
    } else {
        WriteBootCondition(0x02);
    }

    if (fd != -1) {
        close(fd);
        fd = -1;
    }

    stm32_spiUnlock();
}


uint8_t sendSetTimeout(stm32_spi_reg command, uint8_t timeout) {
    stm32_spiLock();

    if (spiInit() != 0) {
        stm32_spiUnlock();
        return 1;
    }

    uint8_t tx_buf[6] = {startBit, command, timeout, 0x00, 0x00, lasttBit};

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
        stm32_spiUnlock();
        return 1;
    }

    if (fd != -1) {
        close(fd);
        fd = -1;
    }

    stm32_spiUnlock();
    return 0;
}

uint8_t sendCommandForResponseOneByte(stm32_spi_reg command) {
    stm32_spiLock();

    if (spiInit() != 0) {
        stm32_spiUnlock();
        return 0xFF;
    }

    uint8_t tx_buf[6] = { startBit, command, 0x00, 0x00, 0x00, lasttBit };
    uint8_t tx_buf2[2] = { 0xFF, 0xFF };
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
        stm32_spiUnlock();
        return 0xFF;
    }

    usleep(10000);

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer[1]) < 0) {
        perror("Failed to receive data");
        stm32_spiUnlock();
        return 0xFF;
    }

    if (fd != -1) {
        close(fd);
        fd = -1;
    }

    stm32_spiUnlock();
    return spi_rx_buf[0];
}

uint8_t sendFourByte(stm32_spi_reg command, uint8_t firstValue, uint8_t secondValue, uint8_t thirdValue) {
    stm32_spiLock();

    if (spiInit() != 0) {
        stm32_spiUnlock();
        return 1;
    }

    uint8_t tx_buf[6] = {startBit, command, firstValue, secondValue, thirdValue, lasttBit};

    struct spi_ioc_transfer xfer = {
        .tx_buf = (uintptr_t)tx_buf,
        .rx_buf = 0,
        .len = sizeof(tx_buf),
        .delay_usecs = 0,
        .speed_hz = speed,
        .bits_per_word = bits
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer) < 0) {
        perror("Failed to spi sendFourByte");
        stm32_spiUnlock();
        return 1;
    }

    if (fd != -1) {
        close(fd);
        fd = -1;
    }

    stm32_spiUnlock();
    return 0;
}

uint8_t sendFourByteForResponse(stm32_spi_reg command, uint8_t firstValue) {
    stm32_spiLock();

    if (spiInit() != 0) {
        stm32_spiUnlock();
        return 0xFF;
    }

    uint8_t tx_buf[6] = {startBit, command, firstValue, 0x00, 0x00, lasttBit};
    uint8_t tx_buf2[2] = {0xFF, 0xFF};
    uint8_t spi_rx_buf[1] = {0};

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
        stm32_spiUnlock();
        return 0xFF;
    }

    usleep(10000);

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &xfer[1]) < 0) {
        perror("Failed to receive data");
        stm32_spiUnlock();
        return 0xFF;
    }

    if (fd != -1) {
        close(fd);
        fd = -1;
    }

    stm32_spiUnlock();
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



