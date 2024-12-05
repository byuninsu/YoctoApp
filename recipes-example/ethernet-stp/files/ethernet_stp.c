#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if_ether.h>
#include <netinet/in.h>
#include <net/ethernet.h>


// 포트 상태 정의
#define PORT_STATE_DISABLED 0x0
#define PORT_STATE_BLOCKING 0x1
#define PORT_STATE_LEARNING 0x2
#define PORT_STATE_FORWARDING 0x3

#define BPDU_DEST_MAC "\x01\x80\xc2\x00\x00\x00"  // STP Destination MAC

// 포트 수 정의
#define NUM_PORTS 10

typedef struct {
    uint8_t protocol_id[2];
    uint8_t version_id;
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
} BPDU;

// 포트 상태 테이블
typedef struct {
    int port_num;
    uint8_t state;  // 현재 포트 상태
    int stp_enabled; // STP 활성화 여부
} PortState;

typedef struct {
    BPDU last_bpdu;      // 마지막으로 수신한 BPDU 메시지
    int received_count;  // 동일한 메시지 수신 횟수
    time_t last_received_time; // 마지막 BPDU 수신 시간
} PortBPDUStatus;

PortState port_states[NUM_PORTS];
PortBPDUStatus port_bpdu_status[NUM_PORTS]; // 각 포트의 BPDU 상태 저장

uint8_t current_root_id[8] = {0xFF}; // 현재 루트 브리지 ID (초기값은 최댓값)
uint32_t current_root_path_cost = UINT32_MAX; // 현재 루트 경로 비용 (초기값은 최댓값)

volatile int bpdu_running = 0; // BPDU 감지 스레드 실행 상태 플래그
pthread_t bpdu_thread;         // BPDU 감지 스레드
pthread_t recovery_thread;     // BPDU 타이머 복구 스레드

// BPDU 비교 함수
int compare_bpdu(const BPDU *a, const BPDU *b) {
    return memcmp(a, b, sizeof(BPDU));
}

//To_CPU DSA Tag 파싱
int extract_src_port(const char *dsa_tag) {
    if (dsa_tag == NULL) {
        fprintf(stderr, "Error: DSA Tag pointer is NULL\n");
        return -1; // 에러 반환
    }

    uint8_t src_port_byte = dsa_tag[1]; // 2번째 바이트
    //printf("DSA Tag raw byte[1]: 0x%02X\n", src_port_byte);

    int src_port = (src_port_byte >> 3) & 0x1F; // 상위 5비트 추출
    printf("Extracted Src_Port (raw): %d\n", src_port);

    return src_port + 1; // 포트 번호는 1부터 시작하므로 +1
}

// 포트 상태를 변경하는 함수
void set_port_state(const char *interface, int port_num, uint8_t state) {
    char command[128];
    char buffer[128];
    uint16_t current_value;

    snprintf(command, sizeof(command), "mdio-tool r %s 0x%02X 0x04", interface, port_num);
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        perror("Failed to execute mdio-tool command");
        exit(EXIT_FAILURE); // 강제 종료 또는 다른 에러 처리
    }

    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        current_value = (uint16_t)strtol(buffer, NULL, 16);
    } else {
        fprintf(stderr, "Failed to parse MDIO register value for port %d\n", port_num);
        pclose(fp);
        return;
    }
    pclose(fp);

    uint16_t new_value = (current_value & 0xFFFC) | (state & 0x03);

    snprintf(command, sizeof(command), "mdio-tool w %s 0x%02X 0x04 0x%04X", interface, port_num, new_value);
    int ret = system(command);
    if (ret != 0) {
        fprintf(stderr, "Failed to write MDIO register for port %d\n", port_num);
    } else {
        printf("Port %d state updated: 0x%04X -> 0x%04X\n", port_num, current_value, new_value);
        port_states[port_num - 1].state = state;
    }
}

// 포트 복구 함수
void recover_blocked_ports(const char *interface) {
    time_t current_time = time(NULL);

    for (int i = 0; i < NUM_PORTS; i++) {
        if (port_states[i].state == PORT_STATE_BLOCKING &&
            current_time - port_bpdu_status[i].last_received_time > 20) { // 20초 타임아웃
            printf("Recovering blocked port %d\n", i + 1);
            set_port_state(interface, i + 1, PORT_STATE_FORWARDING);
        }
    }
}

// BPDU 파싱 함수
void parse_bpdu(const char *buffer, BPDU *bpdu) {
    memcpy(bpdu, buffer, sizeof(BPDU));

    printf("Parsed BPDU:\n");
    printf("Root ID: ");
    for (int i = 0; i < 8; i++) {
        printf("%02X", bpdu->root_id[i]);
    }
    printf("\nRoot Path Cost: %d\n", ntohl(bpdu->root_path_cost));
    printf("Bridge ID: ");
    for (int i = 0; i < 8; i++) {
        printf("%02X", bpdu->bridge_id[i]);
    }
    printf("\nPort ID: %d\n", ntohs(bpdu->port_id));
}

