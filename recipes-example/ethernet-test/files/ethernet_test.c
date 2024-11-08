#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <termios.h>
#include <fcntl.h>

#include "discrete_in.h"
#include "led_control.h"

#define SERVER_PORT 8080        // Listening port
#define BUFFER_SIZE 1024

#define STX_VALUE 0x56
#define DLC_VALUE 0x15
#define STATUS_READY 0x00
#define STATUS_RUN 0x01
#define EOT_VALUE 0x4F
#define DATA_TYPE_DISCRETE_IN 0x01
#define DATA_TYPE_DISCRETE_OUT 0x02
#define DATA_TYPE_RS232 0x03

typedef struct {
    uint8_t STX;                // 1 Byte
    uint8_t DLC;                // 1 Byte
    uint8_t Status;             // 1 Byte
    uint8_t Data[15];           // 15 Bytes
    uint8_t CRC[2];             // 2 Bytes
    uint8_t EOT;                // 1 Byte
} Packet;

//함수 프로토타입 추가
Packet parseSendPacket(void);
void initialize_packet(Packet *packet, uint8_t status, uint8_t *data, size_t data_len);
uint16_t crc16_ccitt(uint8_t *data, size_t len);
uint16_t readDiscreteIn(void);
int broadSocketSetup(const char* ipAddr);
// void *broadcastThreadStart(void *arg);

// 전역 변수와 플래그 선언
int serverSocket = -1;
int clientSocket = -1;
int broadSocket = -1;
int running = 1; // 스레드 제어 플래그
int keep_running = 1;
int uart_fd;
pthread_t broadcastThread;
struct sockaddr_in broadcastAddr;


