#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define SERVER_PORT 8080        // Listening port
#define BUFFER_SIZE 1024

// 전역 변수로 소켓과 플래그를 선언하여 스레드에서 접근할 수 있게 합니다
int serverSocket = -1;
int clientSocket = -1;
int running = 1; // 스레드 제어 플래그

// 수신 스레드 함수
void* receive_and_echo(void* arg) {
    char buffer[BUFFER_SIZE];
    int recvLen;

    while (running) {
        // 클라이언트로부터 데이터 수신
        recvLen = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        if (recvLen <= 0) {
            if (recvLen == 0) {
                printf("Client disconnected.\n");
            } else {
                perror("Receive failed");
            }
            break;
        }

        buffer[recvLen] = '\0'; // 받은 데이터를 문자열로 처리
        printf("Received from client: %s\n", buffer);

        // 에코 기능: 받은 데이터를 클라이언트로 바로 전송
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

    // 소켓 생성
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Server socket created.\n");

    // 서버 주소 설정
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // 소켓 바인딩
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Socket bind failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    printf("Socket bound to port %d.\n", SERVER_PORT);

    // 연결 대기 상태로 설정
    if (listen(serverSocket, 5) < 0) {
        perror("Socket listen failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d.\n", SERVER_PORT);

    // 클라이언트 연결 수락
    addrSize = sizeof(clientAddr);
    clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &addrSize);
    if (clientSocket < 0) {
        perror("Client accept failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    printf("Client connected.\n");

    // 수신 및 에코 스레드 생성
    running = 1; // 플래그 활성화
    pthread_create(&recvThread, NULL, receive_and_echo, NULL);

    // 수신 스레드 종료 대기
    pthread_join(recvThread, NULL);

    // 클라이언트 연결 종료
    close(clientSocket);
    printf("Client connection closed.\n");
}

// 서버 중지 함수
void stop_server() {
    running = 0; // 수신 스레드 종료 요청

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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <start/stop>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "start") == 0) {
        start_server();
    } else if (strcmp(argv[1], "stop") == 0) {
        stop_server();
    } else {
        printf("Invalid command. Use 'start' or 'stop'.\n");
        return 1;
    }

    return 0;
}
