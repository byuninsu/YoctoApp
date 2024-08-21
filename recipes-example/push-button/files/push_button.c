#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "push_button.h"

#define GPIO_CHIP "gpiochip1"
#define GPIO_PIN "127"

uint8_t GPU_API GetButtonState(void) {
    int ret;
    char command[64];
    char value[2] = {0};

    // gpioget command
    snprintf(command, sizeof(command), "gpioget %s %s", GPIO_CHIP, GPIO_PIN);

    // Run the command and get the result
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        perror("Unable to execute gpioget command");
        return -1;
    }

    if (fgets(value, sizeof(value), fp) == NULL) {
        perror("Unable to read gpioget output");
        pclose(fp);
        return -1;
    }

    pclose(fp);

    // Convert '0' or '1' to integer
    return (value[0] == '1') ? 1 : 0;
}



