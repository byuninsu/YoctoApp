#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "libethernet_control.h"
#include "mii.h"  

#define MAX_ETH 8 /* Maximum # of interfaces */
#define MAX_PORT 10  // 포트 개수

static char iface[20] = {0};
static int skfd = -1; /* AF_INET socket for ioctl() calls. */
static struct ifreq ifr;

static int ethernetInit(const char* interface_name) {
    /* Open a basic socket. */
    if (skfd < 0) {
        if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("socket");
            return -1;
        }
    }

    /* Get the vitals from the interface. */
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ);
    if (ioctl(skfd, SIOCGMIIPHY, &ifr) < 0) {
        if (errno != ENODEV)
            fprintf(stderr, "SIOCGMIIPHY on '%s' failed: %s\n", interface_name, strerror(errno));
        return -1;
    }

    return 0;
}

static int mdio_read(const char* interface_name, int phy_addr, int page, int location) {
    if (ethernetInit(interface_name) < 0) {
        return -1;
    }

    struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;

    if (page >= 0) {
        mii->phy_id = phy_addr;
        mii->reg_num = 22;
        mii->val_in = page;
        if (ioctl(skfd, SIOCSMIIREG, &ifr) < 0) {
            fprintf(stderr, "SIOCSMIIREG (page set) on %s failed: %s\n", ifr.ifr_name, strerror(errno));
            return -1;
        }
    }

    mii->phy_id = phy_addr;
    mii->reg_num = location;
    if (ioctl(skfd, SIOCGMIIREG, &ifr) < 0) {
        fprintf(stderr, "SIOCGMIIREG on %s failed: %s\n", ifr.ifr_name, strerror(errno));
        return -1;
    }
    return mii->val_out;
}

static void mdio_write(const char* interface_name, int phy_addr, int page, int location, int value) {
    if (ethernetInit(interface_name) < 0) {
        return;
    }

    struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;

    if (page >= 0) {
        mii->phy_id = phy_addr;
        mii->reg_num = 22;
        mii->val_in = page;
        if (ioctl(skfd, SIOCSMIIREG, &ifr) < 0) {
            fprintf(stderr, "SIOCSMIIREG (page set) on %s failed: %s\n", ifr.ifr_name, strerror(errno));
        }
    }

    mii->phy_id = phy_addr;
    mii->reg_num = location;
    mii->val_in = value;
    if (ioctl(skfd, SIOCSMIIREG, &ifr) < 0) {
        fprintf(stderr, "SIOCSMIIREG on %s failed: %s\n", ifr.ifr_name, strerror(errno));
    }
}

char* checkEthernetInterface() {
    int sock;
    struct ifreq ifr;

    char mac_addr[18];
    struct ethtool_value edata;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        return NULL;
    }

    for (int i = 0; i < 2; i++) {
        snprintf(iface, sizeof(iface), "eth%d", i);
        strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

        if (ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
            perror("ioctl SIOCGIFHWADDR");
            close(sock);
            return NULL;
        }

        snprintf(mac_addr, sizeof(mac_addr), "%02x:%02x:%02x:%02x:%02x:%02x",
                 (unsigned char)ifr.ifr_hwaddr.sa_data[0],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[1],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[2],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[3],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[4],
                 (unsigned char)ifr.ifr_hwaddr.sa_data[5]);

        if ((unsigned char)ifr.ifr_hwaddr.sa_data[0] == 0x48) {
            printf("Interface %s has a MAC address starting with 48: %s\n", iface, mac_addr);

            edata.cmd = ETHTOOL_GLINK;
            ifr.ifr_data = (caddr_t)&edata;

            if (ioctl(sock, SIOCETHTOOL, &ifr) == -1) {
                perror("ioctl SIOCETHTOOL");
                close(sock);
                return NULL;
            }


            return iface;

            // if (edata.data) {
            //     printf("Link detected on %s\n", iface);
            //     close(sock);
            //     return iface;
            // } else {
            //     printf("No link detected on %s\n", iface);
            // }
        }
    }

    close(sock);

    return NULL; 
}


