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
    uint8_t temperature;                 // Temperature
    uint8_t available_spare;             // Available Spare
    uint8_t available_spare_threshold;   // Available Spare Threshold
    uint8_t percentage_used;             // Percentage Used
    uint32_t data_units_read;            // Data Units Read
    uint32_t data_units_written;         // Data Units Written
    uint32_t host_read_commands;         // Host Read Commands
    uint32_t host_write_commands;        // Host Write Commands
    uint32_t controller_busy_time;       // Controller Busy Time
    uint32_t power_cycles;               // Power Cycles
    uint32_t power_on_hours;             // Power On Hours
    uint32_t unsafe_shutdowns;           // Unsafe Shutdowns
    uint32_t media_and_data_errors;      // Media and Data Errors
    uint32_t error_log_entries;          // Error Log Entries
    uint32_t warning_temp_time;          // Warning Temperature Time
    uint32_t critical_temp_time;         // Critical Temperature Time
} SSD_Status, *pSSD_Status;


// 함수 선언
int read_nvme_info(const char* nvme_dev, char* model_no, size_t model_size, char* serial_no, size_t serial_size);
size_t GetItemCount();
const char* GetItemName(uint32_t mItem);
int check_ssd(const char *ssd_path);
uint32_t GetLastSequence(const char *filePath);
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
uint8_t CheckHwCompatInfo(void);
int checkHoldupModule(void);
int checkStm32Status(void);
int checkPowerStatus(void);
int checkLan7800(void);
int checkUSBc(void);
int checkUSBa(void);
void RequestBit(uint32_t mtype);
void RequestCBIT(uint32_t mtype);
uint32_t readtBitResult(uint32_t type);
SSD_Status getSSDSmartLog(uint8_t ssd_type);
uint8_t initializeDataSSD(void);
void sendRS232Message(const char* message);



#endif // BIT_MANAGER_H

