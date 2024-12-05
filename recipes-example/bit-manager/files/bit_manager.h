#ifndef BIT_MANAGER_H
#define BIT_MANAGER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <arpa/inet.h>
#include <stddef.h>  
#include <cjson/cJSON.h> 


typedef struct {
    int critical_warning;
    int temperature;
    int available_spare;
    int available_spare_threshold;
    int percentage_used;
    unsigned long long data_units_read;
    unsigned long long data_units_written;
    unsigned long long host_read_commands;
    unsigned long long host_write_commands;
    int power_cycles;
    int power_on_hours;
    int unsafe_shutdowns;
    int media_errors;
    int num_err_log_entries;
    int temperature_sensor_1;
    int temperature_sensor_2;
    int temperature_sensor_3;
} SSDSmartLog;

// 함수 선언
size_t GetItemCount();
const char* GetItemName(uint32_t mItem);
int check_ssd(const char *ssd_path);
int  WriteBitErrorData(uint32_t bitStatus, uint32_t mtype);
cJSON*  ReadBitErrorLog(void);
int check_gpio_expander();
int check_discrete_out();
int checkDiscrete_in();
int checkEthernet();
int checkGPU();
int checkNvram();
int checkRs232();
int checkEthernetSwitch();
int checkTempSensor();
int checkPowerMonitor();
int checkUsb(void);
int checkOptic();
void  RequestBit(uint32_t mtype);
uint32_t readtBitResult(uint32_t type);
SSDSmartLog getSSDSmartLog(uint8_t ssd_type);
uint8_t initializeDataSSD(void);



#endif // BIT_MANAGER_H

