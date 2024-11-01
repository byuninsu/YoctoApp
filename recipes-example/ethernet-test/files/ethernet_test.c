#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_PORT 8080        // Listening port
#define BUFFER_SIZE 1024

// 전역 변수와 플래그 선언
int serverSocket = -1;
int clientSocket = -1;
int running = 1; // 스레드 제어 플래그

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
            if (send(clientSocket, buffer, result, 0) < 0) {
                perror("Echo send failed");
                running = 0;
                break;
            } else {
                printf("Echoed back to server: %s\n", buffer);
            }
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
    } else {
        printf("Invalid mode. Use 'server' or 'client'.\n");
    }

    return 0;
}
