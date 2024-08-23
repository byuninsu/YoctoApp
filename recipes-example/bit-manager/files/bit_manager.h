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



#endif // BIT_MANAGER_H

