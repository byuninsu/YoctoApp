#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include "libled_control.h"

// 상수 정의
#define LED_CONTROLLER_ADDR 0x74 // TCA9539의 변경 I2C 주소(led)
#define DISCRETE_OUT_ADDR 0x75 // TCA9539의 변경 I2C 주소(discrete-out)

static int i2c_fd = -1;

typedef enum {
    GPIO_EXPENDER_00P = 0x02,
    GPIO_EXPENDER_01P = 0x03,
    GPIO_EXPENDER_CONF_00P = 0x06,
    GPIO_EXPENDER_CONF_01P = 0x07
} gpio_expender_commands;

uint8_t i2cInit(uint8_t addr) {
    // I2C 버스가 열려있지 않으면 열기
    if (i2c_fd < 0) {
        i2c_fd = open("/dev/i2c-2", O_RDWR);
        if (i2c_fd < 0) {
            perror("Failed to open I2C bus");
            return 1;
        }

        // TCA9539의 I2C 주소 설정
        if (ioctl(i2c_fd, I2C_SLAVE, addr) < 0) {
            perror("Failed to select I2C device");
            close(i2c_fd);
            i2c_fd = -1;
            return 1;
        }
    }
    
    return 0;
}

// I2C 장치에서 바이트를 읽는 함수
int i2c_read_byte(unsigned char reg, unsigned char *value) {
    if (write(i2c_fd, &reg, 1) != 1) {
        perror("Failed to write to the i2c bus");
        return -1;
    }
    if (read(i2c_fd, value, 1) != 1) {
        perror("Failed to read from the i2c bus");
        return -1;
    }
    return 0;
}

// I2C 장치에 바이트를 쓰는 함수
int i2c_write_byte(unsigned char reg, unsigned char value) {
    unsigned char buffer[2];
    buffer[0] = reg;
    buffer[1] = value;
    if (write(i2c_fd, buffer, 2) != 2) {
        perror("Failed to write to the i2c bus");
        return -1;
    }
    return 0;
}

uint32_t setLedState(uint8_t gpio, uint16_t value) {
    unsigned char reg  = 0;
    unsigned char port_value;
    unsigned char new_value;
    uint8_t currentValue = 0;
    unsigned char mask = 1 << (gpio % 8);
    uint8_t original_gpio = gpio; // 원래의 gpio 값을 보존

    i2cInit(LED_CONTROLLER_ADDR);

    if(gpio < 8){
        reg = GPIO_EXPENDER_CONF_00P;
    }else if (gpio >= 8 ){
        reg = GPIO_EXPENDER_CONF_01P;
    }

    if (i2c_read_byte(reg, &currentValue) != 0) {
        fprintf(stderr, "Failed to read from I2C device\n");
        return 1;
    }

    if(currentValue != 0x00){
        i2c_write_byte(reg,0x00);
        printf("set configuration Value : 0xFF"); 
    }


    if (gpio < 8) {
        // Output Port 0의 현재 값 읽기
        i2c_read_byte(GPIO_EXPENDER_00P, &port_value);
        // 새로운 값 설정
        if (value) {
            new_value = port_value | mask; // 비트 설정
        } else {
            new_value = port_value & ~mask; // 비트 클리어
        }
        // Output Port 0에 새로운 값 쓰기
        i2c_write_byte(GPIO_EXPENDER_00P, new_value);
    } else {
        gpio -= 8; // 포트 1의 GPIO 핀 번호 조정
        // Output Port 1의 현재 값 읽기
        i2c_read_byte(GPIO_EXPENDER_01P, &port_value);
        // 새로운 값 설정
        if (value) {
            new_value = port_value | mask; // 비트 설정
        } else {
            new_value = port_value & ~mask; // 비트 클리어
     
        }
        // Output Port 1에 새로운 값 쓰기
        i2c_write_byte(GPIO_EXPENDER_01P, new_value);
    }

    printf("Set GPIO %d to value %d\n", original_gpio, value); // 원래의 gpio 값을 사용하여 출력

    return 0;
}


uint8_t getGpioState(uint8_t gpio) {
    unsigned char mask = 1 << (gpio % 8);
    unsigned char reg = 0;
    unsigned char currentValue = 0;

    // GPIO 범위 확인
    if (gpio >= 16) {
        fprintf(stderr, "Invalid GPIO number: %d\n", gpio);
        return 1;
    }

    i2cInit(LED_CONTROLLER_ADDR);

    if (gpio < 0x08) {
        reg = GPIO_EXPENDER_00P;
    } else {
        reg = GPIO_EXPENDER_01P;
    }

    if (i2c_read_byte(reg, &currentValue) != 0) {
        fprintf(stderr, "Failed to read from I2C device\n");
        return 1;
    }

    uint8_t gpioState = (currentValue & mask) ? 1 : 0;

    printf("Current GPIO state %d: %d\n", gpio, gpioState);

    return gpioState;
}

uint32_t setGpioConf(uint8_t port, uint8_t value) {

    printf("setGpioConf inpu value : %d\n", value);

    i2cInit(LED_CONTROLLER_ADDR);

    uint8_t reg = (port == 0) ? GPIO_EXPENDER_CONF_00P : GPIO_EXPENDER_CONF_01P;

    if (i2c_write_byte(reg, value) != 0) {
        fprintf(stderr, "Failed to write to I2C device\n");
        return 1;
    }

    printf("Set GPIO configuration for port %d to value 0x%02x\n", port, value);
    return 0;
}

