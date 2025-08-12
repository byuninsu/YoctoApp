#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <termios.h>
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
#define PHY_SETTING_FILE "/mnt/dataSSD/phy_setting"
#define LOG_FILE_PATH "/mnt/dataSSD/ProgramPinErrorLog"

pthread_t led_init_thread;
volatile int led_init_running = 0;

int keepRunning = 1; // 프로그램 실행 플래그

void InitializeAllLEDs() {
    for (uint8_t gpio = 0x00; gpio <= 0x0B; gpio++) {
        if (setLedState(gpio, 0) != STATUS_SUCCESS) {
            printf("Failed to initialize GPIO 0x%02X\n", gpio);
        }
    }
    printf("All GPIOs initialized to 0.\n");
}

void logProgramPinError() {
    FILE *logFile;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[20];
    
    // 시간 형식 YYMMDDHHMMSS
    strftime(timestamp, sizeof(timestamp), "%y%m%d%H%M%S", t);

    // 로그 저장
    logFile = fopen(LOG_FILE_PATH, "a");
    if (logFile) {
        fprintf(logFile, "%sPROGRAM_PIN_COMPAT_FAULT\n", timestamp);
        fflush(logFile);  // 캐시 플러시
        fsync(fileno(logFile)); // 디스크 동기화
        fclose(logFile);
    } else {
        perror("Failed to open log file");
    }

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
                    printf("SENSE write input Value: 0x%02X\n", value);
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
                    setDiscreteConf(port, value);
                }
            } else if (strcmp(argv[3], "get") == 0) {
                if (strcmp(argv[4], "conf") == 0) {
                    uint8_t port = (uint8_t)strtol(argv[5], NULL, 16);
                    uint8_t value;
                    getDiscreteConf(port, &value);
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

            //sendJetsonBootComplete();
        } 
    } 

    // Handling 'ethernet' commands
    else if (strcmp(argv[1], "ethernet") == 0) {
        if (strcmp(argv[2], "init") == 0) {
            uint16_t readValue; 
        
            readValue = GetDiscreteState();

            int isHigh12 = readValue & (1 << 3);
            int isHigh13 = readValue & (1 << 2);
            int isHigh14 = readValue & (1 << 1);
            int isHigh15 = readValue & (1 << 0);

            if (isHigh12 && (isHigh13 == 0 && isHigh14 == 0 && isHigh15 ==0)) {
                printf("Ethernet init : Optic Set.\n");
                // Optic 전환
                setOpticPort();
            }  else if ((isHigh12 == 0 && isHigh13 && isHigh14 && isHigh15) ||  // 0111
                (isHigh12 == 0 && isHigh13 && isHigh14 && isHigh15 == 0)) { // 0011
                printf("Ethernet init : Copper Set.\n");
                
            } else {
                printf("Ethernet init : Program Pin Compatibility Fault. Logging error...\n");
                logProgramPinError();
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
        } else if (strcmp(argv[2], "setip") == 0) {
            struct in_addr ip_addr;
            char command[128] = {0};
            int ethKind = atoi(argv[3]); // 1 : usb to ethernet, 2 : 6390x
            char* ipNum = argv[4];

            char* ethernetInterface = checkEthernetInterface();
            char* iface;

            if (inet_aton(ipNum, &ip_addr) == 0) {
                fprintf(stderr, "Invalid IP address format: %s\n", ipNum);
                return;
            }

            uint32_t ip = ntohl(ip_addr.s_addr);
            ip &= 0xFFFFFF00;  // Apply /24 netmask (255.255.255.0)
            ip_addr.s_addr = htonl(ip);

            char subnetStr[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &ip_addr, subnetStr, sizeof(subnetStr));

            if (ethKind == 2) {
                iface = ethernetInterface;
            } else if (ethKind == 1) {
                if (strcmp(ethernetInterface, "eth0") == 0) {
                    iface = "eth1";
                } else {
                    iface = "eth0";
                }
            } else {
                fprintf(stderr, "Invalid ethKind value: %d (expected 1 or 2)\n", ethKind);
                return;
            }

            if (ethKind == 1) {
                snprintf(command, sizeof(command), "/usr/sbin/ethtool -s %s autoneg on", iface);
                int status = system(command);
                if (status == -1) {
                    perror("Failed to execute ethtool command");
                    return;
                }
            }

            // Set IP address
            snprintf(command, sizeof(command), "ifconfig %s %s", iface, ipNum);
            if (system(command) != 0) {
                fprintf(stderr, "Failed to set IP address for %s.\n", iface);
                return;
            }

            // Add dynamic route based on subnet
            snprintf(command, sizeof(command),
                    "route add -net %s netmask 255.255.255.0 dev %s",
                    subnetStr, iface);
            if (system(command) != 0) {
                fprintf(stderr, "Failed to add route for %s/24.\n", subnetStr);
                return;
            }

            printf("Setting IP %s on %s...\n", ipNum, iface);
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
            } else if (strcmp(argv[3], "green") == 0) {

                //init LED 종료
                FILE* cmdFile = fopen("/tmp/led_cmd", "w");
                if (cmdFile) {
                    fprintf(cmdFile, "stop\n");
                    fclose(cmdFile);
                } else {
                    printf("Failed to open /tmp/led_cmd to stop led-init\n");
                }
                
                sendJetsonBootComplete();
            }  else if (strcmp(argv[3], "conf") == 0) {
                uint8_t port = (uint8_t)strtol(argv[4], NULL, 16);
                uint8_t value = (uint8_t)strtol(argv[5], NULL, 16);
                printf("main setGpioConf inpu value : %d\n", value);
                setGpioConf(port, value);
            } else if (strcmp(argv[3], "status") == 0) {
                if(strcmp(argv[4], "green") == 0) {
                    sendJetsonBootComplete();
                } else if ( strcmp(argv[4], "red") == 0 ) {

                } else if ( strcmp(argv[4], "yellow") == 0 ) {

                } else if (strcmp(argv[4], "sequence") == 0) {
                    printf("Starting LED sequence: Yellow , Green , Red\n");

                    // 1~4번 LED Yellow
                    InitializeAllLEDs();
                    printf("LED 1~4 set to Yellow.\n");
                    setLedState(0x00, 1);
                    setLedState(0x03, 1);
                    setLedState(0x06, 1);
                    setLedState(0x09, 1);
                    usleep(5000000);

                    // 1~4번 LED Green
                    InitializeAllLEDs();
                    printf("LED 1~4 set to Green.\n");
                    setLedState(0x01, 1);
                    setLedState(0x04, 1);
                    setLedState(0x07, 1);
                    setLedState(0x0A, 1);
                    usleep(5000000);

                    // 1~4번 LED Red
                    InitializeAllLEDs();
                    printf("LED 1~4 set to Red.\n");
                    setLedState(0x02, 1);
                    setLedState(0x05, 1);
                    setLedState(0x08, 1);
                    setLedState(0x0B, 1);
                    usleep(5000000);

                    // 최종적으로 모든 LED 초기화 (꺼짐 상태)
                    InitializeAllLEDs();
                    
                    printf("LED sequence completed.\n");
                }
            }
        } else if (strcmp(argv[2], "get") == 0) {
            if (strcmp(argv[3], "conf") == 0) {
                uint8_t port = (uint8_t)strtol(argv[4], NULL, 16);
                uint8_t conf = getConfState(port);
                 printf("getConfState(port=0x%02X) 0x%02X\n", port, conf);
            } else if (strcmp(argv[3], "gpio") == 0) {
                uint8_t port = (uint8_t)strtol(argv[4], NULL, 16);
                uint8_t gpio = getGpioState(port);
                printf("getGpioState(port=0x%02X) 0x%02X\n", port, gpio);
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
            ReWriteCumulativeTime(time);
        } else if (strcmp(argv[3], "test") == 0) {
            uint32_t addr = (uint32_t)strtoul(argv[4], NULL, 0); 
            uint32_t value = (uint32_t)strtoul(argv[5], NULL, 0); 
            WriteBitResult(addr,value);
        } else if (strcmp(argv[3], "version") == 0) {
            const char *content = argv[4];  
            WriteSWversion(content);
        } else if (strcmp(argv[3], "ssd") == 0) {
            const struct hwCompatInfo myInfo = {
                .supplier_part_no = "OESG-51G09000",
                .supplier_serial_no = "2504H0005G",
                .ssd0_model_no = "TS320GMTE560I", 
                .ssd0_serial_no = "I490480002",
                .ssd1_model_no = "EXPI4M7680GB",
                .ssd1_serial_no = "X08TZB3R130456",
                .sw_part_number = "HSC-OESG-IRIS"
            };
            WriteHwCompatInfoToNVRAM(&myInfo);

            const char *verStr = "1.0";
            WriteSWversion(verStr);

        } else if (strcmp(argv[3], "serial") == 0) {
            int fieldIndex = atoi(argv[4]);    
            const char *content = argv[5];     
            if (fieldIndex >= 1 && fieldIndex <= 7) {
                WriteSerialInfoToNVRAM(fieldIndex, content);
            } else {
                printf("Invalid field index. Use 1~7.\n");
            }
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
        } else if (strcmp(argv[3], "ssd") == 0) {
            struct hwCompatInfo info;
            memset(&info, 0, sizeof(info));

            if (ReadHwCompatInfoFromNVRAM(&info) == 0) {
                printf("SSD Compatibility Info:\n");
                printf(" Supplier Part No  : %s\n", info.supplier_part_no);
                printf(" Supplier Serial No: %s\n", info.supplier_serial_no);
                printf(" SSD0 Model No     : %s\n", info.ssd0_model_no);
                printf(" SSD0 Serial No    : %s\n", info.ssd0_serial_no);
                printf(" SSD1 Model No     : %s\n", info.ssd1_model_no);
                printf(" SSD1 Serial No    : %s\n", info.ssd1_serial_no);
                printf(" SW Part Number    : %s\n", info.sw_part_number);
            } else {
                fprintf(stderr, "Error reading SSD compatibility info from NVRAM.\n");
            }
        } else if (strcmp(argv[3], "check") == 0) {
            uint8_t result = CheckHwCompatInfo();
            printf("Reading CheckHwCompatInfo Value: %d\n", result); 
        } else if (strcmp(argv[3], "id") == 0) {
            uint32_t id  = getNVRAMId();
            printf("Read NVRAM ID Value: 0x%08X\n", id);
        } else if (strcmp(argv[3], "version") == 0) {
            char buf[6] ={0};
            ReadSWversion(buf);
            printf("SW Version: %s\n", buf);
        } else {
            fprintf(stderr, "Invalid read type: %s\n", argv[3]);
        }
    } else {
        fprintf(stderr, "Invalid action: %s\n", argv[2]);
    }
}

    // Handling 'pushbutton get' command
    else if (strcmp(argv[1], "pushbutton") == 0) {
        if (strcmp(argv[2], "get") == 0) {
            uint8_t state = GetButtonState();
            if (state == 1) {
                printf("Push Button State: ON\n");
            } else if (state == 0) {
                printf("Push Button State: OFF\n");
            } else {
                printf("Failed to read Push Button state.\n");
            }
        } else if (strcmp(argv[2], "holdup") == 0) {
            uint8_t state = GetHoldupState();
            if (state == 1) {
                printf("Holdup State: ON\n");
            } else if (state == 0) {
                printf("Holdup State: OFF\n");
            } else {
                printf("Failed to read Holdup state.\n");
            }
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
        } else if (strcmp(argv[2], "available") == 0) {
            int state = isRS232Available();
            if (state == 0) {
                printf("RS232 State: ON\n");
            } else if (state == 1) {
                printf("RS232 State: OFF\n");
            } else {
                printf("Failed to read RS232 state.\n");
            }
        }
    }

    // Handling 'holdup interrupt' start commands
    // else if (strcmp(argv[1], "holdup") == 0) {
    //     if (strcmp(argv[2], "start") == 0) {
    //         printf("Starting holdup process: hold_up_callback\n");
    //         system("hold_up_callback &");  // 백그라운드 실행
    //     } else if (strcmp(argv[2], "stop") == 0) {
    //         printf("Stopping holdup process...\n");
    //         system("pkill -f hold_up_callback");  // 실행 중인 프로세스 종료
    //     } else {
    //         printf("Invalid command: %s\n", argv[2]);
    //         return 1;
    //     }
    // }

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
            } if (strcmp(argv[2], "cinit") == 0) {
            // USB-C FUSB 관련 초기화
            const char *cmd = "i2cset -y 1 0x25 0x02 0x02";
            int ret = system(cmd);

            if (ret == -1) {
                perror("system call failed");
                return 1;
            } else if (WEXITSTATUS(ret) != 0) {
                fprintf(stderr, "i2cset command failed with exit code %d\n", WEXITSTATUS(ret));
                return 1;
            }

            printf("USB-C port initialized via FUSB302 (i2c addr 0x25)\n");
            return 0;
            } else if (strcmp(argv[2], "available") == 0) {

            int state = checkUsb();
            if (state == 0) {
                printf("USB State: ON\n");
            } else if (state == 1) {
                printf("USB State: OFF\n");
            } else {
                printf("Failed to read USB state.\n");
            }
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
            sendStartWatchdog();
        } else if (strcmp(argv[2], "stop") == 0) {
            sendStopWatchdog();
        } else if (strcmp(argv[2], "settime") == 0) {
            int time = atoi(argv[3]);
            sendCommandTimeout(time);
        } else if (strcmp(argv[2], "remain") == 0) {
            uint8_t result = sendWatchdogRemainTime();
            fprintf(stderr, "Watchdog remain Time: %s \n", result);
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

            if( (discreteinValue & 0x000F) == 0x0008 ) {
                setOpticPort();
                printf("Ethernet Optic Discrete Check : Setting Optic Ethernet.\n");
            } else if ( (discreteinValue & 0x000F) == 0x0007 || (discreteinValue & 0x000F) == 0x0003 ) {
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

        if (strcmp(argv[2], "on") == 0) {
            setVlanStp();
        }

        if (strcmp(argv[2], "disable") == 0) {
            setPortDisableWithout2();
        }

        if (strcmp(argv[2], "enable") == 0) {
            setPortEnableWithout2();
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

        if (strcmp(argv[2], "check") == 0) {
            uint32_t discreteInValue = GetDiscreteState();

            if ((discreteInValue & 0x0020) == 0x0020 && (discreteInValue & 0x0040) == 0) {
                char command[256];

                //setVlanStp();
                //setPortDisableWithout2();

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
        } else if ( strcmp(argv[2], "phy") == 0) {

            char* iface = checkEthernetInterface();

            if ( strcmp(argv[3], "start") == 0) {
                // 파일 값 읽기
                FILE *file = fopen(PHY_SETTING_FILE, "r");
                int setting = 0; // 기본값 0
                if (file != NULL) {
                    fscanf(file, "%d", &setting);
                    fclose(file);
                }

                if (setting == 1) {
                    printf("PHY setting is ON, bringing interface down.\n");
                    char command[256];
                    snprintf(command, sizeof(command), "ip link set %s down", iface);
                    system(command);
                } else {
                    printf("PHY setting is OFF, doing nothing.\n");
                }

            } else if ( strcmp(argv[3], "on") == 0 ) {
                // 파일에 "1" 저장
                FILE *file = fopen(PHY_SETTING_FILE, "w");
                if (file == NULL) {
                    perror("Failed to open setting file");
                    return 1;
                }
                fprintf(file, "1");
                fclose(file);
                printf("PHY setting enabled.\n");

            } else if ( strcmp(argv[3], "off") == 0 ) {
                // 파일에 "0" 저장
                FILE *file = fopen(PHY_SETTING_FILE, "w");
                if (file == NULL) {
                    perror("Failed to open setting file");
                    return 1;
                }
                fprintf(file, "0");
                fclose(file);
                printf("PHY setting disabled.\n");

            }
        } else if ( strcmp(argv[2], "complete") == 0) {
            char rs232Result[128] = {0};

            uint32_t result = readtBitResult(2);

            if (result & 0x7FFFFFFF) {
                //printf("\n[ BSP & API initialization Fail ]\n");
                snprintf(rs232Result, sizeof(rs232Result), "\n[ BSP & API initialization Fail ]\n ");
            } else {
                //printf("\n[ BSP & API initialization Success ]\n");
                snprintf(rs232Result, sizeof(rs232Result), "\n[ BSP & API initialization Success ]\n ");
            }

            sendRS232MessageBeforeInit(rs232Result); //Rs232 전송
        } else if (strcmp(argv[2], "startSh") == 0) {
            if (argc < 4) {
                fprintf(stderr, "Missing script path\n");
                return 1;
            }

            const char* scriptPath = argv[3];

            char printRs232[128] = {0};
            snprintf(printRs232, sizeof(printRs232), "\n[ START run-all Script: %s ]\n", scriptPath);
            sendRS232MessageBeforeInit(printRs232);

            char command[256] = {0};
            snprintf(command, sizeof(command), "sh %s", scriptPath);
            system(command);
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

            if( type == 2){
                RequestBit(type);
            } else if (type == 3) {
                RequestCBIT(type);
            } else if (type == 4) {
                RequestBit(type);
            }
            
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
