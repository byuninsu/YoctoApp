#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <stdbool.h>
#include "ethernet_control.h"

volatile int running = 0;
pthread_t recv_thread;
pthread_t poll_thread;
char* ethernet_iface;
struct timeval start, now;

uint8_t current_root_id[8] = {0xFF};
uint32_t current_root_cost = UINT32_MAX;
int current_root_port = -1;
uint16_t current_port_id = 0xFFFF;

// MAC address to use as source
const uint8_t SOURCE_MAC[6] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 };

#define BPDU_FRAME_LEN 60
#define BUF_SIZE 1600
#define MAX_WAIT_MS 3000
#define POLL_INTERVAL_MS 100

// LLC + SNAP Header
struct llc_header {
    uint8_t dsap;
    uint8_t ssap;
    uint8_t control;
} __attribute__((packed));

// BPDU Header
struct bpdu_header {
    uint8_t protocol_id[2];
    uint8_t version;
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

// DSA 태그 해석
int extract_dsa_src_port(const uint8_t *dsa_tag) {
    return (dsa_tag[1] & 0xF8) >> 3;
}

int is_port_link_up(int port) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
        "mdio-tool r %s 0x%02X 0x00", ethernet_iface, port);

    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;

    char buf[64];
    if (fgets(buf, sizeof(buf), fp)) {
        uint16_t reg_val = (uint16_t)strtol(buf, NULL, 0);
        pclose(fp);
        return ((reg_val >> 11) & 0x01);  // bit 11 = Link
    }

    pclose(fp);
    return 0;
}

void* bpdu_sender_thread(void* arg) {
    char* iface = (char*)arg;
    int sock;
    struct ifreq ifr;
    struct sockaddr_ll addr;
    uint8_t frame[BPDU_FRAME_LEN];

    // Create raw socket
    sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0) {
        perror("socket");
        return NULL;
    }

    // Get interface index
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("SIOCGIFINDEX");
        close(sock);
        return NULL;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = ifr.ifr_ifindex;
    addr.sll_protocol = htons(ETH_P_ALL);

    while (1) {
        memset(frame, 0, sizeof(frame));

        // Ethernet header
        memcpy(frame, "\x01\x80\xc2\x00\x00\x00", 6); // Destination: STP Multicast
        memcpy(frame + 6, SOURCE_MAC, 6);                  // Source MAC
        frame[12] = 0x00; frame[13] = 0x26;                // EtherType for Marvell tag (or use 0x8100 if normal VLAN/STP)

        // LLC + SNAP header (3 bytes)
        frame[14] = 0x42; // DSAP
        frame[15] = 0x42; // SSAP
        frame[16] = 0x03; // Control

        // BPDU Header (17 bytes min, simplified config BPDU)
        frame[17] = 0x00; // Protocol ID high
        frame[18] = 0x00; // Protocol ID low
        frame[19] = 0x00; // Version
        frame[20] = 0x00; // Type: Config BPDU
        frame[21] = 0x00; // Flags

        // Root ID (8 bytes)
        memcpy(&frame[22], "\x80\x00", 2);              // Root priority
        memcpy(&frame[24], SOURCE_MAC, 6);                // Root MAC

        // Root path cost (4 bytes)
        frame[30] = 0x00; frame[31] = 0x00; frame[32] = 0x00; frame[33] = 0x00;

        // Bridge ID (8 bytes)
        memcpy(&frame[34], "\x80\x00", 2);              // Bridge priority
        memcpy(&frame[36], SOURCE_MAC, 6);                // Bridge MAC

        // Port ID (2 bytes)
        frame[42] = 0x80; frame[43] = 0x01;

        // Message Age, Max Age, Hello Time, Forward Delay (each 2 bytes)
        frame[44] = 0x00; frame[45] = 0x00; // Message Age
        frame[46] = 0x00; frame[47] = 0x14; // Max Age = 20
        frame[48] = 0x00; frame[49] = 0x02; // Hello Time = 2
        frame[50] = 0x00; frame[51] = 0x0f; // Forward Delay = 15

        // Padding (Ethernet min size = 60)
        for (int i = 52; i < BPDU_FRAME_LEN; i++) frame[i] = 0x00;

        // Send frame
        if (sendto(sock, frame, BPDU_FRAME_LEN, 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("sendto");
        } else {
            printf("[BPDU] Sent Configuration BPDU\n");
        }

        sleep(2); // send every 2 seconds
    }

    close(sock);
    return NULL;
}


