#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
#include "cJSON.h"

#define BOOT_SIL_MODE 0x01
#define BOOT_NOMAL_MODE 0x00  

#define HOLDUP_LOG_FILE_DIR "/mnt/dataSSD/"
#define HOLDUP_LOG_FILE_NAME "HoldupLog.json"
#define LOG_FILE_PATH HOLDUP_LOG_FILE_DIR HOLDUP_LOG_FILE_NAME

int keepRunning = 1; // 프로그램 실행 플래그

void InitializeAllLEDs() {
    for (uint8_t gpio = 0x00; gpio <= 0x0B; gpio++) {
        if (setLedState(gpio, 0) != STATUS_SUCCESS) {
            printf("Failed to initialize GPIO 0x%02X\n", gpio);
        }
    }
    printf("All GPIOs initialized to 0.\n");
}

void startHoldupPFLogging() {
    printf(" Holdup CC Logging Started...\n");
    while (keepRunning) {
        uint8_t ccValue = sendRequestHoldupPF();  // SPI로 값 읽기
        printf("CC Value: %u\n", ccValue);
        writeLogToFile(ccValue);  // JSON 로그 저장
        usleep(10000);
    }
    printf("Logging Stopped.\n");
}

// JSON 로그 파일에 데이터 추가 함수
void writeLogToFile(uint8_t ccValue) {
    FILE *file = fopen(LOG_FILE_PATH, "r");
    cJSON *jsonLog = NULL;

    // 기존 JSON 로그 파일 읽기 (없으면 새로 생성)
    if (file) {
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        rewind(file);

        if (fileSize > 0) {

            char *jsonStr = (char *)malloc(fileSize + 1);
            if (jsonStr == NULL) {
                fclose(file);
                printf("ERROR: Memory allocation failed for jsonStr\n");
                return;
            }
            fread(jsonStr, 1, fileSize, file);
            jsonStr[fileSize] = '\0';


            fclose(file);

            jsonLog = cJSON_Parse(jsonStr);
            free(jsonStr);
        } else {
            fclose(file);
        }
    }

    // 기존 JSON 파일이 없거나 비어 있으면 새로운 JSON 배열 생성
    if (!jsonLog) {
        jsonLog = cJSON_CreateArray();
    }

    // 현재 시간 추가
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    if (t == NULL) {  
        printf("ERROR: localtime() failed!\n");
        return;
    }

    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", t);


    // 새 JSON 객체 생성 {"timestamp": "2025-02-07 12:00:00", "cc_value": 1}
    cJSON *logEntry = cJSON_CreateObject();
    cJSON_AddStringToObject(logEntry, "timestamp", timeStr);
    cJSON_AddNumberToObject(logEntry, "cc_value", ccValue);

    // JSON 배열에 추가
    cJSON_AddItemToArray(jsonLog, logEntry);

    // 파일에 저장
    file = fopen(LOG_FILE_PATH, "w");
    if (file) {
        char *jsonString = cJSON_Print(jsonLog);
        fprintf(file, "%s", jsonString);
        fclose(file);
        free(jsonString);
    }

    // 메모리 해제
    cJSON_Delete(jsonLog);
}

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
                    getDiscreteOut(port);
                } else if (strcmp(argv[4], "all") == 0) {
                    uint16_t discreteValue = getDiscreteOutAll();
                    printf("All discreteValue = 0x%04X\n", discreteValue);
                }
            } else {
                printf("Invalid command. Use set, conf, or get.\n");
            }

        }
    }

    else if (strcmp(argv[1], "boot") == 0 ) {
        if (strcmp(argv[2], "systemlog") == 0 ) {
            //Power ON Count는 기본적으로 1 증가
            WriteSystemLogReasonCount(2);

            //Maintenance Mode check
            //if(maintenance)
            WriteSystemLogReasonCount(4);

            //Check BootCondition
            uint32_t bootcondition = ReadBootCondition();
            if(bootcondition == 0x03){
                WriteSystemLogReasonCount(7);
            } else if (bootcondition == 0x04){
                WriteSystemLogReasonCount(3);
            } else if (bootcondition == 0x05){
                WriteSystemLogReasonCount(6);
            } else {
                WriteSystemLogReasonCount(8);
            }
        } 
    } 

    // Handling 'ethernet' commands
    else if (strcmp(argv[1], "ethernet") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s ethernet <set|get|> <1~9> <gpio|conf> [args...]\n", argv[0]);
            return 1;
        }
        if (strcmp(argv[2], "init") == 0) {
            uint16_t readValue; 
            uint32_t discreteIn7to0Value = GetDiscreteState7to0();
            uint32_t discreteIn15to8Value = GetDiscreteState15to8();

            readValue = (discreteIn15to8Value & 0xFF) <<8;
            readValue |= discreteIn7to0Value & 0xFF;

            int isHigh12 = readValue & (1 << 12);
            int isHigh13 = readValue & (1 << 13);
            int isHigh14 = readValue & (1 << 14);
            int isHigh15 = readValue & (1 << 15);

            if (isHigh12 && (isHigh13 == 0 && isHigh14 == 0 && isHigh15 ==0)) {
                printf("Ethernet init : Optic Set.\n");
                // Optic 전환
                setOpticPort();
            } 
        }
        if (strcmp(argv[2], "get") == 0) {
            int port = atoi(argv[3]);
            getEthernetPort(port);
            
        } else if (strcmp(argv[2], "set") == 0) {
            int port = atoi(argv[3]);
            int value = atoi(argv[4]);
            setEthernetPort(port, value);
        } else if (strcmp(argv[2], "nego") ==0 ) {
            char* otherIface = checkEthernetInterface();
            char* iface;

            if(strcmp( otherIface, "eth0") == 0 ){
                iface = "eth1";
            } else {
                iface = "eth0";
            }
            char command[256];
            if (strcmp(argv[3], "disable") ==0){
                snprintf(command, sizeof(command), "/usr/sbin/ethtool -s %s speed 1000 duplex full autoneg off", iface);
                printf("Disabling auto-negotiation on %s\n", iface);

            } else if (strcmp(argv[3], "enable") ==0) {
                snprintf(command, sizeof(command), "/usr/sbin/ethtool -s %s autoneg on", iface);
                printf("Enabling auto-negotiation on %s\n", iface);

            }
            int status = system(command);
            if (status == -1) {
                perror("Failed to execute ethtool command");
                return 1;
            }
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
            } else if (strcmp(argv[3], "status") == 0) {
                if(strcmp(argv[4], "green") == 0) {
                    InitializeAllLEDs();
                    if (setLedState(0x01, 1) == STATUS_SUCCESS) {
                        printf("LED 1 set to Green.\n");
                    } else {
                        printf("Failed to set LED 1 to Green.\n");
                    }
                } else if ( strcmp(argv[4], "red") == 0 ) {
                    InitializeAllLEDs();
                    if (setLedState(0x02, 1) == STATUS_SUCCESS) {
                        printf("LED 1 set to Red.\n");
                    } else {
                        printf("Failed to set LED 1 to Red.\n");
                    }
                } else if ( strcmp(argv[4], "yellow") == 0 ) {
                    InitializeAllLEDs();
                    if (setLedState(0x00, 1) == STATUS_SUCCESS) {
                        printf("LED 1 set to Yellow.\n");
                    } else {
                        printf("Failed to set LED 1 to Yellow.\n");
                    }
                }
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
    if (argc < 3) {
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
        } else if (strcmp(argv[3], "custom") == 0) {
            uint32_t addr = (uint32_t)strtoul(argv[4], NULL, 0); 
            uint32_t value = (uint32_t)strtoul(argv[5], NULL, 0);
            WriteSystemLogReasonCountCustom(addr, value);
        } else if (strcmp(argv[3], "time") == 0) {
            uint32_t time = (uint32_t)strtoul(argv[4], NULL, 0); 
            WriteCumulativeTime(time);
        } else if (strcmp(argv[3], "test") == 0) {
            uint32_t addr = (uint32_t)strtoul(argv[4], NULL, 0); 
            uint32_t value = (uint32_t)strtoul(argv[5], NULL, 0); 
            WriteBitResult(addr,value);
        } else if (strcmp(argv[3], "ssd") == 0) {
            const struct hwCompatInfo myInfo = {
                .supplier_part_no = "22-00155668",
                .ssd0_model_no = "TS320GMTE560I", 
                .ssd0_serial_no = "I487060005",
                .ssd1_model_no = "EXPI4M7680GB",
                .ssd1_serial_no = "X08TZB3R042511",
                .sw_part_number = "HSC-OESG-IRIS"
            };
            WriteHwCompatInfoToNVRAM(&myInfo);
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

        } else if (strcmp(argv[3], "custom") == 0) {
            uint32_t addr = (uint32_t)strtoul(argv[4], NULL, 0);
            uint32_t result = ReadSystemLogReasonCountCustom(addr);
            printf("Reading systemLog Value at 0x%08x: %u\n", addr, result); 

        } else if (strcmp(argv[3], "time") == 0) {
            uint32_t result = ReadCumulativeTime();
            printf("Reading CumulativeTime Value: %u\n", result); 
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
            printf("Push Button State: ON\n");
        } else if (state == 0) {
            printf("Push Button State: OFF\n");
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
            printf("Voltage: %.2f\n", getVoltageValue());
        } else if (strcmp(argv[2], "current") == 0) {
            printf("Current: %.2f\n", getCurrentValue());
        } else if (strcmp(argv[2], "bootcondition") == 0) {
            sendBootCondition();
        } else if (strcmp(argv[2], "holdupcc") == 0) {
            uint8_t returnValue = sendRequestHoldupCC();
            printf("sendRequestHoldupCC Value = %u\n ", returnValue );

        } else if (strcmp(argv[2], "holduppf") == 0) {
            uint8_t returnValue = sendRequestHoldupPF();
            printf("sendRequestHoldupPF Value = %u\n ", returnValue );
        } 

        if (argc > 3) {
            if (strcmp(argv[2], "holduppf") == 0) {
                if (strcmp(argv[3], "start") == 0) {
                    keepRunning = 1;
                    startHoldupPFLogging();
                } else if (strcmp(argv[3], "stop") == 0) {
                    printf(" Stopping Holdup PFLogging.\n");
                    keepRunning = 0;
                }
            }
        }
        return 0;
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
        return 0;
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
        } else if (strcmp(argv[2], "settime") == 0) {
            int time = atoi(argv[3]);
            SetWatchDogTimeout(time);
        } 
        return 0;
    }

    // Handling 'optic' commands
    else if (strcmp(argv[1], "optic") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s optic <test/set/copper>\n", argv[0]);
            return 1;
        }

        if (strcmp(argv[2], "test") == 0) {
            getOpticTestRegister();
        } 
        else if (strcmp(argv[2], "set") == 0) {  
            setOpticPort();
        }
        else if (strcmp(argv[2], "copper") == 0) {  
            setDefaultPort();
        }
        else if (strcmp(argv[2], "check") == 0) {  
            uint32_t discreteinValue = GetDiscreteState();

            if( (discreteinValue & 0xF000) == 0x1000 ) {
                setOpticPort();
                printf("Ethernet Optic Discrete Check : Setting Optic Ethernet.\n");
            } else if ( (discreteinValue & 0xF000) == 0x7000 || (discreteinValue & 0xF000) == 0x3000 ) {
                setDefaultPort();
                printf("Ethernet Optic Discrete Check : Setting Copper Ethernet.\n");
            } else {
            printf("Ethernet Optic Discrete Check : No matching condition met.\n");
        }

        } else {
            fprintf(stderr, "Invalid optic command. Usage: %s optic <test/set/copper>\n", argv[0]);
            return 1;
        }

        
    }

        // Handling 'stp' commands
    else if (strcmp(argv[1], "stp") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: %s stp <1/0>\n", argv[0]);
            return 1;
        }

        if (strcmp(argv[2], "1") == 0) {
            setEthernetStp(1);
        } 

        if (strcmp(argv[2], "0") == 0) {
            setEthernetStp(0);
        } 
        return 0;
    }

        // Handling 'erase' commands
    else if (strcmp(argv[1], "erase") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: %s erase <ssd/nvram>\n", argv[0]);
            return 1;
        }

        if (strcmp(argv[2], "ssd") == 0) {
            initializeDataSSD();
        } 

        if (strcmp(argv[2], "nvram") == 0) {
            InitializeNVRAMToZero();
        } 

        return 0;
    }

        // Handling 'boot mode' commands
    else if (strcmp(argv[1], "bootmode") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: %s bootmode check\n", argv[0]);
            return 1;
        }

        if (strcmp(argv[2], "check") == 0) {
            uint32_t discreteInValue = GetDiscreteState();

            if ((discreteInValue & 0x0020) == 0x0020 && (discreteInValue & 0x0040) == 0) {
                char command[256];
                snprintf(command, sizeof(command), "ethernet-test broadcast start &");
                int ret = system(command);
                if (ret == -1) {
                    perror("system command failed");
                }

                WriteBootModeStatus(BOOT_SIL_MODE);

                printf("\n Detect Discrete-In 'Input_05 Value: True', BootMode: Test\n");
                printf("Start ethernet-test app!\n");
            } else {
                printf("Detect Discrete-In 'Input_05 Value: False', BootMode: Normal\n");
            }
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
            } else if (strcmp(option, "data") == 0){
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

            uint32_t result = ReadBitResult(type);


            printf("BIT Result Value [%s]: 0x%X\n", type_str, result);

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
