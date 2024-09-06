#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "mcu_watchdog.h"
#include "stm32_control.h"

bool isStartWatchdog = false;

int main (int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s {start|stop|settime <timeout>}\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "start") == 0) {
        printf("start Watchdog ! \n");
         sendStartWatchdog();

        isStartWatchdog = true;

         while (isStartWatchdog) {
            printf("Watchdog sendKeepAlive ==> \n");
            usleep(500000);
            sendKeepAlive();
         }

    } else if (strcmp(argv[1], "stop") == 0) {
        printf("stop Watchdog ! \n");
        sendStopWatchdog();
        isStartWatchdog = false;

    } else if (strcmp(argv[1], "settime") == 0) {
        uint8_t timeout = atoi(argv[2]);

        sendCommandTimeout(timeout);
    }

}