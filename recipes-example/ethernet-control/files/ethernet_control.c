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
#include "ethernet_control.h"
#include "mii.h"  

#define MAX_ETH 8 /* Maximum # of interfaces */

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

            if (edata.data) {
                printf("Link detected on %s\n", iface);
                close(sock);
                return iface;
            } else {
                printf("No link detected on %s\n", iface);
            }
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


