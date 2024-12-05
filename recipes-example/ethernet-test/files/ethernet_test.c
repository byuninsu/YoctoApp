#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/socket.h>

#include "cJSON.h"
#include "stm32_control.h"
#include "rs232_control.h"
#include "ethernet_control.h"
#include "discrete_in.h"
#include "led_control.h"
#include "nvram_control.h"
#include "bit_manager.h"

#define RECEIVE_PORT 8080        // Listening port
#define SEND_PORT 8060        // Sending port
#define BUFFER_SIZE 1024
#define OPCODE_SIZE 10  // OPCode 크기 정의
#define MAX_PARM_SIZE 256  // Parm 최대 크기 정의
#define BROADCAST_IP "255.255.255.255"  // 브로드캐스트 주소
#define SSD_SMARTLOG_LENGTH 17  // SMARTLOG LENGTH

//OPCODE 정의
#define OPCODE_DISCRETE_OUT "CSRICR0001"
#define OPCODE_BOOT_MODE "CSRSMR0005" 
#define OPCODE_BIT_TEST_PBIT "CSCBMR0001"
#define OPCODE_BIT_TEST_CBIT "CSCBMR0002"
#define OPCODE_BIT_TEST_IBIT "CSCBMR0003" 
#define OPCODE_OS_SSD_BITE_TEST "CSRICR0006" 

// 메시지 구조체 정의
typedef struct {
    char OPCode[OPCODE_SIZE];       // OPCode (sizeof(char) * 10)
    uint16_t SequenceCount;         // SequenceCount (2 byte)
    uint16_t SizeofParm;            // SizeofParm (2 byte)
    char Parm[MAX_PARM_SIZE];       // Parm (sizeof(char) * SizeofParm 크기)
} Message;


// 함수 프로토타입 추가
void processingDiscreteOut(const char *Parm, uint16_t sequenceCount);
void processingBootMode(const char Parm[], uint16_t sequenceCount);
void processingBitResult(const char Parm[], uint16_t sequenceCount, uint32_t requestBit);
int init_recv_socket();
int init_send_socket();
void *receive_and_parse_message(void *arg);
void start_broadcast();
void stop_broadcast();
uint16_t parse_binary_string(const char *binary_str);
void uint16_to_binary_string(uint16_t value, char *output, size_t output_size);
void broadcastSendMessage(const Message *message); 
void processingSSDBite(const char Parm[], uint16_t sequenceCount, uint8_t ssd_type);


// 전역 변수와 플래그 선언
int recv_sockfd = -1;
int send_sockfd = -1;
int keep_running = 1;
int uart_fd;
pthread_t receiveThread;
struct sockaddr_in servaddr, cliaddr;

