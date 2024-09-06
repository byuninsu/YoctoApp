#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "libdiscrete_in.h"

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

    printf("spi mode: 0x%x\n", mode);
    printf("spi bits per word: %d\n", bits);
    printf("spi max speed: %d Hz (%d KHz)\n", speed, speed/1000);

    return 0; // 초기화 성공
}

uint32_t GetDiscreteState(void) {
    uint8_t tx_buf1[1];
    uint8_t tx_buf2[4] = { 0xFF }; // 더미 데이터 전송 버퍼
    uint8_t rx_buf[4] = { 0 };     // 데이터 수신 버퍼
    uint32_t discrete_state = 0;

    discreteSpiInit(); // SPI 초기화 함수 호출

    // SENSE<31:0> 읽기
    tx_buf1[0] = HOLT_READ_ALL; // 0xF8 명령어
    struct spi_ioc_transfer getDiscreteXfer[2] = {
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

    // SPI 통신 에러 처리
    if (ioctl(fd, SPI_IOC_MESSAGE(2), getDiscreteXfer) < 0) {
        perror("Failed to write/read SPI");
        close(fd);
        return 0xFFFFFFFF; // 에러 발생 시 특정 값을 반환
    }

    // 전송 버퍼 및 수신 버퍼 출력
    for (int i = 0; i < sizeof(tx_buf1); i++) {
        printf("GetDiscreteState tx_buf[%d] = 0x%02X\n", i, tx_buf1[i]);
    }

    for (int i = 0; i < sizeof(rx_buf); i++) {
        printf("GetDiscreteState rx_buf[%d] = 0x%02X\n", i, rx_buf[i]);
    }

    // SPI 디바이스 닫기
    close(fd);

    // 수신된 데이터 처리
    discrete_state = (rx_buf[0] << 24) | (rx_buf[1] << 16) | (rx_buf[2] << 8) | rx_buf[3];

    return discrete_state;
}

uint32_t GetDiscreteState7to0(void) {
    uint8_t tx_buf1[1];
    uint8_t tx_buf2[1] = { 0xFF }; // 더미 데이터 전송 버퍼
    uint8_t rx_buf[1] = { 0 }; // 데이터 수신 버퍼
    uint32_t discrete_state = 0;

    discreteSpiInit(); // SPI 초기화 함수 호출

    // SENSE<7:0> 읽기
    tx_buf1[0] = HOLT_READ_7_TO_0; // 0x90 명령어
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
        return 1;
    }
    discrete_state = rx_buf[0];

    for (int i = 0; i < sizeof(tx_buf1); i++) {
        printf("GetDiscreteState7to0 tx_buf[%d] = 0x%02X\n", i, tx_buf1[i]);
    }

    for (int i = 0; i < sizeof(rx_buf); i++) {
        printf("GetDiscreteState7to0 rx_buf[%d] = 0x%02X\n", i, rx_buf[i]);
    }

    // SPI 디바이스 닫기
    close(fd);

    return discrete_state;
}

uint32_t GetDiscreteState15to8(void) {
    uint8_t tx_buf1[1];
    uint8_t tx_buf2[1] = { 0xFF }; // 더미 데이터 전송 버퍼
    uint8_t rx_buf[1] = { 0 }; // 데이터 수신 버퍼
    uint32_t discrete_state = 0;

    discreteSpiInit(); // SPI 초기화 함수 호출

    // SENSE<15:8> 읽기
    tx_buf1[0] = HOLT_READ_15_TO_8; // 0x92 명령어
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
        return 1;
    }
    discrete_state = rx_buf[0];

    for (int i = 0; i < sizeof(tx_buf1); i++) {
        printf("GetDiscreteState15to8 tx_buf[%d] = 0x%02X\n", i, tx_buf1[i]);
    }

    for (int i = 0; i < sizeof(rx_buf); i++) {
        printf("GetDiscreteState15to8 rx_buf[%d] = 0x%02X\n", i, rx_buf[i]);
    }


    // SPI 디바이스 닫기
    close(fd);

    return discrete_state;
}

uint8_t ReadProgramSenseBanks(void) {
    uint8_t discrete_state = 0;
    uint8_t tx_buf1[1] = {HOLT_READ_SENSE_BANK};  //0x84 명령어
    uint8_t tx_buf2[1] = { 0xFF }; // 더미 데이터 전송 버퍼
    uint8_t rx_buf[1] = {0}; // 수신 버퍼

    discreteSpiInit(); // SPI 초기화 함수 호출

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
        return 1;
    }

    discrete_state = rx_buf[0];

    for (int i = 0; i < sizeof(tx_buf1); i++) {
        printf("ReadProgramSenseBanks tx_buf[%d] = 0x%02X\n", i, tx_buf1[i]);
    }

    for (int i = 0; i < sizeof(rx_buf); i++) {
        printf("ReadProgramSenseBanks rx_buf[%d] = 0x%02X\n", i, rx_buf[i]);
    }

    // SPI 디바이스 닫기
    close(fd);

    return discrete_state;
}

void WriteProgramSenseBanks(uint8_t bank_settings) {
    uint8_t tx_buf[2];

    printf("WriteProgramSenseBanks input Value : 0x%04X\n", bank_settings);

    discreteSpiInit(); // SPI 초기화 함수 호출

    // PSEN 레지스터 설정
    tx_buf[0] = HOLT_WRITE_SENSE_BANK; // 0x04 명령어
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
        return;
    }

    for (int i = 0; i < sizeof(tx_buf); i++) {
        printf("WriteProgramSenseBanks tx_buf[%d] = 0x%02X\n", i, tx_buf[i]);
    }


    // SPI 디바이스 닫기
    close(fd);
}

