#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/sockios.h>
#include "mii.h"

#define MAX_ETH 8 /* Maximum # of interfaces */

static int skfd = -1; /* AF_INET socket for ioctl() calls. */
static struct ifreq ifr;

static int mdio_read(int skfd, int phy_addr, int page, int location) {
    struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;

    // 페이지 설정
    if (page >= 0) {
        mii->phy_id = phy_addr;
        mii->reg_num = 22;
        mii->val_in = page;
        if (ioctl(skfd, SIOCSMIIREG, &ifr) < 0) {
            fprintf(stderr, "SIOCSMIIREG (page set) on %s failed: %s\n", ifr.ifr_name, strerror(errno));
            return -1;
        }
    }

    // 레지스터 읽기
    mii->phy_id = phy_addr;
    mii->reg_num = location;
    if (ioctl(skfd, SIOCGMIIREG, &ifr) < 0) {
        fprintf(stderr, "SIOCGMIIREG on %s failed: %s\n", ifr.ifr_name, strerror(errno));
        return -1;
    }
    return mii->val_out;
}


static void mdio_write(int skfd, int phy_addr, int page, int location, int value) {
    struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;

    // 페이지 설정
    if (page >= 0) {
        mii->phy_id = phy_addr;
        mii->reg_num = 22;
        mii->val_in = page;
        if (ioctl(skfd, SIOCSMIIREG, &ifr) < 0) {
            fprintf(stderr, "SIOCSMIIREG (page set) on %s failed: %s\n", ifr.ifr_name, strerror(errno));
        }
    }

    // 레지스터 쓰기
    mii->phy_id = phy_addr;
    mii->reg_num = location;
    mii->val_in = value;
    if (ioctl(skfd, SIOCSMIIREG, &ifr) < 0) {
        fprintf(stderr, "SIOCSMIIREG on %s failed: %s\n", ifr.ifr_name, strerror(errno));
    }
}

int main(int argc, char **argv) {
    int addr, val, phy_addr, page = -1;

    if (argc < 5 || (argv[1][0] == 'w' && argc < 6)) {
        printf("Usage: mdio-tool [r/w] [interface] [phy_addr] [reg] [val (for write)] [page (optional)]\n");
        return 1;
    }

    /* Open a basic socket. */
    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    /* Get the vitals from the interface. */
    strncpy(ifr.ifr_name, argv[2], IFNAMSIZ);
    if (ioctl(skfd, SIOCGMIIPHY, &ifr) < 0) {
        if (errno != ENODEV)
            fprintf(stderr, "SIOCGMIIPHY on '%s' failed: %s\n", argv[2], strerror(errno));
        return -1;
    }

    phy_addr = strtol(argv[3], NULL, 16);
    addr = strtol(argv[4], NULL, 16);
    if (argc > 5 && argv[1][0] == 'w') {
        val = strtol(argv[5], NULL, 16);
    }
    if (argc == 7) {
        page = strtol(argv[6], NULL, 16);
    }

    if (argv[1][0] == 'r') {
        int result = mdio_read(skfd, phy_addr, page, addr);
        if (result >= 0) {
            printf("0x%.4x\n", result);
        }
    } else if (argv[1][0] == 'w') {
        mdio_write(skfd, phy_addr, page, addr, val);
    } else {
        printf("Invalid operation!\n");
    }

    close(skfd);
    return 0;
}

