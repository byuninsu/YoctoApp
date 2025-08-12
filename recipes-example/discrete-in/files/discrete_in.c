#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "discrete_in.h"

// HOLT spi command
typedef enum {
    HOLT_READ_7_TO_0= 0x90,
    HOLT_READ_15_TO_8= 0x92,
    HOLT_READ_23_TO_16= 0x94,
    HOLT_READ_31_TO_24= 0x96,
    HOLT_READ_ALL= 0xF8,
    HOLT_WRITE_SENSE_BANK=0x04,
    HOLT_READ_SENSE_BANK=0x84,
    HOLT_WRITE_TEST_MODE=0x1E,
    HOLT_READ_TEST_MODE=0x9E
} HoltSpiCommand;

static const char *device = "/dev/spidev0.0";
static uint32_t speed = 4000000;
static uint8_t bits = 8;
static uint8_t mode = 0;
static int fd;
static int lock_fd = -1;   

// HOLT 용
#define SPI_LOCK_FILE "/var/lock/holt_stm32.lock"

uint32_t discreteSpiInit(void) {
    int ret = 0;

    // SPI 장치 파일 열기
    fd = open(device, O_RDWR);
    if (fd < 0) {
        printf("Error: Failed to open SPI device file.\n");
        return 1; // 오류 코드 반환
    }

    // SPI 모드 설정
    ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if (ret == -1) {
        perror("can't set spi mode");
        return 1;
    }

    ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
    if (ret == -1) {
        perror("can't get spi mode");
        return 1;
    }

    // bits per word 설정
    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1) {
        perror("can't set bits per word");
        return 1;
    }

    ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1) {
        perror("can't get bits per word");
        return 1;
    }

    // max speed hz 설정
    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1) {
        perror("can't set max speed hz");
        return 1;
    }

    ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret == -1) {
        perror("can't get max speed hz");
        return 1;
    }

    // printf("spi mode: 0x%x\n", mode);
    // printf("spi bits per word: %d\n", bits);
    // printf("spi max speed: %d Hz (%d KHz)\n", speed, speed/1000);

    return 0; // 초기화 성공
}

void holt_spiLock(void) {
    if (lock_fd == -1) {
        lock_fd = open(SPI_LOCK_FILE, O_CREAT | O_RDWR, 0666);
        if (lock_fd < 0) {
            perror("lock file open failed");
            return;
        }
    }

    if (flock(lock_fd, LOCK_EX) < 0) {
        perror("flock LOCK_EX failed");
    }
}

void holt_spiUnlock(void) {
    if (lock_fd != -1) {
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
        lock_fd = -1;
    }
}

// 8비트 비트 순서를 반전하는 함수
uint8_t reverse_bits(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}


uint8_t GetDiscreteState7to0(void) {
    uint8_t tx_buf1[1];
    uint8_t tx_buf2[1] = { 0xFF };
    uint8_t rx_buf[1] = { 0 };
    uint8_t discrete_state = 0;

    holt_spiLock(); 

    if (discreteSpiInit() != 0) {
        holt_spiUnlock();
        return 1;
    }

    tx_buf1[0] = HOLT_READ_7_TO_0;

    struct spi_ioc_transfer get7to0Xfer[2] = {
        {
            .tx_buf = (uintptr_t)tx_buf1,
            .rx_buf = 0,
            .len = sizeof(tx_buf1),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        },
        {
            .tx_buf = (uintptr_t)tx_buf2,
            .rx_buf = (uintptr_t)rx_buf,
            .len = sizeof(tx_buf2),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        }
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(2), get7to0Xfer) < 0) {
        perror("Failed to write/read SPI");
        close(fd);
        holt_spiUnlock(); 
        return 1;
    }

    discrete_state = reverse_bits(rx_buf[0]);

    close(fd); // SPI 디바이스 닫기
    holt_spiUnlock(); 

    return discrete_state;
}

uint8_t GetDiscreteState15to8(void) {
    uint8_t tx_buf1[1];
    uint8_t tx_buf2[1] = { 0xFF };
    uint8_t rx_buf[1] = { 0 };
    uint8_t discrete_state = 0;

    holt_spiLock();

    if (discreteSpiInit() != 0) {
        holt_spiUnlock();
        return 1;
    }

    tx_buf1[0] = HOLT_READ_15_TO_8;

    struct spi_ioc_transfer get15to8Xfer[2] = {
        {
            .tx_buf = (uintptr_t)tx_buf1,
            .rx_buf = 0,
            .len = sizeof(tx_buf1),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        },
        {
            .tx_buf = (uintptr_t)tx_buf2,
            .rx_buf = (uintptr_t)rx_buf,
            .len = sizeof(tx_buf2),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        }
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(2), get15to8Xfer) < 0) {
        perror("Failed to write/read SPI");
        close(fd);
        holt_spiUnlock();
        return 1;
    }

    discrete_state = reverse_bits(rx_buf[0]);

    close(fd);
    holt_spiUnlock();

    return discrete_state;
}

uint16_t GetDiscreteState(void) {
    //printf("GetDiscreteState ++ \n");

    uint16_t result = 0;

    uint8_t firstByte =  GetDiscreteState7to0();
    uint8_t secondByte =  GetDiscreteState15to8();

    result = (firstByte << 8) | secondByte;

    return result;
}

uint8_t ReadProgramSenseBanks(void) {
    uint8_t discrete_state = 0;
    uint8_t tx_buf1[1] = { HOLT_READ_SENSE_BANK };
    uint8_t tx_buf2[1] = { 0xFF };
    uint8_t rx_buf[1] = { 0 };

    holt_spiLock();

    if (discreteSpiInit() != 0) {
        holt_spiUnlock();
        return 1;
    }

    struct spi_ioc_transfer readSenseXfer[2] = {
        {
            .tx_buf = (uintptr_t)tx_buf1,
            .rx_buf = 0,
            .len = sizeof(tx_buf1),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        },
        {
            .tx_buf = (uintptr_t)tx_buf2,
            .rx_buf = (uintptr_t)rx_buf,
            .len = sizeof(tx_buf2),
            .delay_usecs = 0,
            .speed_hz = speed,
            .bits_per_word = bits
        }
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(2), readSenseXfer) < 0) {
        perror("Failed to write/read SPI");
        close(fd);
        holt_spiUnlock();
        return 1;
    }

    discrete_state = rx_buf[0];

    close(fd);
    holt_spiUnlock();

    return discrete_state;
}

void WriteProgramSenseBanks(uint8_t bank_settings) {
    uint8_t tx_buf[2];

    holt_spiLock();

    if (discreteSpiInit() != 0) {
        holt_spiUnlock();
        return 1;
    }

    tx_buf[0] = HOLT_WRITE_SENSE_BANK;
    tx_buf[1] = bank_settings & 0xFF;

    struct spi_ioc_transfer writeSenseXfer = {
        .tx_buf = (uintptr_t)tx_buf,
        .rx_buf = 0,
        .len = sizeof(tx_buf),
        .delay_usecs = 0,
        .speed_hz = speed,
        .bits_per_word = bits
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &writeSenseXfer) < 0) {
        perror("Failed to write program sense banks to SPI");
        close(fd);
        holt_spiUnlock();
        return;
    }

    close(fd);
    holt_spiUnlock();
}