void process_stp_logic(int src_port, struct bpdu_header *bpdu) {

    int better_root = memcmp(bpdu->root_id, current_root_id, 8) < 0;
    int same_root_better_cost = (memcmp(bpdu->root_id, current_root_id, 8) == 0) &&
                                 (ntohl(bpdu->root_path_cost) < current_root_cost);
    int same_root_same_cost = (memcmp(bpdu->root_id, current_root_id, 8) == 0) &&
                                (ntohl(bpdu->root_path_cost) == current_root_cost);
    int better_port_id = (ntohs(bpdu->port_id) < current_port_id);  

    if (better_root || same_root_better_cost ||
        (same_root_same_cost && ntohs(bpdu->port_id) < current_port_id)) {
        
        printf("[STP Logic] find better port ! , current_root_port : %d \n", current_root_port);
    
        memcpy(current_root_id, bpdu->root_id, 8);
        current_root_cost = ntohl(bpdu->root_path_cost);
        current_port_id = ntohs(bpdu->port_id);  
    
        if (current_root_port >= 0 && current_root_port != src_port) {
            setEthernetPort(current_root_port, 0);
            printf("[STP] Blocking previous root port %d\n", current_root_port);
        }
    
        current_root_port = src_port;  
        setEthernetPort(current_root_port, 1);
        printf("[STP] Forwarding root port %d (Port ID: 0x%04X)\n", current_root_port, current_port_id);
    } else {
        // 루트 포트가 아니면 명시적으로 포트를 차단
        if (src_port != current_root_port) {
            setEthernetPort(src_port, 0);
            printf("[STP] Blocking non-root port %d\n", src_port);
        }
    }
}

void print_dsa_tag_info(const uint8_t *dsa_tag) {
    printf("Raw DSA Tag Bytes: %02X %02X %02X %02X\n",
        dsa_tag[0], dsa_tag[1], dsa_tag[2], dsa_tag[3]);

    uint8_t byte0 = dsa_tag[0];
    uint8_t byte1 = dsa_tag[1];
    uint8_t byte2 = dsa_tag[2];
    uint8_t byte3 = dsa_tag[3];

    int dsa_cmd = (byte0 >> 6) & 0x03;
    int src_dev = (byte0 >> 3) & 0x07;
    int src_port = (byte1 >> 3) & 0x1F;
    int rx_sniff = (byte1 >> 2) & 0x01;
    int cfi = (byte1 >> 1) & 0x01;
    int pri = (byte2 >> 5) & 0x07;
    int vid = ((byte2 & 0x1F) << 8) | byte3;

    printf("---- DSA Tag ----\n");
    printf("DSA Command     : 0x%02X (%s)\n", dsa_cmd,
           dsa_cmd == 1 ? "To_CPU" :
           dsa_cmd == 2 ? "From_CPU" :
           dsa_cmd == 3 ? "To_Sniffer" : "Unknown");
    printf("Source Device   : %d\n", src_dev);
    printf("Source Port     : %d\n", src_port);
    printf("Rx_Sniff        : %d\n", rx_sniff);
    printf("CFI             : %d\n", cfi);
    printf("Priority (PRI)  : %d\n", pri);
    printf("VLAN ID (VID)   : %d\n", vid);
    printf("------------------\n");
}

