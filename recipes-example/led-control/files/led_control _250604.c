#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include "led_control.h"
#include "stm32_control.h"

// 상수 정의
#define LED_CONTROLLER_ADDR 0x74 // TCA9539의 변경 I2C 주소(led)
#define DISCRETE_OUT_ADDR 0x75 // TCA9539의 변경 I2C 주소(discrete-out)

static int i2c_fd = -1;

typedef enum {
    GPIO_EXPENDER_INPUT_00P = 0x00,
    GPIO_EXPENDER_INPUT_01P = 0x01,
    GPIO_EXPENDER_00P = 0x02,
    GPIO_EXPENDER_01P = 0x03,
    GPIO_EXPENDER_CONF_00P = 0x06,
    GPIO_EXPENDER_CONF_01P = 0x07
} gpio_expender_commands;


static const char* i2c_device_path_old = "/dev/i2c-1";
static const char* i2c_device_path_qt = "/dev/i2c-2";

static int i2c_fd = -1;
static pthread_mutex_t i2c_mutex = PTHREAD_MUTEX_INITIALIZER;

uint8_t i2cInit(uint8_t addr) {
     pthread_mutex_lock(&i2c_mutex);

    // I2C 버스가 열려있지 않으면 열기
    if (i2c_fd < 0) {
        i2c_fd = open(i2c_device_path_old, O_RDWR);
        if (i2c_fd < 0) {
            perror("Failed to open I2C bus");
            thread_mutex_unlock(&i2c_mutex);
            return 1;
        }

        // TCA9539의 I2C 주소 설정
        if (ioctl(i2c_fd, I2C_SLAVE, addr) < 0) {
            perror("Failed to select I2C device");
            close(i2c_fd);
            i2c_fd = -1;
            thread_mutex_unlock(&i2c_mutex);
            return 1;
        }
    }

    thread_mutex_unlock(&i2c_mutex);
    return 0;
}