int init_recv_socket() {
    int broadcastEnable = 1;
    recv_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (recv_sockfd < 0) {
        perror("Receive socket creation failed");
        return -1;
    }

    if (setsockopt(recv_sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("setsockopt failed");
        close(recv_sockfd);
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(RECEIVE_PORT);

    if (bind(recv_sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        close(recv_sockfd);
        return -1;
    }

    printf("Receive socket initialized and bound to port %d\n", RECEIVE_PORT);
    return 0;
}

int init_send_socket() {
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Send socket creation failed");
        return -1;
    }

    int broadcastEnable = 1;
    if (setsockopt(send_sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("setsockopt for broadcast failed");
        close(send_sockfd);
        return -1;
    }

    printf("Send socket initialized\n");
    return 0;
}


void *receive_and_parse_message(void *arg) {
    socklen_t len = sizeof(cliaddr);
    Message msg;

    printf("Waiting to receive messages...\n");

    while (keep_running) {  // 무한 대기 루프
        // 수신 버퍼 초기화
        memset(&msg, 0, sizeof(msg));

        // 데이터 수신 (recvfrom은 기본적으로 블로킹 모드)
        ssize_t n = recvfrom(recv_sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) {
            perror("recvfrom failed");
            continue;
        }

        // 네트워크 바이트 순서를 호스트 바이트 순서로 변환
        uint16_t sequenceCount = ntohs(msg.SequenceCount);
        uint16_t sizeofParm = ntohs(msg.SizeofParm);

        // 데이터 파싱 및 출력
        printf("\n \n ----Received message----\n");
        printf("OPCode: %.*s\n", OPCODE_SIZE, msg.OPCode);
        printf("SequenceCount: %u\n", sequenceCount);
        printf("SizeofParm: %u\n", sizeofParm);
        printf("Parm: %.*s\n", sizeofParm, msg.Parm);
        printf("------------------------\n \n");

        printf(" processing receive data ..... \n");

        // 변환된 데이터와 함께 함수 호출
        if (memcmp(msg.OPCode, OPCODE_DISCRETE_OUT, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_DISCRETE_OUT message\n \n");
            processingDiscreteOut(msg.Parm, sequenceCount);
        } else if (memcmp(msg.OPCode, OPCODE_BOOT_MODE, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_BOOT_MODE message\n \n");
            processingBootMode(msg.Parm, sequenceCount);
        } else if (memcmp(msg.OPCode, OPCODE_BIT_TEST_PBIT, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_BIT_TEST_PBIT message\n \n");
            uint32_t bit_state = 2;
            processingBitResult(msg.Parm, sequenceCount, bit_state);
        } else if (memcmp(msg.OPCode, OPCODE_BIT_TEST_CBIT, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_BIT_TEST_CBIT message\n \n");
            uint32_t bit_state = 3;
            processingBitResult(msg.Parm, sequenceCount, bit_state);
        } else if (memcmp(msg.OPCode, OPCODE_BIT_TEST_IBIT, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_BIT_TEST_IBIT message\n \n");
            uint32_t bit_state = 4;
            processingBitResult(msg.Parm, sequenceCount, bit_state);
        } else if (memcmp(msg.OPCode, OPCODE_OS_SSD_BITE_TEST, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_OS_SSD_BITE_TEST message\n \n ");
            uint8_t ssd_type = 2;
            processingSSDBite(msg.Parm, sequenceCount, ssd_type);
        }
    }
}

// Discrete Out 처리 함수
void processingDiscreteOut(const char Parm[], uint16_t sequenceCount) {
    printf("processingDiscreteOut ++\n");

    uint16_t parsedParm = parse_binary_string(Parm);

    int result = setDiscreteOutAll(parsedParm);
    if (result != 0) {
        printf("Failed to set Discrete Out\n");
    } else {
        printf("Successfully set Discrete Out\n");

        Message message;
        strncpy(message.OPCode, OPCODE_DISCRETE_OUT, OPCODE_SIZE);
        
        // sequenceCount에 1을 더한 값을 네트워크 바이트 순서로 변환 후 할당
        sequenceCount++;
        message.SequenceCount = htons(sequenceCount);

        // getDiscreteOutAll() 값을 2진수 문자열로 변환
        uint16_t value = getDiscreteOutAll();
        char binary_string[17];  // 16자리 이진수 + null 문자
        uint16_to_binary_string(value, binary_string, sizeof(binary_string));

        // message.Parm에 이진수 문자열을 복사
        strncpy(message.Parm, binary_string, MAX_PARM_SIZE);

        // Parm의 실제 크기를 계산하여 SizeofParm에 저장
        message.SizeofParm = htons(strlen(binary_string));

        broadcastSendMessage(&message);
    }
}

void processingBootMode(const char Parm[], uint16_t sequenceCount) {
    printf("processingBootMode ++\n");

    // Boot Mode 읽기
    uint8_t readBootModeValue = ReadBootModeStatus();

    // 메시지 생성
    Message message;
    strncpy(message.OPCode, OPCODE_BOOT_MODE, OPCODE_SIZE);

    // SequenceCount 증가 및 변환
    sequenceCount++;
    printf("SequenceCount before htons: %d\n", sequenceCount);
    message.SequenceCount = htons(sequenceCount);
    printf("SequenceCount after htons (network): %d\n", ntohs(message.SequenceCount));

    // Boot Mode 값을 이진 문자열로 변환
    char binary_string[17];  // 16자리 이진수 + null 문자
    uint16_to_binary_string(readBootModeValue, binary_string, sizeof(binary_string));

    // 메시지에 데이터 할당
    strncpy(message.Parm, binary_string, MAX_PARM_SIZE);

    // SizeofParm 계산 및 변환
    int parmLength = strlen(binary_string);
    printf("SizeofParm before htons: %d\n", parmLength);
    message.SizeofParm = htons(parmLength);
    printf("SizeofParm after htons (network): %d\n", ntohs(message.SizeofParm));

    // 메시지 전송
    broadcastSendMessage(&message);

    printf("processingBootMode --\n");
}

// BIT_Test 요청 처리 함수
void processingBitResult(const char Parm[], uint16_t sequenceCount, uint32_t requestBit) {
    printf("processingBitResult ++\n");

    RequestBit(requestBit);

    Message message;

    if( requestBit == 1 ){
        strncpy(message.OPCode, OPCODE_BIT_TEST_PBIT, OPCODE_SIZE);
    } else if ( requestBit == 2 ){
        strncpy(message.OPCode, OPCODE_BIT_TEST_CBIT, OPCODE_SIZE);
    } else if ( requestBit == 3 ){
        strncpy(message.OPCode, OPCODE_BIT_TEST_IBIT, OPCODE_SIZE);
    }

    // sequenceCount에 1을 더한 값을 네트워크 바이트 순서로 변환 후 할당
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);

    uint32_t readBitValue = ReadBitResult(requestBit); // BIT 결과값 읽어와서

    // uint32_t 값을 문자열로 변환하여 Parm에 저장
    snprintf(message.Parm, MAX_PARM_SIZE, "%x", readBitValue);

    // Parm에 저장된 문자열의 길이를 SizeofParm에 저장
    message.SizeofParm = strlen(message.Parm);

    broadcastSendMessage(&message);
}

// processingSSDBite 처리 함수
void processingSSDBite(const char Parm[], uint16_t sequenceCount, uint8_t ssd_type) {
    printf("processingSSDBite ++\n");

    SSDSmartLog logValue = getSSDSmartLog(ssd_type);

    Message message;
    strncpy(message.OPCode, OPCODE_OS_SSD_BITE_TEST, OPCODE_SIZE);

    char buffer[256]; // 임시 문자열 저장 버퍼

    // 1. Critical Warning
    snprintf(buffer, sizeof(buffer), "Critical Warning: %d", logValue.critical_warning);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 2. Temperature
    snprintf(buffer, sizeof(buffer), "Temperature: %d C", logValue.temperature);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 3. Available Spare
    snprintf(buffer, sizeof(buffer), "Available Spare: %d%%", logValue.available_spare);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 4. Available Spare Threshold
    snprintf(buffer, sizeof(buffer), "Available Spare Threshold: %d%%", logValue.available_spare_threshold);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 5. Percentage Used
    snprintf(buffer, sizeof(buffer), "Percentage Used: %d%%", logValue.percentage_used);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 6. Data Units Read
    snprintf(buffer, sizeof(buffer), "Data Units Read: %llu", logValue.data_units_read);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 7. Data Units Written
    snprintf(buffer, sizeof(buffer), "Data Units Written: %llu", logValue.data_units_written);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 8. Host Read Commands
    snprintf(buffer, sizeof(buffer), "Host Read Commands: %llu", logValue.host_read_commands);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 9. Host Write Commands
    snprintf(buffer, sizeof(buffer), "Host Write Commands: %llu", logValue.host_write_commands);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 10. Power Cycles
    snprintf(buffer, sizeof(buffer), "Power Cycles: %d", logValue.power_cycles);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 11. Power On Hours
    snprintf(buffer, sizeof(buffer), "Power On Hours: %d", logValue.power_on_hours);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 12. Unsafe Shutdowns
    snprintf(buffer, sizeof(buffer), "Unsafe Shutdowns: %d", logValue.unsafe_shutdowns);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 13. Media Errors
    snprintf(buffer, sizeof(buffer), "Media Errors: %d", logValue.media_errors);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 14. Num Error Log Entries
    snprintf(buffer, sizeof(buffer), "Num Error Log Entries: %d", logValue.num_err_log_entries);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 15. Temperature Sensor 1
    snprintf(buffer, sizeof(buffer), "Temperature Sensor 1: %d C", logValue.temperature_sensor_1);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 16. Temperature Sensor 2
    snprintf(buffer, sizeof(buffer), "Temperature Sensor 2: %d C", logValue.temperature_sensor_2);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    // 17. Temperature Sensor 3
    snprintf(buffer, sizeof(buffer), "Temperature Sensor 3: %d C", logValue.temperature_sensor_3);
    sequenceCount++;
    message.SequenceCount = htons(sequenceCount);
    strncpy(message.Parm, buffer, MAX_PARM_SIZE);
    message.SizeofParm = htons(strlen(message.Parm));
    broadcastSendMessage(&message);

    printf("processingSSDBite --\n");
}

// 브로드캐스트 메시지 전송 함수
void broadcastSendMessage(const Message *message) {
    struct sockaddr_in broadcastAddr;
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(SEND_PORT);
    broadcastAddr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    printf("\n\n----Send message----\n");
    printf("OPCode: %.*s\n", OPCODE_SIZE, message->OPCode);
    printf("SequenceCount: %u\n", ntohs(message->SequenceCount)); // 변환 후 출력
    printf("SizeofParm: %u\n", ntohs(message->SizeofParm));       // 변환 후 출력
    printf("Parm: %.*s\n", ntohs(message->SizeofParm), message->Parm);
    printf("--------------------\n");

    int sendLen = sendto(send_sockfd, message, sizeof(Message), 0, (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
    if (sendLen < 0) {
        perror("Broadcast message send failed");
        printf("\n");
    } else {
        printf("Broadcast message sent.\n \n");
        printf("\n");
    }
}


// RS232 Init Config
int configure_uart(int fd) {
    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag &= ~PARENB;   // No parity
    options.c_cflag &= ~CSTOPB;   // 1 stop bit
    options.c_cflag &= ~CSIZE;    // Mask the character size bits
    options.c_cflag |= CS8;       // 8 data bits
    options.c_cflag |= CREAD | CLOCAL; // Enable receiver and set local mode
    tcsetattr(fd, TCSANOW, &options);

    int flags = fcntl(uart_fd, F_GETFL,0);
    fcntl(uart_fd, F_SETFL, flags | O_NONBLOCK );

    return 0;
}


void start_broadcast(void) {
    init_recv_socket();
    init_send_socket();






    keep_running = 1;
    if (pthread_create(&receiveThread, NULL, receive_and_parse_message, NULL) != 0) {
        perror("Failed to create receive thread");
        close(recv_sockfd);
        close(send_sockfd);
        return;
    }

    pthread_join(receiveThread, NULL);
}

// 리시브 중지 함수
void stop_broadcast() {
    printf("Stopping receive.\n");
    keep_running = 0;
    if (recv_sockfd != -1) {
        pthread_join(receiveThread, NULL);
        printf("receive_and_parse_message thread has stopped.\n"); 
    }
    close(recv_sockfd);
    close(send_sockfd);
}

uint16_t readDiscreteIn(void){
    uint16_t readValue; 
    uint32_t discreteIn7to0Value = GetDiscreteState7to0();
    uint32_t discreteIn15to8Value = GetDiscreteState15to8();

    readValue = (discreteIn7to0Value & 0xFF) <<8;
    readValue |= discreteIn15to8Value & 0xFF;

    return readValue;
}

uint16_t parse_binary_string(const char *binary_str) {
    uint16_t result = 0;

    // 문자열 길이 확인 및 최대 16비트 길이만큼만 처리
    size_t len = strlen(binary_str);
    if (len > 16) {
        fprintf(stderr, "Warning: binary string exceeds 16 bits, truncating to 16 bits.\n");
        len = 16;
    }

    for (size_t i = 0; i < len; i++) {
        if (binary_str[i] == '1') {
            result |= (1 << (len - 1 - i));  // 각 비트를 왼쪽에서부터 채웁니다.
        } else if (binary_str[i] != '0') {
            fprintf(stderr, "Error: Invalid character in binary string: %c\n", binary_str[i]);
            return 0;  // 잘못된 입력에 대해 0을 반환할 수 있음
        }
    }

    return result;
}

void uint16_to_binary_string(uint16_t value, char *output, size_t output_size) {
    // output_size가 17이어야 16비트 + null 문자 ('\0') 를 담을 수 있음
    if (output_size < 17) {
        fprintf(stderr, "Output buffer is too small.\n");
        return;
    }

    output[16] = '\0';  // Null terminator 추가

    for (int i = 15; i >= 0; i--) {
        output[15 - i] = (value & (1 << i)) ? '1' : '0';
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <broadcast> <start/stop>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "broadcast") == 0) {
        if (strcmp(argv[2], "start") == 0) {
            start_broadcast();
        } else if (strcmp(argv[2], "stop") == 0) {
            stop_broadcast();
        } else {
            printf("Invalid command. Use 'start' or 'stop'.\n");
        }
    }

    return 0;
}
