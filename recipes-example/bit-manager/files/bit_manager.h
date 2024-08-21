#ifndef BIT_MANAGER_H
#define BIT_MANAGER_H

#define GPU_API

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

// 함수 선언
int check_ssd(const char *ssd_path);
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
uint32_t GPU_API RequestBit(uint32_t mtype);
uint32_t GPU_API readtBitResult(void);
uint32_t GPU_API WriteBitErrorData(uint32_t bitStatus, uint32_t mtype);
cJSON* GPU_API ReadBitErrorLog(void);


#endif // BIT_MANAGER_H