void* link_polling_thread(void *arg) {
    const int ports[] = {2, 3, 4, 5, 6, 7, 8, 9, 10};  // 감시할 스위치 포트
    const int port_count = sizeof(ports)/sizeof(ports[0]);
    const int cpu_port_num = 1;
    int port_states[32] = {0};
    int dsa_mode_enabled = 0;
    uint8_t buffer[BUF_SIZE];

    while (running) {
        int new_link_up_port = -1;
        int new_link_down_port = -1;

        for (int i = 0; i < port_count; i++) {
            int port = ports[i];
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "mdio-tool r %s 0x%02X 0x00", ethernet_iface, port);
            FILE *fp = popen(cmd, "r");
            if (!fp) continue;

            char buf[64];
            if (fgets(buf, sizeof(buf), fp)) {
                uint16_t reg_val = (uint16_t)strtol(buf, NULL, 0);
                int link_up = (reg_val >> 11) & 0x1;

                if (link_up && port_states[port] == 0) {
                    new_link_up_port = port;
                    port_states[port] = 1;
                } else if (!link_up && port_states[port] == 1) {
                    new_link_down_port = port;
                    port_states[port] = 0;
                }
            }

            pclose(fp);
        }

        if (new_link_up_port != -1 && !dsa_mode_enabled) {

            int bpdu_received = 0;
            int elapsed_ms = 0;

            // DSA 모드 진입
            printf("[LINK UP] Port %d detected. Enabling DSA mode...\n", new_link_up_port);
            char dsa_cmd[128];
            snprintf(dsa_cmd, sizeof(dsa_cmd), "mdio-tool w %s 0x%02X 0x04 0x037F", ethernet_iface, cpu_port_num);
            system(dsa_cmd);
            dsa_mode_enabled = 1;

            // BPDU 수신을 위해 RAW 소켓 열기
            int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
            if (sock < 0) {
                perror("socket");
                dsa_mode_enabled = 0;
                continue;
            }

            struct ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, ethernet_iface, IFNAMSIZ - 1);
            if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
                perror("SIOCGIFINDEX");
                close(sock);
                continue;
            }

            struct sockaddr_ll saddr = {0};
            saddr.sll_family = AF_PACKET;
            saddr.sll_protocol = htons(ETH_P_ALL);
            saddr.sll_ifindex = ifr.ifr_ifindex;
            if (bind(sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
                perror("bind");
                close(sock);
                continue;
            }

            // 수신 타임아웃 2초 설정
            struct timeval timeout = {2, 0};
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

            printf("[STP] Listening for BPDUs for 2 seconds...\n");

            gettimeofday(&start, NULL);

            while (elapsed_ms < MAX_WAIT_MS) {
                ssize_t len = recvfrom(sock, buffer, BUF_SIZE, 0, NULL, NULL);

                gettimeofday(&now, NULL);

                elapsed_ms = (now.tv_sec - start.tv_sec) * 1000L +
                    (now.tv_usec - start.tv_usec) / 1000L;

                if (elapsed_ms >= MAX_WAIT_MS) break;

                if (len <= 0) {
                    elapsed_ms += POLL_INTERVAL_MS;  // 타임아웃된 경우에도 증가시켜줘야 함
                    continue;
                }

                // BPDU 목적지 MAC 확인
                if (memcmp(buffer, "\x01\x80\xC2\x00\x00\x00", 6) != 0) continue;

                // DSA, LLC, BPDU offset
                int eth_offset = 12;
                int vlan_offset = eth_offset + 4;
                int dsa_offset = vlan_offset;
                int llc_offset = dsa_offset + 6;
                int bpdu_offset = llc_offset + 3;

                const uint8_t *dsa_tag = buffer + dsa_offset;
                int src_port = extract_dsa_src_port(dsa_tag);

                struct llc_header *llc = (struct llc_header *)(buffer + llc_offset);
                if (llc->dsap != 0x42 || llc->ssap != 0x42 || llc->control != 0x03) continue;

                struct bpdu_header *bpdu = (struct bpdu_header *)(buffer + bpdu_offset);

                printf("---- BPDU Packet Received ----\n");
                printf("Source Port    : %d\n", src_port);
                printf("BPDU Type      : 0x%02X\n", bpdu->bpdu_type);
                printf("Flags          : 0x%02X\n", bpdu->flags);
                printf("Root Bridge ID : "); print_mac(&bpdu->root_id[2]); printf("\n");
                printf("Bridge ID      : "); print_mac(&bpdu->bridge_id[2]); printf("\n");
                printf("Port ID        : 0x%04X\n", ntohs(bpdu->port_id));
                printf("Root Path Cost : %u\n", ntohl(bpdu->root_path_cost));
                printf("Message Age    : %.2f sec\n", ntohs(bpdu->message_age) / 256.0);
                printf("Hello Time     : %.2f sec\n", ntohs(bpdu->hello_time) / 256.0);
                printf("Forward Delay  : %.2f sec\n", ntohs(bpdu->forward_delay) / 256.0);
                printf("------------------------------\n\n");

                if (is_port_link_up(src_port)) {
                    process_stp_logic(src_port, bpdu);
                    bpdu_received = 1;
                } else {
                    printf("[STP] Ignoring BPDU from port %d (link down)\n", src_port);
                }

                usleep(POLL_INTERVAL_MS * 1000);  // 100ms
                elapsed_ms += POLL_INTERVAL_MS;
            }

            close(sock);

            // DSA 모드 해제
            printf("[INFO] STP completed. Disabling DSA mode...\n");
            char normal_cmd[128];
            snprintf(normal_cmd, sizeof(normal_cmd), "mdio-tool w %s 0x%02X 0x04 0x007F", ethernet_iface, cpu_port_num);
            system(normal_cmd);
            dsa_mode_enabled = 0;

        } 
        //링크 다운시..
        else if ( new_link_down_port != -1 && !dsa_mode_enabled) {
            bool isSettingComplete = false;
            
            // 루트포트 초기화, 새로운 루트포트를 찾는다
            if (new_link_down_port != -1) {
                printf("[LINK DOWN] Port %d detected. Resetting STP state...\n", new_link_down_port);
                memset(current_root_id, 0xFF, sizeof(current_root_id));
                current_root_cost = UINT32_MAX;
                current_root_port = -1;
                current_port_id = 0xFFFF;
            }

            printf("[LINK DOWN] Port %d detected. Enabling DSA mode...\n", new_link_down_port);

            char dsa_cmd[128];
            snprintf(dsa_cmd, sizeof(dsa_cmd), "mdio-tool w %s 0x%02X 0x04 0x037F", ethernet_iface, cpu_port_num);
            system(dsa_cmd);
            dsa_mode_enabled = 1;


            //Find best BPDU
            int bpdu_received = 0;
            int elapsed_ms = 0;

            // BPDU 수신을 위해 RAW 소켓 열기
            int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
            if (sock < 0) {
                perror("socket");
                dsa_mode_enabled = 0;
                continue;
            }

            struct ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, ethernet_iface, IFNAMSIZ - 1);
            if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
                perror("SIOCGIFINDEX");
                close(sock);
                continue;
            }

            struct sockaddr_ll saddr = {0};
            saddr.sll_family = AF_PACKET;
            saddr.sll_protocol = htons(ETH_P_ALL);
            saddr.sll_ifindex = ifr.ifr_ifindex;
            if (bind(sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
                perror("bind");
                close(sock);
                continue;
            }

            // 수신 타임아웃 2초 설정
            struct timeval timeout = {2, 0};
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

            printf("[STP] Listening for BPDUs for 2 seconds...\n");

            gettimeofday(&start, NULL);

            while (elapsed_ms < MAX_WAIT_MS) {
                ssize_t len = recvfrom(sock, buffer, BUF_SIZE, 0, NULL, NULL);

                gettimeofday(&now, NULL);

                elapsed_ms = (now.tv_sec - start.tv_sec) * 1000L +
                    (now.tv_usec - start.tv_usec) / 1000L;

                if (elapsed_ms >= MAX_WAIT_MS) break;

                if (len <= 0) {
                    elapsed_ms += POLL_INTERVAL_MS;  // 타임아웃된 경우에도 증가시켜줘야 함
                    continue;
                }

                // BPDU 목적지 MAC 확인
                if (memcmp(buffer, "\x01\x80\xC2\x00\x00\x00", 6) != 0) continue;

                // DSA, LLC, BPDU offset
                int eth_offset = 12;
                int vlan_offset = eth_offset + 4;
                int dsa_offset = vlan_offset;
                int llc_offset = dsa_offset + 6;
                int bpdu_offset = llc_offset + 3;

                const uint8_t *dsa_tag = buffer + dsa_offset;
                int src_port = extract_dsa_src_port(dsa_tag);

                struct llc_header *llc = (struct llc_header *)(buffer + llc_offset);
                if (llc->dsap != 0x42 || llc->ssap != 0x42 || llc->control != 0x03) continue;

                struct bpdu_header *bpdu = (struct bpdu_header *)(buffer + bpdu_offset);

                printf("---- BPDU Packet Received ----\n");
                printf("Source Port    : %d\n", src_port);
                printf("BPDU Type      : 0x%02X\n", bpdu->bpdu_type);
                printf("Flags          : 0x%02X\n", bpdu->flags);
                printf("Root Bridge ID : "); print_mac(&bpdu->root_id[2]); printf("\n");
                printf("Bridge ID      : "); print_mac(&bpdu->bridge_id[2]); printf("\n");
                printf("Port ID        : 0x%04X\n", ntohs(bpdu->port_id));
                printf("Root Path Cost : %u\n", ntohl(bpdu->root_path_cost));
                printf("Message Age    : %.2f sec\n", ntohs(bpdu->message_age) / 256.0);
                printf("Hello Time     : %.2f sec\n", ntohs(bpdu->hello_time) / 256.0);
                printf("Forward Delay  : %.2f sec\n", ntohs(bpdu->forward_delay) / 256.0);
                printf("------------------------------\n\n");

                if (is_port_link_up(src_port)) {
                    process_stp_logic(src_port, bpdu);
                    bpdu_received = 1;
                } else {
                    printf("[STP] Ignoring BPDU from port %d (link down)\n", src_port);
                }

                usleep(POLL_INTERVAL_MS * 1000);  // 100ms
                elapsed_ms += POLL_INTERVAL_MS;
            }

            close(sock);

            printf("[INFO] STP completed. Disabling DSA mode...\n");
            char normal_cmd[128];
            snprintf(normal_cmd, sizeof(normal_cmd), "mdio-tool w %s 0x%02X 0x04 0x007F", ethernet_iface, cpu_port_num);
            system(normal_cmd);
            dsa_mode_enabled = 0;
        }
        usleep(1000000);  // 1초 주기 확인
    }
    pthread_exit(NULL);
}

