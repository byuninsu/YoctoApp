#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watchdog_control.h"
#include "usb_control.h"
#include "stm32_control.h"
#include "rs232_control.h"
#include "push_button.h"
#include "nvram_control.h"
#include "led_control.h"
#include "ethernet_control.h"
#include "discrete_in.h"
#include "optic_control.h"
#include "bit_manager.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [options]\n", argv[0]);
        return 1;
    }

    // Handling 'discrete' commands
    if (strcmp(argv[1], "discrete") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s discrete <in|out> <SENSE|set|get> [args...]\n", argv[0]);
            return 1;
        }

        if (strcmp(argv[2], "in") == 0) {
            if (strcmp(argv[3], "read") == 0) {
                if (strcmp(argv[4], "0") == 0) {
                    printf("Discrete Value (7:0): 0x%02X\n", GetDiscreteState7to0());
                } else if (strcmp(argv[4], "1") == 0) {
                    printf("Discrete Value (15:8): 0x%02X\n", GetDiscreteState15to8());
                } else if (strcmp(argv[4], "all") == 0) {
                    printf("Discrete Value (15:0): 0x%04X\n", GetDiscreteState());
                }
            } else if (strcmp(argv[3], "SENSE") == 0) {
                if (strcmp(argv[4], "write") == 0) {
                    uint8_t value = (uint8_t)strtoul(argv[5], NULL, 0);
                    printf("main input Value: 0x%02X\n", value);
                    WriteProgramSenseBanks(value);
                } else if (strcmp(argv[4], "read") == 0) {
                    printf("HOLT set Sense Value: 0x%02X\n", ReadProgramSenseBanks());
                } 
            }

        } else if (strcmp(argv[2], "out") == 0) {
            if (strcmp(argv[3], "set") == 0) {
                if (strcmp(argv[4], "gpio") == 0) {
                    uint8_t gpio = (uint8_t)strtol(argv[5], NULL, 16);
                    uint8_t value = (uint8_t)strtol(argv[6], NULL, 16);
                    if (gpio >= 0 && gpio <= 15 && (value == 0 || value == 1)) {
                        setDiscreteOut(gpio, value);
                    } else {
                        printf("Invalid GPIO number or value. GPIO should be 0-15 and value should be 0 or 1.\n");
                    }
                } else if (strcmp(argv[4], "conf") == 0) {
                    uint8_t port = (uint8_t)strtol(argv[5], NULL, 16);
                    uint8_t value = (uint8_t)strtol(argv[6], NULL, 16);
                    printf("main setGpioConf inpu value : %d\n", value);
                    setGpioConf(port, value);
                }
            } else if (strcmp(argv[3], "get") == 0) {
                if (strcmp(argv[4], "conf") == 0) {
                    uint8_t port = (uint8_t)strtol(argv[5], NULL, 16);
                    uint8_t value;
                    getConfState(port, &value);
                } else if (strcmp(argv[4], "gpio") == 0) {
                    uint8_t port = (uint8_t)strtol(argv[5], NULL, 16);
                    getGpioState(port);
                }
            } else {
                printf("Invalid command. Use set, conf, or get.\n");
            }

        }
    }

    // Handling 'ethernet' commands
    else if (strcmp(argv[1], "ethernet") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s ethernet <1~9> <set|get> <gpio|conf> [args...]\n", argv[0]);
            return 1;
        }
        if (strcmp(argv[2], "get") == 0) {
            int port = atoi(argv[3]);
            getEthernetPort(port);
            
        } else if (strcmp(argv[2], "set") == 0) {
            int port = atoi(argv[3]);
            int value = atoi(argv[4]);
            setEthernetPort(port, value);
        }
    }

    // Handling 'led' commands
    else if (strcmp(argv[1], "led") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s led <set|get> <gpio|conf> [args...]\n", argv[0]);
            return 1;
        }

        if (strcmp(argv[2], "set") == 0) {
            if (strcmp(argv[3], "gpio") == 0) {
                uint8_t gpio = (uint8_t)strtol(argv[4], NULL, 16);
                uint8_t value = (uint8_t)strtol(argv[5], NULL, 16);
                if (gpio >= 0 && gpio <= 15 && (value == 0 || value == 1)) {
                    setLedState(gpio, value);
                } else {
                    printf("Invalid GPIO number or value. GPIO should be 0-15 and value should be 0 or 1.\n");
                }
            } else if (strcmp(argv[3], "conf") == 0) {
                uint8_t port = (uint8_t)strtol(argv[4], NULL, 16);
                uint8_t value = (uint8_t)strtol(argv[5], NULL, 16);
                printf("main setGpioConf inpu value : %d\n", value);
                setGpioConf(port, value);
            }
        } else if (strcmp(argv[2], "get") == 0) {
            if (strcmp(argv[3], "conf") == 0) {
                uint8_t port = (uint8_t)strtol(argv[4], NULL, 16);
                uint8_t value;
                getConfState(port, &value);
            } else if (strcmp(argv[3], "gpio") == 0) {
                uint8_t port = (uint8_t)strtol(argv[4], NULL, 16);
                getGpioState(port);
            }
        } else {
            printf("Invalid command. Use set, conf, or get.\n");
        }
    }

    // Handling 'nvram' commands