uint32_t GPU_API getConfState(uint8_t port, uint8_t *value) {
    // 포트 범위 확인
    if (port > 1) {
        fprintf(stderr, "Invalid port number: %d\n", port);
        return 1;
    }

    i2cInit(LED_CONTROLLER_ADDR);

    unsigned char reg = (port == 0) ? GPIO_EXPENDER_CONF_00P : GPIO_EXPENDER_CONF_01P;

    if (i2c_read_byte(reg, value) != 0) {
        fprintf(stderr, "Failed to read from I2C device\n");
        return 1;
    }

    printf("Current configuration state %d: 0x%02x\n", port, *value);
    return 0;
}


uint32_t setDiscreteOut(uint8_t gpio, uint16_t value) {
    unsigned char reg  = 0;
    unsigned char initReg  = 0;
    unsigned char port_value;
    unsigned char new_value;
    uint8_t currentValue = 0;
    unsigned char mask = 1 << (gpio % 8);
    uint8_t original_gpio = gpio; // 원래의 gpio 값을 보존

    i2cInit(DISCRETE_OUT_ADDR);

    if(gpio < 8){
        reg = GPIO_EXPENDER_CONF_00P;
        initReg = GPIO_EXPENDER_00P;
    }else if (gpio >= 8 ){
        reg = GPIO_EXPENDER_CONF_01P;
        initReg = GPIO_EXPENDER_01P;
    }

    if (i2c_read_byte(reg, &currentValue) != 0) {
        fprintf(stderr, "Failed to read from I2C device\n");
        return 1;
    }

    if(currentValue != 0x00){
        i2c_write_byte(reg,0x00);
        i2c_write_byte(initReg, 0x00);
        printf("set configuration Value : 0xFF"); 
    }


    if (gpio < 8) {
        // Output Port 0의 현재 값 읽기
        i2c_read_byte(GPIO_EXPENDER_00P, &port_value);
        // 새로운 값 설정
        if (value) {
            new_value = port_value | mask; // 비트 설정
        } else {
            new_value = port_value & ~mask; // 비트 클리어
        }
        // Output Port 0에 새로운 값 쓰기
        i2c_write_byte(GPIO_EXPENDER_00P, new_value);
    } else {
        gpio -= 8; // 포트 1의 GPIO 핀 번호 조정
        // Output Port 1의 현재 값 읽기
        i2c_read_byte(GPIO_EXPENDER_01P, &port_value);
        // 새로운 값 설정
        if (value) {
            new_value = port_value | mask; // 비트 설정
        } else {
            new_value = port_value & ~mask; // 비트 클리어
        }
        // Output Port 1에 새로운 값 쓰기
        i2c_write_byte(GPIO_EXPENDER_01P, new_value);
    }

    printf("Set GPIO %d to value %d\n", original_gpio, value); // 원래의 gpio 값을 사용하여 출력

    return 0;
}


uint8_t getDiscreteOut(uint8_t gpio) {
    unsigned char mask = 1 << (gpio % 8);
    unsigned char reg = 0;
    unsigned char currentValue = 0;

    // GPIO 범위 확인
    if (gpio >= 16) {
        fprintf(stderr, "Invalid GPIO number: %d\n", gpio);
        return 1;
    }

    i2cInit(DISCRETE_OUT_ADDR);

    if (gpio < 0x08) {
        reg = GPIO_EXPENDER_00P;
    } else {
        reg = GPIO_EXPENDER_01P;
    }

    if (i2c_read_byte(reg, &currentValue) != 0) {
        fprintf(stderr, "Failed to read from I2C device\n");
        return 1;
    }

    uint8_t gpioState = (currentValue & mask) ? 1 : 0;

    printf("Current GPIO state %d: %d\n", gpio, gpioState);

    return gpioState;
}

uint32_t setDiscreteConf(uint8_t port, uint8_t value) {

    printf("setDiscreteConf inpu value : %d\n", value);

    i2cInit(DISCRETE_OUT_ADDR);

    uint8_t reg = (port == 0) ? GPIO_EXPENDER_CONF_00P : GPIO_EXPENDER_CONF_01P;

    if (i2c_write_byte(reg, value) != 0) {
        fprintf(stderr, "Failed to write to I2C device\n");
        return 1;
    }

    printf("Set GPIO configuration for port %d to value 0x%02x\n", port, value);
    return 0;
}

uint32_t getDiscreteConf(uint8_t port, uint8_t *value) {
    // 포트 범위 확인
    if (port > 1) {
        fprintf(stderr, "Invalid port number: %d\n", port);
        return 1;
    }

    i2cInit(DISCRETE_OUT_ADDR);

    unsigned char reg = (port == 0) ? GPIO_EXPENDER_CONF_00P : GPIO_EXPENDER_CONF_01P;

    if (i2c_read_byte(reg, value) != 0) {
        fprintf(stderr, "Failed to read from I2C device\n");
        return 1;
    }

    printf("Current configuration state %d: 0x%02x\n", port, *value);
    return 0;
}
