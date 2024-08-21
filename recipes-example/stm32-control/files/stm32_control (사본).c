#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include "stm32_control.h"

#define SPI_DEVICE "/dev/spidev0.0"
#define SPI_SPEED 20000000 // 20MHz

    int fd;

uint32_t spiInit() {
            // SPI 디바이스 열기
            fd = open(SPI_DEVICE, O_RDWR);
            
            if (fd < 0) {
                 perror("Failed to open SPI device");
                 return 0;
            }
            
            // SPI 속도 설정
            if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, SPI_SPEED) < 0) {
                perror("Failed to set SPI speed");
                close(fd);
                return -1;
            }
}

uint32_t GPU_API GetDiscreteState(void) {
    uint8_t tx_buf[2];
    uint8_t rx_buf[2];
    uint32_t discrete_state = 0;

    spiInit(); // SPI 초기화 함수 호출

    // SENSE<7:0> 읽기
    tx_buf[0] = 0x90; // 0x90 명령어
    
    if (write(fd, tx_buf, 1) != 1) {
        perror("Failed to write SPI command");
        close(fd);
        return 0;
    }
    if (read(fd, rx_buf, 1) != 1) {
        perror("Failed to read SPI response");
        close(fd);
        return 0;
    }
    
    discrete_state = rx_buf[0];

    // SENSE<15:8> 읽기
    tx_buf[0] = 0x92; // 0x92 명령어
    if (write(fd, tx_buf, 1) != 1) {
        perror("Failed to write SPI command");
        close(fd);
        return 0;
    }
    if (read(fd, rx_buf, 1) != 1) {
        perror("Failed to read SPI response");
        close(fd);
        return 0;
    }
    
    discrete_state |= (rx_buf[0] << 8);

    close(fd); // SPI 디바이스 닫기

    return discrete_state;
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <read>\n", argv[0]);
        return 1;
    }


    if (strcmp(argv[1], "read") == 0) {
    
        int discrete_status = GetDiscreteState();
        
        if (discrete_status >= 0) {
            printf("Current discrete_status: 0x%02X\n", discrete_status);
        } else {
            printf("Failed to get discrete_status.\n");
        }
    } else {
        printf("Unknown command. Usage: %s <read>\n", argv[0]);
    }

    close(fd);
    return 0;
}