uint8_t setEthernetPort(int port, int state) {
    if (iface[0] == '\0') {
        checkEthernetInterface();
    }

    if (skfd < 0) {
        if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("socket");
            return 1;
        }
    }

    int phy_addr = port;
    int reg_addr = 4;
    int page = -1;

    if (state == 1) {
        mdio_write(iface, phy_addr, page, reg_addr, 0x007f);
    } else {
        mdio_write(iface, phy_addr, page, reg_addr, 0x0078);
    }

    close(skfd);
    skfd = -1;
    return 0;
}


uint32_t getEthernetPort(int port) {
    if (iface[0] == '\0') {
        checkEthernetInterface();
    }

    if (skfd < 0) {
        if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("socket");
            return 0;
        }
    }

    int phy_addr = port;
    int reg_addr = 4;
    int page = -1;

    int result = mdio_read(iface, phy_addr, page, reg_addr);
    if (result >= 0) {
        printf("Port %d status: 0x%.4x\n", port, result);
    }

    close(skfd);
    skfd = -1;
    return (uint32_t)result;
}

// IP 주소 가져오기 함수
int get_ip_address(const char *iface, char *ip_addr) {
    int fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("Socket failed");
        return -1;
    }

    strncpy(ifr.ifr_name, iface, IFNAMSIZ);
    if (ioctl(fd, SIOCGIFADDR, &ifr) == -1) {
        perror("ioctl SIOCGIFADDR failed");
        close(fd);
        return -1;
    }

    struct sockaddr_in *ip = (struct sockaddr_in *)&ifr.ifr_addr;
    strcpy(ip_addr, inet_ntoa(ip->sin_addr));

    close(fd);
    return 0;
}

void setEthernetStp(int value) {
    char command[256];
    char ip_addr[INET_ADDRSTRLEN];

    // MAC 주소가 48:로 시작하는 인터페이스가 있는지 확인
    if (iface[0] == '\0') {
        if (checkEthernetInterface() == NULL) {
            printf("이더넷 인터페이스를 찾을 수 없습니다.\n");
            return;
        }
    }

    // 인터페이스에서 IP 주소를 가져오기
    if (get_ip_address(iface, ip_addr) == 0) {
        printf("인터페이스 %s의 IP: %s\n", iface, ip_addr);
    } else {
        printf("IP 주소를 가져오는 데 실패했습니다.\n");
        return;
    }

    // IP 이동 및 STP 활성화/비활성화
    if (value == 1) {
        system("ip link add name br0 type bridge"); // br0 브리지 생성
        snprintf(command, sizeof(command), "ip link set dev %s master br0", iface);
        system(command); // eth0/eth1을 브리지에 추가

        // eth0/eth1에서 IP 제거
        snprintf(command, sizeof(command), "ip addr flush dev %s", iface);
        system(command);

        // 인터페이스가 브리지에 정상적으로 추가되었는지 확인
        system("ip link show br0");

        // br0에 IP 할당
        snprintf(command, sizeof(command), "ip addr add %s/24 dev br0", ip_addr);
        system(command);

        // 브리지 및 STP 활성화 (brctl 명령 사용)
        system("ip link set br0 up");
        system("brctl stp br0 on");
        printf("STP 활성화 완료.\n");
    } else {
        // STP 비활성화 및 브리지 삭제
        system("brctl stp br0 off");  // STP 비활성화
        system("ip link set br0 down"); // 브리지 비활성화
        system("brctl delbr br0"); // 브리지 삭제
        printf("STP 비활성화 및 브리지 삭제 완료.\n");
    }
}

#define MAX_PORT 10  // 포트 개수

uint8_t setVlanStp(void) {

    char command[256];

    // MAC 주소가 48:로 시작하는 인터페이스가 있는지 확인
    if (iface[0] == '\0') {
        if (checkEthernetInterface() == NULL) {
            printf("not found ethernet interface contain 48: \n");
            return 1;
        }
    }

    // 포트 1 ~ 10(A)까지 VLAN 설정
    for (int port = 2; port <= MAX_PORT; port++) {
        snprintf(command, sizeof(command),
                 "%s %s 0x%02x 0x06 0x0002",
                 "mdio-tool w", iface, port);

        printf("Executing: %s\n", command);
        system(command);
    }

    return 0;
}



