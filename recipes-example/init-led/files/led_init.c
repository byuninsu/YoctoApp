#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "stm32_control.h"
#include "led_control.h"
#include "nvram_control.h"

volatile int led_init_running = 0;
pthread_t led_thread;

void* led_blink_thread(void* arg) {
    // Turn off 0, 1, 2번 LED
    setLedState(0x00, 0);
    setLedState(0x01, 0);
    setLedState(0x02, 0);

    // Blink 0번 LED
    while (led_init_running) {
        printf("ledInitBlinkThread while start ++");
        setLedState(0x00, 1);
        usleep(1000000);
        setLedState(0x00, 0);
        usleep(1000000);
    }
    return NULL;
}

void process_command(const char* cmd) {
    if (strcmp(cmd, "start") == 0) {
        if (!led_init_running) {
            led_init_running = 1;
            pthread_create(&led_thread, NULL, led_blink_thread, NULL);
        } else {
            printf("LED thread already running\n");
        }
    } else if (strcmp(cmd, "stop") == 0) {
        if (led_init_running) {
            led_init_running = 0;
            pthread_join(led_thread, NULL);
        } else {
            printf("LED thread not running\n");
        }
    }
}

int main() {
    char cmd[64];
    printf("LED service started. Waiting for commands...\n");

    while (1) {
        FILE* fp = fopen("/tmp/led_cmd", "r");
        if (fp) {
            if (fgets(cmd, sizeof(cmd), fp)) {
                cmd[strcspn(cmd, "\n")] = 0; // remove newline
                process_command(cmd);
            }
            fclose(fp);
            unlink("/tmp/led_cmd");
        }
        usleep(200000); // poll interval
    }

    return 0;
}