// 패킷 초기화 함수
void initialize_packet(Packet *packet, uint8_t status, uint8_t *data, size_t data_len) {
    packet->STX = STX_VALUE;
    packet->DLC = DLC_VALUE;
    packet->Status = status;
    memset(packet->Data, 0, 15);
    memcpy(packet->Data, data, data_len);

    uint16_t crc = crc16_ccitt(packet->Data, 15);
    packet->CRC[0] = (crc >> 8) & 0xFF;
    packet->CRC[1] = crc & 0xFF;
    packet->EOT = EOT_VALUE;
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

// 일회성 브로드캐스트 메시지 전송 함수
void broadcastSendMessage(const char* ipAddr, const char* message) {
    // 브로드캐스트 소켓 초기화
    if (broadSocketSetup(ipAddr) != 0) {
        fprintf(stderr, "Failed to setup broadcast socket\n");
        return;
    }

    // 메시지 전송
    int sendLen = sendto(broadSocket, message, strlen(message), 0, 
                         (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
    if (sendLen < 0) {
        perror("Broadcast message send failed");
    } else {
        printf("Broadcast message sent: %s\n", message);
    }

    // 소켓 닫기
    close(broadSocket);
    broadSocket = -1;  // 소켓 상태 초기화
}

int broadSocketSetup(const char* ipAddr) {
    printf("Setting up broadcast socket with IP: %s\n", ipAddr);
    broadSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadSocket < 0) {
        perror("broadSocket creation failed");
        return 1;
    }

    int broadcastEnable = 1;
    if (setsockopt(broadSocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("setsockopt failed");
        close(broadSocket);
        return 1;
    }

    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(SERVER_PORT);
    broadcastAddr.sin_addr.s_addr = inet_addr(ipAddr);

    printf("Broadcast socket setup completed.\n");

    return 0;
}


// 브로드캐스트 스레드 함수
void *broadcastThreadStart(void *arg) {
    printf("Broadcast thread started.\n");

    printf("Entering broadcast loop. keep_running = %d\n", keep_running);

    while (keep_running) {
        printf("Calling parseSendPacket...\n");

        Packet packet = parseSendPacket();

         printf("Sending broadcast message...\n");
        int sendLen = sendto(broadSocket, &packet, sizeof(Packet), 0,
                             (struct sockaddr *)&broadcastAddr, sizeof(broadcastAddr));

        printf("DATA: ");
        for (int i = 0; i < 15; i++) {
            printf("%02x ", packet.Data[i]);
        }
        printf("\n");

        if (sendLen < 0) {
            perror("Broadcast message send failed");
            break;
        }
        printf("Broadcast message sent.\n");
        usleep(1000000); // 1초 대기

         printf("Loop iteration complete. keep_running = %d\n", keep_running);
    }

    close(broadSocket); // 스레드 종료 시 소켓 닫기
    broadSocket = -1;
    printf("Broadcast thread ending.\n");
    return NULL;
}


// 브로드캐스트 시작 함수
void start_broadcast(const char* ipAddr) {
    printf("Starting broadcast with IP: %s\n", ipAddr);
    
    if (broadSocketSetup(ipAddr) != 0) {
        fprintf(stderr, "Failed to setup broadcast socket\n");
        return;
    }

    keep_running = 1;

    int result = pthread_create(&broadcastThread, NULL, broadcastThreadStart, NULL);
    pthread_join(broadcastThread, NULL);

    if (result != 0) {
        fprintf(stderr, "Error creating broadcast thread: %d\n", result);
    } else {
        printf("Broadcast thread created successfully.\n");
    }
}

// 브로드캐스트 중지 함수
void stop_broadcast() {
    printf("Stopping broadcast.\n");
    keep_running = 0;
    if (broadSocket != -1) {
        pthread_join(broadcastThread, NULL);
        printf("Broadcast thread has stopped.\n"); 
    }
}

Packet parseSendPacket(void) { 

    printf("Start parseSendPacket ++\n");

    Packet packet;
    uint8_t data[15];
    char buffer[BUFFER_SIZE];
    char sendRS232buffer[8];
    int bytes_read;

    // Uart 초기화
    if (configure_uart(uart_fd) < 0) {
        fprintf(stderr, "UART init failed\n");
        return packet;  // 실패 시 초기화된 빈 패킷 반환
    }

    printf("UART init success ");

    //discrete-in value 가져오기
    uint16_t discreteInValue = readDiscreteIn();
    printf("Discrete IN value: %04x\n", discreteInValue);

    //discrete-out value 가져오기
    uint16_t discreteOutValue = getDiscreteOutAll();
    printf("Discrete OUT value: %04x\n", discreteOutValue);

    // RS232 읽기
    bytes_read = read(uart_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0'; // Null-terminate the buffer
        
        //sendRS232buffer 0으로 초기화
        memset(sendRS232buffer, 0, 8);
        //읽어온 내용 복사
        strcpy(sendRS232buffer, buffer);

        printf("RS232 read data: %s\n", sendRS232buffer);
    } else if (bytes_read < 0) {
        //sendRS232buffer 0으로 초기화
        memset(sendRS232buffer, 0, 8);
    }

    data[0] = DATA_TYPE_DISCRETE_IN;
    data[1] = (discreteInValue >> 8) & 0xFF;
    data[2] = discreteInValue & 0xFF;
    data[3] = DATA_TYPE_DISCRETE_OUT;
    data[4] = (discreteOutValue >> 8) & 0xFF;
    data[5] = discreteOutValue & 0xFF;
    data[6] = DATA_TYPE_RS232;

    memcpy(&data[7], sendRS232buffer, bytes_read > 8 ? 8 : bytes_read);
    
    printf("Packet data ready  \n");

    initialize_packet(&packet, STATUS_RUN, data, 15);

    printf("End parseSendPacket --\n");
    return packet;
}

// 서버의 수신 및 에코 스레드 함수
void* receive_and_echo(void* arg) {
    char buffer[BUFFER_SIZE];
    int recvLen;

    while (running) {
        recvLen = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        if (recvLen <= 0) {
            if (recvLen == 0) {
                printf("Client disconnected.\n");
            } else {
                perror("Receive failed");
            }
            break;
        }
        buffer[recvLen] = '\0';
        printf("Received from client: %s\n", buffer);

        if (send(clientSocket, buffer, recvLen, 0) < 0) {
            perror("Send failed");
            break;
        } else {
            printf("Echoed back to client: %s\n", buffer);
        }
    }
    return NULL;
}

// 서버 시작 함수
void start_server() {
    struct sockaddr_in serverAddr, clientAddr;
    pthread_t recvThread;
    socklen_t addrSize;

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Server socket created.\n");

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Socket bind failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    printf("Socket bound to port %d.\n", SERVER_PORT);

    if (listen(serverSocket, 5) < 0) {
        perror("Socket listen failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d.\n", SERVER_PORT);

    addrSize = sizeof(clientAddr);
    clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &addrSize);
    if (clientSocket < 0) {
        perror("Client accept failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    printf("Client connected.\n");

    running = 1;
    pthread_create(&recvThread, NULL, receive_and_echo, NULL);
    pthread_join(recvThread, NULL);

    close(clientSocket);
    printf("Client connection closed.\n");
}

// 서버 중지 함수
void stop_server() {
    running = 0;
    if (clientSocket >= 0) {
        close(clientSocket);
        clientSocket = -1;
        printf("Client connection closed.\n");
    }
    if (serverSocket >= 0) {
        close(serverSocket);
        serverSocket = -1;
        printf("Server socket closed.\n");
    }
}

// 클라이언트 송신 스레드 함수
void* client_send(void* arg) {
    while (running) {
        const char* message = "Hello from Jetson client!";
        send(clientSocket, message, strlen(message), 0);
        printf("Sent to server: %s\n", message);
         usleep(3000000);// 3초마다 메시지 전송
    }
    return NULL;
}

// 클라이언트 수신 및 에코 스레드 함수
void* client_receive(void* arg) {
    char buffer[BUFFER_SIZE];
    int result;

    while (running) {
        result = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        if (result > 0) {
            buffer[result] = '\0';
            printf("Received from server: %s\n", buffer);

            // 받은 데이터를 서버에 에코로 다시 전송
            // if (send(clientSocket, buffer, result, 0) < 0) {
            //     perror("Echo send failed");
            //     running = 0;
            //     break;
            // } else {
            //     printf("Echoed back to server: %s\n", buffer);
            // }
        } else {
            printf("Disconnected from server.\n");
            running = 0; // 서버가 연결을 끊으면 클라이언트도 종료
            break;
        }
    }
    return NULL;
}

// 클라이언트 시작 함수
void start_client(const char* server_ip) {
    struct sockaddr_in serverAddr;
    pthread_t sendThread, recvThread;

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, server_ip, &serverAddr.sin_addr);

    printf("Connecting to server at %s:%d...\n", server_ip, SERVER_PORT);
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connection to server failed");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }
    printf("Connected to server.\n");

    running = 1;
    pthread_create(&sendThread, NULL, client_send, NULL);
    pthread_create(&recvThread, NULL, client_receive, NULL);

    pthread_join(sendThread, NULL);
    pthread_join(recvThread, NULL);
}

// 클라이언트 중지 함수
void stop_client() {
    running = 0;
    if (clientSocket >= 0) {
        close(clientSocket);
        clientSocket = -1;
        printf("Client socket closed.\n");
    }
}

// CRC 계산 함수
uint16_t crc16_ccitt(uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    size_t i, j;

    for (i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

uint16_t readDiscreteIn(void){
    uint16_t readValue; 
    uint32_t discreteIn7to0Value = GetDiscreteState7to0();
    uint32_t discreteIn15to8Value = GetDiscreteState15to8();

    readValue = (discreteIn7to0Value & 0xFF) <<8;
    readValue |= discreteIn15to8Value & 0xFF;

    return readValue;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <server/client> <start/stop> [server_ip for client mode]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "server") == 0) {
        if (strcmp(argv[2], "start") == 0) {
            start_server();
        } else if (strcmp(argv[2], "stop") == 0) {
            stop_server();
        } else {
            printf("Invalid command for server. Use 'start' or 'stop'.\n");
        }
    } else if (strcmp(argv[1], "client") == 0) {
        if (strcmp(argv[2], "start") == 0) {
            if (argc < 4) {
                printf("Please specify the server IP for client mode.\n");
                return 1;
            }
            start_client(argv[3]);
        } else if (strcmp(argv[2], "stop") == 0) {
            stop_client();
        } else {
            printf("Invalid command for client. Use 'start' or 'stop'.\n");
        }
    } else if (strcmp(argv[1], "broadcast") == 0) {
        if (strcmp(argv[2], "start") == 0) {
            if (argc < 4) {
                printf("Please specify the broadcast IP for broadcast start.\n");
                return 1;
            }
            start_broadcast(argv[3]);
        } else if (strcmp(argv[2], "stop") == 0) {
            stop_broadcast();
        } else if (strcmp(argv[2], "send")== 0) {
            if (argc < 5) {
            printf("Usage: %s broadcast send <IP> <Message>\n", argv[0]);
            return 1;
        }
            broadcastSendMessage(argv[3],argv[4]);
        } else {
            printf("Invalid command for broadcast. Use 'start' or 'stop'.\n");
        }
    } else {
        printf("Invalid mode. Use 'broadcast start <IP>' or 'broadcast stop'.\n");
    }

    return 0;
}
