#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ethernet_control.h"

static char iface[20] = {0};

char * checkEthernetInterface() {
    int sock;
    struct ifreq ifr;

    char mac_addr[18];
    struct ethtool_value edata;

    // 네트워크 인터페이스를 위한 소켓 생성
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        return NULL;
    }

    // eth0과 eth1 인터페이스를 순차적으로 확인
    for (int i = 0; i < 2; i++) {
        snprintf(iface, sizeof(iface), "eth%d", i);
        strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

        // MAC 주소 가져오기
        if (ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
            perror("ioctl SIOCGIFHWADDR");
            close(sock);
            return NULL;
        }

        // MAC 주소를 문자열로 변환
        snprintf(mac_addr, sizeof(mac_addr), "%02x:%02x:%02x:%02x:%02x:%02x",
                 (unsigned char)ifr.ifr_hwaddr.sa_data[0],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[1],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[2],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[3],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[4],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[5]);

        if ((unsigned char)ifr.ifr_hwaddr.sa_data[0] == 0x48) {
            printf("Interface %s has a MAC address starting with 48: %s\n", iface, mac_addr);

            // ethtool을 사용하여 링크 상태 확인
            edata.cmd = ETHTOOL_GLINK;
            ifr.ifr_data = (caddr_t)&edata;

            if (ioctl(sock, SIOCETHTOOL, &ifr) == -1) {
                perror("ioctl SIOCETHTOOL");
                close(sock);
                return NULL;
            }

            // edata.data가 1이면 링크가 감지됨
            if (edata.data) {
                printf("Link detected on %s\n", iface);
                close(sock);
                return iface; // 링크 감지됨, 정상 작동
            } else {
                printf("No link detected on %s\n", iface);
            }
        }
    }

    close(sock);

    return NULL; 
}



uint8_t GPU_API setEthernetPort(int port, int state) {

    if (iface[0] == '\0') {
        checkEthernetInterface();
    }


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

uint32_t GPU_API getEthernetPort(int port) {

    if (iface[0] == '\0') {
        checkEthernetInterface();
    }
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


