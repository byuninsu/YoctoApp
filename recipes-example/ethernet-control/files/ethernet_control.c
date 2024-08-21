#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>  
#include <string.h>
#include "ethernet_control.h"


uint8_t GPU_API setEthernetPort(char *iface, int port, int state) {
    char command[256];

    // MDIO 레지스터 주소 및 값
    char *reg_addr = "0x04";
    char *enable_value = "0x007f";
    char *disable_value = "0x0078";

    // 포트 상태에 따라 명령어 설정
    if (state == 1) {
        snprintf(command, sizeof(command), "mdio-tool w %s %d %s %s", iface, port, reg_addr, enable_value);
    } else {
        snprintf(command, sizeof(command), "mdio-tool w %s %d %s %s", iface, port, reg_addr, disable_value);
    }

    // 명령어 실행
    printf("Executing command: %s\n", command);
    
    int ret = system(command);
    if (ret == -1) {
        perror("Error executing system command");
        return 1;
    }
    
    return 0;
}

uint32_t GPU_API getEthernetPort(char *iface, int port) {
    char command[256];
    char result[128];
    FILE *fp;
    uint32_t port_status = 0;

    // MDIO 레지스터 주소
    const char *reg_addr = "0x04";

    // 포트 상태에 따라 명령어 설정
    snprintf(command, sizeof(command), "mdio-tool r %s %d %s", iface, port, reg_addr);

    // 명령어 실행 및 결과 읽기
    printf("Executing command: %s\n", command);

    fp = popen(command, "r");
    if (fp == NULL) {
        perror("popen failed");
        return 0; // 실패 시 반환할 기본값. 필요시 수정
    }

    // 명령어 결과 읽기
    if (fgets(result, sizeof(result), fp) != NULL) {
        // 결과 값을 처리하여 반환할 값을 결정 (예시로 값을 uint32_t로 변환)
        port_status = (uint32_t)strtoul(result, NULL, 16); // 결과를 16진수로 변환
    } else {
        printf("No output from command or error reading result.\n");
    }

    // popen에서 반환된 파일 포인터 닫기
    pclose(fp);

    return port_status;
}


