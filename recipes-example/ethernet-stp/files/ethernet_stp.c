#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <signal.h>

volatile int running = 0;
pthread_t recv_thread;

#define BUF_SIZE 1600
#define DSA_TAG_LEN 4

// LLC + SNAP 헤더
struct llc_header {
    uint8_t dsap;
    uint8_t ssap;
    uint8_t control;
    uint8_t org_code[3];
    uint16_t ethertype;
} __attribute__((packed));

// BPDU Header
struct bpdu_header {
    uint8_t protocol_id[2];
    uint8_t protocol_version;
    uint8_t bpdu_type;
    uint8_t flags;
    uint8_t root_id[8];
    uint32_t root_path_cost;
    uint8_t bridge_id[8];
    uint16_t port_id;
    uint16_t message_age;
    uint16_t max_age;
    uint16_t hello_time;
    uint16_t forward_delay;
} __attribute__((packed));

void print_mac(const uint8_t *mac) {
    printf("%02X:%02X:%02X:%02X:%02X:%02X", 
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void* receive_thread(void *arg) {
    char *iface = (char *)arg;
    int sock;
    uint8_t buffer[BUF_SIZE];
    struct ifreq ifr;
    struct sockaddr_ll saddr;

    sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0) {
        perror("socket");
        pthread_exit(NULL);
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("SIOCGIFINDEX");
        close(sock);
        pthread_exit(NULL);
    }

    memset(&saddr, 0, sizeof(saddr));
    saddr.sll_family = AF_PACKET;
    saddr.sll_protocol = htons(ETH_P_ALL);
    saddr.sll_ifindex = ifr.ifr_ifindex;

    if (bind(sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        perror("bind");
        close(sock);
        pthread_exit(NULL);
    }

    printf("Listening for BPDU packets on %s...\n", iface);

    while (running) {
        ssize_t len = recvfrom(sock, buffer, BUF_SIZE, 0, NULL, NULL);
        if (len <= 0) continue;

        struct ethhdr *eth = (struct ethhdr *)buffer;

        // 기본 파싱 오프셋
        int offset = sizeof(struct ethhdr);
        int dsa_present = 0;
        uint8_t src_dev = 0, src_port = 0, cpu_code = 0;

        if (len > offset + 4) {
            uint8_t dsa_tag4 = buffer[offset + 3];

            if ((dsa_tag4 & 0xC0) == 0x80) {  // 상위 2비트가 10 = To_CPU
                dsa_present = 1;
                src_dev = (buffer[offset + 2] & 0xF0) >> 4;
                src_port = (buffer[offset + 2] & 0x0F);
                cpu_code = (buffer[offset + 3] & 0x07);

                printf("[INFO] To_CPU DSA Tag Detected:\n");
                printf("       - Src Dev  : 0x%X\n", src_dev);
                printf("       - Src Port : 0x%X (%d)\n", src_port, src_port);
                printf("       - CPU Code : 0x%X (%s)\n", cpu_code,
                    cpu_code == 0x0 ? "BPDU" :
                    cpu_code == 0x1 ? "Frame2Reg" :
                    cpu_code == 0x2 ? "IGMP/MLD" :
                    cpu_code == 0x3 ? "Policy Trap" :
                    cpu_code == 0x4 ? "ARP Mirror" :
                    cpu_code == 0x5 ? "Policy Mirror" :
                    "Reserved");
                offset += DSA_TAG_LEN;
            }
        }

        if (memcmp(buffer, "\x01\x80\xC2\x00\x00\x00", 6) != 0)
            continue; // Not BPDU

        struct llc_header *llc = (struct llc_header *)(buffer + offset);
        if (llc->dsap != 0x42 || llc->ssap != 0x42 || llc->control != 0x03)
            continue;

        struct bpdu_header *bpdu = (struct bpdu_header *)(buffer + offset + sizeof(struct llc_header));

        printf("---- BPDU Packet Received ----\n");
        printf("BPDU Type      : 0x%02X\n", bpdu->bpdu_type);
        printf("Flags          : 0x%02X\n", bpdu->flags);
        printf("Root Bridge ID : ");
        print_mac(&bpdu->root_id[2]);
        printf("\n");
        printf("Bridge ID      : ");
        print_mac(&bpdu->bridge_id[2]);
        printf("\n");
        printf("Port ID        : 0x%04X\n", ntohs(bpdu->port_id));
        printf("Message Age    : %.2f sec\n", ntohs(bpdu->message_age) / 256.0);
        printf("Hello Time     : %.2f sec\n", ntohs(bpdu->hello_time) / 256.0);
        printf("Forward Delay  : %.2f sec\n", ntohs(bpdu->forward_delay) / 256.0);
        printf("------------------------------\n\n");
    }

    close(sock);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <start|stop> <interface>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "start") == 0) {
        if (running) {
            printf("Already running!\n");
            return 0;
        }

        running = 1;
        if (pthread_create(&recv_thread, NULL, receive_thread, argv[2]) != 0) {
            perror("pthread_create");
            return 1;
        }

        printf("BPDU sniffer started.\n");
        pthread_join(recv_thread, NULL);
    } else if (strcmp(argv[1], "stop") == 0) {
        running = 0;
        printf("BPDU sniffer stopped.\n");
    } else {
        printf("Invalid command. Use start or stop.\n");
        return 1;
    }

    return 0;
}