else if (strcmp(argv[1], "nvram") == 0) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s nvram <write|read> <maintenance|bootcondition|system> [args...]\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[2], "write") == 0) {
        if (strcmp(argv[3], "maintenance") == 0) {
            uint8_t state = (uint8_t)strtoul(argv[4], NULL, 0); 
            WriteMaintenanceModeStatus(state);
        } else if (strcmp(argv[3], "bootcondition") == 0) {
            uint32_t bootingConditionState = (uint32_t)strtoul(argv[4], NULL, 0);
            WriteBootCondition(bootingConditionState);
        } else if (strcmp(argv[3], "system") == 0) {
            uint32_t addr = (uint32_t)strtoul(argv[4], NULL, 0); 
            WriteSystemLogReasonCount(addr);
        } else {
            fprintf(stderr, "Invalid write type: %s\n", argv[3]);
        }
    } else if (strcmp(argv[2], "read") == 0) {
        if (strcmp(argv[3], "maintenance") == 0) {
            uint32_t result = ReadMaintenanceModeStatus();
            printf("Reading maintenance Value: %u\n", result); 

        } else if (strcmp(argv[3], "bootcondition") == 0) {
            uint32_t result = ReadBootCondition();
            printf("Reading bootcondition Value: %u\n", result); 

        } else if (strcmp(argv[3], "system") == 0) {
            uint32_t addr = (uint32_t)strtoul(argv[4], NULL, 0);
            uint32_t result = ReadSystemLog(addr);
            printf("Reading systemLog Value at 0x%08x: %u\n", addr, result); 

        } else {
            fprintf(stderr, "Invalid read type: %s\n", argv[3]);
        }
    } else {
        fprintf(stderr, "Invalid action: %s\n", argv[2]);
    }
}

    // Handling 'pushbutton get' command
    else if (strcmp(argv[1], "pushbutton") == 0) {
        if (argc < 3 || strcmp(argv[2], "get") != 0) {
            fprintf(stderr, "Usage: %s pushbutton get\n", argv[0]);
            return 1;
        }

        uint8_t state = GetButtonState();
        if (state == 1) {
            printf("Push Button State: OFF\n");
        } else if (state == 0) {
            printf("Push Button State: ON\n");
        } else {
            printf("Failed to read Push Button state.\n");
        }
    }

    // Handling 'rs232' commands
    else if (strcmp(argv[1], "rs232") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s rs232 <enable|disable>\n", argv[0]);
            return 1;
        }

        if (strcmp(argv[2], "enable") == 0) {
            ActivateRS232();
        } else if (strcmp(argv[2], "disable") == 0) {
            DeactivateRS232();
        }
    }

    // Handling 'stm32' commands
    else if (strcmp(argv[1], "stm32") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s stm32 <temp|vol|current>\n", argv[0]);
            return 1;
        }

        if (strcmp(argv[2], "temp") == 0) {
            printf("Temperature: %.2f\n", getTempSensorValue());
        } else if (strcmp(argv[2], "vol") == 0) {
            printf("Temperature: %.2f\n", getVoltageValue());
        } else if (strcmp(argv[2], "current") == 0) {
            printf("Temperature: %.2f\n", getCurrentValue());
        }
    }

    // Handling 'usb' commands
    else if (strcmp(argv[1], "usb") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s usb <enable|disable>\n", argv[0]);
            return 1;
        }

        if (strcmp(argv[2], "enable") == 0) {
            ActivateUSB();
        } else if (strcmp(argv[2], "disable") == 0) {
            DeactivateUSB();
        }
    }

    // Handling 'watchdog' commands
    else if (strcmp(argv[1], "watchdog") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s watchdog <start|stop>\n", argv[0]);
            return 1;
        }

        if (strcmp(argv[2], "start") == 0) {
            StartWatchDog();
        } else if (strcmp(argv[2], "stop") == 0) {
            StopWatchDog();
        }
    }

    // Handling 'optic' commands
    else if (strcmp(argv[1], "optic") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: %s optic <get/set>\n", argv[0]);
            return 1;
        }

        if (strcmp(argv[2], "test") == 0) {
            getOpticTestRegister();
        } 

        if (strcmp(argv[2], "set") == 0) {
            setOpticPort();
        } 
    }

    // BIT Check commands
    else if (strcmp(argv[1], "check") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: %s check <options>\n", argv[0]);
            return 1;
        }

        const char *option = argv[2];

        if (strcmp(option, "gpio") == 0) {
            check_gpio_expander();
        } else if (strcmp(option, "ssd") == 0) {
            char *option = argv[3];
            if (strcmp(option, "os") == 0 ){
                check_ssd("os");
            }else if (strcmp(option, "data") == 0){
                check_ssd("data");
            }
        } else if (strcmp(option, "discrete") == 0) {
            char *option = argv[3];
            if (strcmp(option, "out") == 0 ){
                check_discrete_out();
            }else if (strcmp(option, "in") == 0){
                checkDiscrete_in();
            }
        } else if (strcmp(option, "ethernet") == 0){
            checkEthernet();
        } else if (strcmp(option, "gpu") == 0){
            checkGPU();
        } else if (strcmp(option, "nvram") == 0){
            checkNvram();
        } else if (strcmp(option, "rs232") == 0){
            checkRs232();
        } else if (strcmp(option, "switch") == 0){
            checkEthernetSwitch();
        } else if (strcmp(option, "temp") == 0){
            checkTempSensor();
        } else if (strcmp(option, "power") == 0){
            checkPowerMonitor();
        } else if (strcmp(option, "usb") == 0){
            checkUsb();
        } else if (strcmp(option, "optic") == 0){
            checkOptic();
        } else if (strcmp(option, "read") == 0){
            uint32_t type = (uint32_t)strtoul(argv[3], NULL, 0); 
            const char *type_str;

            switch (type) {
                case 2:
                    type_str = "power-on";
                    break;
                case 3:
                    type_str = "continuous";
                    break;
                case 4:
                    type_str = "initiated";
                    break;
                default:
                    type_str = "unknown";
                    break;
            }

            uint32_t result = readtBitResult(type);


            printf("BIT Result Value [%s]: %u\n", type_str, readtBitResult(result));

        } else if (strcmp(option, "all") == 0){
            uint32_t type = (uint32_t)strtoul(argv[3], NULL, 0); 
            RequestBit(type);
        } else {
            printf("Invalid option. Use 'gpio', 'ssd', or 'discrete'.\n");
            return 1;
        }
    }

    else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