void switch_init_setting () {
    char promisc_cmd[128];
    snprintf(promisc_cmd, sizeof(promisc_cmd), "ip link set %s promisc on", ethernet_iface);
    system(promisc_cmd);

    char cpu_cfg1[128], cpu_cfg2[128], cpu_cfg3[128];
    snprintf(cpu_cfg1, sizeof(cpu_cfg1), "mdio-tool write %s 0x1B 0x1A 0xB001", ethernet_iface);
    snprintf(cpu_cfg2, sizeof(cpu_cfg2), "mdio-tool write %s 0x1B 0x1A 0x8101", ethernet_iface);
    snprintf(cpu_cfg3, sizeof(cpu_cfg3), "mdio-tool write %s 0x1B 0x1A 0x8001", ethernet_iface);
    system(cpu_cfg1);
    system(cpu_cfg2);
    system(cpu_cfg3);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <start|stop>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "start") == 0) {
        //  이더넷 인터페이스 확인
        ethernet_iface = checkEthernetInterface();

        //  스위치 초기 셋팅 
        switch_init_setting();

        if (!ethernet_iface) {
            fprintf(stderr, "Failed to detect Ethernet interface.\n");
            return 1;
        }

        if (running) {
            printf("Already running!\n");
            return 0;
        }

        running = 1;
        // if (pthread_create(&recv_thread, NULL, receive_thread, ethernet_iface) != 0) {
        //     perror("pthread_create");
        //     return 1;
        // }

        if (pthread_create(&poll_thread, NULL, link_polling_thread, NULL) != 0) {
            perror("poll_thread");
            return 1;
        }

        printf("BPDU sniffer started.\n");

        // pthread_join(recv_thread, NULL);
        pthread_join(poll_thread, NULL);

    } else if (strcmp(argv[1], "stop") == 0) {
        running = 0;
        printf("BPDU sniffer stopped.\n");
    } else {
        printf("Invalid command. Use start or stop.\n");
        return 1;
    }

    return 0;
}