void i2cClose() {
    if (i2c_fd >= 0) {
        close(i2c_fd);
        i2c_fd = -1;  // 파일 디스크립터를 초기화하여 재사용 방지
    }
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

uint8_t setLedState(uint8_t gpio, uint8_t value) {
    sendSetLedState(gpio, value);
}

uint8_t getGpioState(uint8_t gpio) {
    return sendGetLedState(gpio);
}

uint8_t setGpioConf(uint8_t port, uint8_t value) {
    sendConfSetLedState(port, value);
}

uint8_t getConfState(uint8_t port) {
    return sendConfGetLedState(port);
}


uint32_t setDiscreteOut(uint8_t gpio, uint16_t value) {
    unsigned char reg = 0;
    unsigned char conf_reg = 0;
    unsigned char port_value;
    unsigned char new_value;
    unsigned char mask = 1 << (gpio % 8);
    uint8_t original_gpio = gpio;

    i2cInit(DISCRETE_OUT_ADDR);

    if (gpio < 8) {
        reg = GPIO_EXPENDER_00P;
        conf_reg = GPIO_EXPENDER_CONF_00P;
    } else {
        gpio -= 8;  // 포트 1의 GPIO 핀 번호 조정
        reg = GPIO_EXPENDER_01P;
        conf_reg = GPIO_EXPENDER_CONF_01P;
    }

    // 현재 출력 포트 값 읽기
    if (i2c_read_byte(reg, &port_value) != 0) {
        fprintf(stderr, "Failed to read from I2C device (reg: 0x%x)\n", reg);
        i2cClose();
        return 1;
    }

    // 새로운 포트 값 계산
    if (value) {
        new_value = port_value | mask; // 비트 설정
    } else {
        new_value = port_value & ~mask; // 비트 클리어
    }

    // 출력 포트에 새로운 값 쓰기
    if (i2c_write_byte(reg, new_value) != 0) {
        fprintf(stderr, "Failed to write to I2C device (reg: 0x%x)\n", reg);
        i2cClose();
        return 1;
    }

    // 설정 레지스터(conf) 읽기
    uint8_t conf_value;
    if (i2c_read_byte(conf_reg, &conf_value) != 0) {
        fprintf(stderr, "Failed to read configuration register (reg: 0x%x)\n", conf_reg);
        i2cClose();
        return 1;
    }

    // 해당 GPIO가 아직 입력 모드(1)인 경우만 출력으로 변경
    if (conf_value & mask) {
        conf_value &= ~mask; // 해당 비트만 0으로 변경 (출력 설정)
        if (i2c_write_byte(conf_reg, conf_value) != 0) {
            fprintf(stderr, "Failed to write configuration register (reg: 0x%x)\n", conf_reg);
            i2cClose();
            return 1;
        }
        printf("GPIO %d mode set to output (0)\n", original_gpio);
    } else {
        printf("GPIO %d is already set as output.\n", original_gpio);
    }

    printf("Set GPIO %d to value %d\n", original_gpio, value);

    i2cClose();
    return 0;
}

uint32_t setDiscreteOutAll(uint16_t value) {
    uint8_t port0_value = (uint8_t)(value & 0xFF);      // 하위 8비트는 Port 0에 할당
    uint8_t port1_value = (uint8_t)((value >> 8) & 0xFF); // 상위 8비트는 Port 1에 할당

    // I2C 초기화
    i2cInit(DISCRETE_OUT_ADDR);

    // Port 0 설정
    if (i2c_write_byte(GPIO_EXPENDER_CONF_00P, 0x00) != 0 ||
        i2c_write_byte(GPIO_EXPENDER_00P, port0_value) != 0) {
        fprintf(stderr, "Failed to write to Port 0\n");
        i2cClose();
        return 1;
    }

    // Port 1 설정
    if (i2c_write_byte(GPIO_EXPENDER_CONF_01P, 0x00) != 0 ||
        i2c_write_byte(GPIO_EXPENDER_01P, port1_value) != 0) {
        fprintf(stderr, "Failed to write to Port 1\n");
        i2cClose();
        return 1;
    }

    printf("Set GPIOs to value: 0x%04X\n", value);

    // I2C 닫기
    i2cClose();

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

    i2cClose();

    return gpioState;
}

uint16_t getDiscreteOutAll(void) {
    uint16_t gpioState = 0;
    unsigned char reg0Value = 0;
    unsigned char reg1Value = 0;

    // I2C 초기화
    i2cInit(DISCRETE_OUT_ADDR);

    // 첫 번째 8비트 (GPIO 0~7) 읽기
    if (i2c_read_byte(GPIO_EXPENDER_00P, &reg0Value) != 0) {
        fprintf(stderr, "Failed to read from I2C device (GPIO 0~7)\n");
        i2cClose();
        return 0xFFFF; // 에러 표시로 모두 1을 반환
    }

    // 두 번째 8비트 (GPIO 8~15) 읽기
    if (i2c_read_byte(GPIO_EXPENDER_01P, &reg1Value) != 0) {
        fprintf(stderr, "Failed to read from I2C device (GPIO 8~15)\n");
        i2cClose();
        return 0xFFFF; // 에러 표시로 모두 1을 반환
    }

    // 16비트로 결합
    gpioState = (reg1Value << 8) | reg0Value;

    printf("Current GPIO states: 0x%04X\n", gpioState);

    i2cClose();

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

    i2cClose();

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

    i2cClose();
    
    return 0;
}

uint32_t setDiscreteInput(uint8_t port, uint8_t value) {
    printf("setDiscreteInput inpu value : %d\n", value);
    uint8_t reg = 0;

    i2cInit(DISCRETE_OUT_ADDR);

    if(port < 7) {
        reg = GPIO_EXPENDER_INPUT_00P;
    } else {
        reg = GPIO_EXPENDER_INPUT_01P;
    }

    if (i2c_write_byte(reg, value) != 0) {
        fprintf(stderr, "Failed to write to I2C device\n");
        return 1;
    }

    printf("Set GPIO Input for port %d to value 0x%02x\n", port, value);

    i2cClose();

    return 0;
}

uint32_t getDiscreteInput(uint8_t port, uint8_t *value) {
    uint8_t reg = 0;

    i2cInit(DISCRETE_OUT_ADDR);

    if(port < 7) {
        reg = GPIO_EXPENDER_INPUT_00P;
    } else {
        reg = GPIO_EXPENDER_INPUT_01P;
    }

    if (i2c_read_byte(reg, value) != 0) {
        fprintf(stderr, "Failed to read from I2C device\n");
        return 1;
    }

    printf("Current Input state %d: 0x%02x\n", port, *value);

    i2cClose();
    
    return 0;
}