// BPDU 기반 STP 로직
void stp_logic(const BPDU *bpdu, int port_num) {
    printf("Processing BPDU on Port %d\n", port_num);

    int is_new_root = memcmp(bpdu->root_id, current_root_id, sizeof(bpdu->root_id)) < 0 ||
                      (memcmp(bpdu->root_id, current_root_id, sizeof(bpdu->root_id)) == 0 &&
                       ntohl(bpdu->root_path_cost) < current_root_path_cost);

    if (is_new_root) {
        printf("New root bridge detected on port %d\n", port_num);
        memcpy(current_root_id, bpdu->root_id, sizeof(bpdu->root_id));
        current_root_path_cost = ntohl(bpdu->root_path_cost);

        for (int i = 0; i < NUM_PORTS; i++) {
            if (i + 1 == port_num) {
                set_port_state("eth0", port_num, PORT_STATE_FORWARDING);
            } else {
                set_port_state("eth0", i + 1, PORT_STATE_BLOCKING);
            }
        }
    }
}

// BPDU 수신 및 루프 감지 
void *bpdu_receiver_thread(void *arg) {
    const char *interface = (const char *)arg;
    int sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0) {
        perror("Socket creation failed");
        pthread_exit(NULL);
    }

    char buffer[1500];
    struct ethhdr *eth;
    BPDU bpdu;

    while (bpdu_running) {
        int len = recvfrom(sock, buffer, sizeof(buffer), 0, NULL, NULL);

        if (len < 0) { // 에러 발생 시 처리
            if (bpdu_running) { // 실행 중일 때만 에러 출력
                perror("Failed to receive packet");
            }
            break;
        }

        if (len < sizeof(BPDU)) { // 길이 검사 (len이 양수일 때만 의미 있음)
            fprintf(stderr, "Received packet too small for BPDU: %d bytes\n", len);
            continue; // 잘못된 패킷 무시하고 다음 반복으로 진행
        }

        // 유효한 데이터 복사
        memcpy(&bpdu, buffer, sizeof(BPDU)); 

        eth = (struct ethhdr *)buffer;

        // BPDU 메시지 필터링
        if (memcmp(eth->h_dest, BPDU_DEST_MAC, 6) == 0) {
            // To_CPU DSA Tag 파싱 (MAC 헤더 뒤에 위치)
            const char *dsa_tag = buffer + 12; // 12바이트: MAC 헤더 길이
            int port_num = extract_src_port(dsa_tag); // To_CPU DSA Tag에서 포트 번호 추출

            if (port_num <= 0 || port_num > NUM_PORTS) {
                printf("Invalid port number extracted: %d\n", port_num);
                continue;
            }

            // BPDU 파싱
            parse_bpdu(buffer + sizeof(struct ethhdr), &bpdu);

            // 동일한 BPDU 메시지 감지
            if (compare_bpdu(&bpdu, &port_bpdu_status[port_num - 1].last_bpdu) == 0) {
                port_bpdu_status[port_num - 1].received_count++;
                port_bpdu_status[port_num - 1].last_received_time = time(NULL);
                printf("Duplicate BPDU detected on port %d. Count: %d\n", port_num,
                       port_bpdu_status[port_num - 1].received_count);

                // 동일한 메시지가 일정 횟수 이상 감지되면 루프 발생
                if (port_bpdu_status[port_num - 1].received_count > 3) {
                    printf("Loop detected on port %d! Blocking port.\n", port_num);
                    set_port_state(interface, port_num, PORT_STATE_BLOCKING);
                }
            } else {
                // 새로운 BPDU 메시지로 업데이트
                memcpy(&port_bpdu_status[port_num - 1].last_bpdu, &bpdu, sizeof(BPDU));
                port_bpdu_status[port_num - 1].received_count = 1;
                port_bpdu_status[port_num - 1].last_received_time = time(NULL);
            }

            // STP 로직 적용
            stp_logic(&bpdu, port_num);
        }
    }

    close(sock);
    pthread_exit(NULL);
}


// 포트 복구 스레드
void *port_recovery_thread(void *arg) {
    const char *interface = (const char *)arg;

    while (bpdu_running) {
        recover_blocked_ports(interface);
        sleep(1);
    }

    pthread_exit(NULL);
}

// STP 활성화
void enable_stp(const char *interface) {
    for (int i = 1; i <= NUM_PORTS; i++) {
        port_states[i - 1].stp_enabled = 1;
    }

    bpdu_running = 1;

    pthread_create(&bpdu_thread, NULL, bpdu_receiver_thread, (void *)interface);
    pthread_create(&recovery_thread, NULL, port_recovery_thread, (void *)interface);
}

// STP 비활성화
void disable_stp(const char *interface) {
    bpdu_running = 0;

    pthread_join(bpdu_thread, NULL);
    pthread_join(recovery_thread, NULL);

    for (int i = 1; i <= NUM_PORTS; i++) {
        port_states[i - 1].stp_enabled = 0;
    }
}

// 앱 시작점
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <interface> <enable|disable>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *interface = argv[1];
    const char *command = argv[2];

    for (int i = 0; i < NUM_PORTS; i++) {
        port_states[i].port_num = i + 1;
        port_states[i].state = PORT_STATE_FORWARDING;
        port_states[i].stp_enabled = 0;
        port_bpdu_status[i].last_received_time = time(NULL);
    }

    if (strcmp(command, "enable") == 0) {
        enable_stp(interface);
    } else if (strcmp(command, "disable") == 0) {
        disable_stp(interface);
    } else {
        fprintf(stderr, "Invalid command. Use 'enable' or 'disable'.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
