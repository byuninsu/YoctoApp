#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>

#include "cJSON.h"
#include "stm32_control.h"
#include "rs232_control.h"
#include "usb_control.h"
#include "ethernet_control.h"
#include "discrete_in.h"
#include "led_control.h"
#include "nvram_control.h"
#include "bit_manager.h"
#include "optic_control.h"

#define VERSION "V1.0"

void print_menu() {
    printf("==== QT Test Menu =====\n");
    printf("1. Ethernet Test \n");
    printf("2. Discrete In (12CH) Test\n");
    printf("3. Discrete Out (12CH) Test\n");
    printf("4. Program Pin Test\n");
    printf("5. RS-232 Serial Test\n");
    printf("6. PBIT Test\n");
    printf("7. Data SSD R/W Test\n");
    printf("8. NVRAM R/W Test\n");

    printf("Press ESC to exit");
}

int getch(void) {
    struct termios oldt, newt;
    int ch;

    tcgetattr(STDIN_FILENO, &oldt);        
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);      
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);   
    ch = getchar();                       
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);   

    return ch;
}

uint16_t readDiscreteIn(void){
    uint16_t readValue; 
    uint32_t discreteIn7to0Value = GetDiscreteState7to0();
    uint32_t discreteIn15to8Value = GetDiscreteState15to8();

    readValue = (discreteIn15to8Value & 0xFF) <<8;
    readValue |= discreteIn7to0Value & 0xFF;

    return readValue;
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

void ethernet_test() {
    const char *base_ip = "192.168.10.";
    int CopperStart = 200;
    int OpticStart = 206;
    int CopperCount = 6;
    int OpticCount = 5;
    int i;
    uint8_t result = 0;

    printf("Running Ethernet Test to Copper (iperf3 to 192.168.10.200~205)...\n");

    for (i = 0; i < CopperCount; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            // Child process
            char ip[32];
            int port = 5002 + i;
            snprintf(ip, sizeof(ip), "%s%d", base_ip, CopperStart + i);

            char cmd[128];
            snprintf(cmd, sizeof(cmd), "iperf3 -c %s -p %d", ip, port);

            printf("Launching: %s\n", cmd);
            execl("/bin/sh", "sh", "-c", cmd, (char *) NULL);

            perror("execl failed");
            _exit(1);
        }
    }

    for (i = 0; i < CopperCount; i++) {
        int status;
        wait(&status);
    }

    result = setOpticPort();

    usleep(9000000);

    printf("Running Ethernet Test to Optic (iperf3 to 192.168.10.206~210)...\n");

    for (i = 0; i < OpticCount; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            // Child process
            char ip[32];
            int port = 5008 + i;
            snprintf(ip, sizeof(ip), "%s%d", base_ip, OpticStart + i);

            char cmd[128];
            snprintf(cmd, sizeof(cmd), "iperf3 -c %s -p %d", ip, port);

            printf("Launching: %s\n", cmd);
            execl("/bin/sh", "sh", "-c", cmd, (char *) NULL);

            perror("execl failed");
            _exit(1);
        }
    }

    result = setDefaultPort();

    for (i = 0; i < OpticCount; i++) {
        int status;
        wait(&status);
    }

    printf("All Ethernet tests completed.\n");
}

void discrete_in_test() {
    uint16_t readDiscreteVaule;
    char discreteString[17]; // 16비트 + 널문자

    readDiscreteVaule = readDiscreteIn();

    uint16_to_binary_string(readDiscreteVaule, discreteString, sizeof(discreteString));

    printf("\n[Discrete In (12ch) Test Executed]\n\n");

    printf("readDis = 0x%04x\n\n", readDiscreteVaule);

    printf(" *** Discrete Value = %s *** \n\n", discreteString);

}

void discrete_out_test() {
    printf("\n[Discrete Out (12ch) Test Executed]\n\n");

    printf("\n Set Descrete [ON] \n\n");

    printf("\n Discrete 0 On ! \n");
    usleep(1000000);
    setDiscreteOut(0, 1);

    printf("\n Discrete 1 On ! \n");
    usleep(1000000);
    setDiscreteOut(1, 1);

    printf("\n Discrete 2 On ! \n");
    usleep(1000000);
    setDiscreteOut(2, 1);

    printf("\n Discrete 3 On ! \n");
    usleep(1000000);
    setDiscreteOut(3, 1);

    printf("\n Discrete 4 On ! \n");
    usleep(1000000);
    setDiscreteOut(4, 1);

    printf("\n Discrete 5 On ! \n");
    usleep(1000000);
    setDiscreteOut(5, 1);

    printf("\n Discrete 6 On ! \n");
    usleep(1000000);
    setDiscreteOut(6, 1);

    printf("\n Discrete 7 On ! \n");
    usleep(1000000);
    setDiscreteOut(7, 1);

    printf("\n Discrete 8 On ! \n");
    usleep(1000000);
    setDiscreteOut(8, 1);

    printf("\n Discrete 9 On ! \n");
    usleep(1000000);
    setDiscreteOut(9, 1);

    printf("\n Discrete 10 On ! \n");
    usleep(1000000);
    setDiscreteOut(10, 1);

    printf("\n Discrete 11 On ! \n\n");
    usleep(1000000);
    setDiscreteOut(11, 1);

    usleep(5000000);

    printf("\n Set Descrete [OFF] \n\n");

    printf("\n Discrete 0 Off ! \n");
    usleep(1000000);
    setDiscreteOut(0, 0);

    printf("\n Discrete 1 Off ! \n");
    usleep(1000000);
    setDiscreteOut(1, 0);

    printf("\n Discrete 2 Off ! \n");
    usleep(1000000);
    setDiscreteOut(2, 0);

    printf("\n Discrete 3 Off ! \n");
    usleep(1000000);
    setDiscreteOut(3, 0);

    printf("\n Discrete 4 Off ! \n");
    usleep(1000000);
    setDiscreteOut(4, 0);

    printf("\n Discrete 5 Off ! \n");
    usleep(1000000);
    setDiscreteOut(5, 0);

    printf("\n Discrete 6 Off ! \n");
    usleep(1000000);
    setDiscreteOut(6, 0);

    printf("\n Discrete 7 Off ! \n");
    usleep(1000000);
    setDiscreteOut(7, 0);

    printf("\n Discrete 8 Off ! \n");
    usleep(1000000);
    setDiscreteOut(8, 0);

    printf("\n Discrete 9 Off ! \n");
    usleep(1000000);
    setDiscreteOut(9, 0);

    printf("\n Discrete 10 Off ! \n");
    usleep(1000000);
    setDiscreteOut(10, 0);

    printf("\n Discrete 11 Off ! \n\n");
    usleep(1000000);
    setDiscreteOut(11, 0);
}

void program_pin_test() {
    uint16_t readDiscreteVaule;
    char discreteString[17]; // 16비트 + 널문자

    readDiscreteVaule = readDiscreteIn();

    uint16_to_binary_string(readDiscreteVaule, discreteString, sizeof(discreteString));

    printf("\n[Program Pin Test Executed]\n\n");

    printf(" *** Discrete Value = %s *** \n\n", discreteString);

    int isHigh12 = readDiscreteVaule & (1 << 12);
    int isHigh13 = readDiscreteVaule & (1 << 13);
    int isHigh14 = readDiscreteVaule & (1 << 14);
    int isHigh15 = readDiscreteVaule & (1 << 15);

    // Default 포트 설정
    if (isHigh12 ==0 && isHigh13 && isHigh14 && isHigh15) { //0111
        printf("Program Pin 0111 (777X) Detected ! \n\n");
    } else if (isHigh12 && isHigh13 == 0 && isHigh14 == 0 && isHigh15 ==0) { //1000
        printf("Program Pin 0111 (787) Detected ! \n\n");
    } else if (isHigh12==0 && isHigh13 == 0 && isHigh14 && isHigh15) { //0011
        printf("Program Pin 0111 (737MAX) Detected ! \n\n");
    } else {
        printf("Invalid Program Pin: %d%d%d%d\n\n",
            !!isHigh12, !!isHigh13, !!isHigh14, !!isHigh15);
    }
}


void rs232_test() {
    int fd = open("/dev/ttyTHS0", O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("Error opening /dev/ttyTHS0");
        return;
    }

    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) {
        perror("Error getting tty attributes");
        close(fd);
        return;
    }

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 1;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("Error setting tty attributes");
        close(fd);
        return;
    }

    char exampleMessage[] = "ABCDE";
    printf("\n wr_len : %zu\n", strlen(exampleMessage));

    if (write(fd, exampleMessage, strlen(exampleMessage)) != (ssize_t)strlen(exampleMessage)) {
        perror("Error writing to RS232");
    } else {
        printf("RS232 Sent : %s\n", exampleMessage);
        printf("================================\n");
    }

    close(fd);
}

void pbit_test() { 
    uint32_t readBitValue = ReadBitResult(2);

    printf("\n[PBIT Test Executed]\n\n");
    printf("RequestPBIT()\n");
    printf("======================= PBIT READY =======================\n");
    
    char bitString[33];  
    for (int i = 31; i >= 0; i--) {
        bitString[31 - i] = (readBitValue & (1U << i)) ? '1' : '0';
    }
    bitString[32] = '\0';

    printf("======== PBIT Data : %s ======== \n\n", bitString);
}

void data_ssd_test() {
    const char *filepath = "/mnt/dataSSD/ssd_test1.txt";
    const char *writeData = "SIL-OESG_TEST";
    char readBuffer[256] = {0};

    printf("\n[DATA SSD R/W Test Executed]\n\n");

    // 파일 쓰기
    FILE *fp = fopen(filepath, "w");
    if (fp == NULL) {
        perror("Failed to write to SSD file");
        return;
    }
    fprintf(fp, "%s", writeData);
    fclose(fp);

    printf("Data stored in SSD at %s\n", filepath);

    // 파일 읽기
    fp = fopen(filepath, "r");
    if (fp == NULL) {
        perror("Failed to read from SSD file");
        return;
    }
    fgets(readBuffer, sizeof(readBuffer), fp);
    fclose(fp);

    printf("Data read from SSD : %s\n", readBuffer);
}

void nvram_test() { 
    const char *writeData = "0xABCDE";
    char readBuffer[21] = {0};  // 20바이트 + \0

    if (WriteQtTestValueToNVRAM(writeData) != 0) {
        printf("Failed to write to NVRAM\n");
        return;
    }

    if (ReadQtTestValueFromNVRAM(readBuffer) != 0) {
        printf("Failed to read from NVRAM\n");
        return;
    }

    printf("\n[NVRAM R/W Test Executed]\n\n");
    printf("SAVE Data : %s\n", writeData);

    printf("Value written to NVRAM (dummy_data): %s\n", writeData);
    printf("Value read from NVRAM (dummy_data): %s\n\n", readBuffer);
}

int main(void) {
    printf("START_ENV_OESG_GPU_diag\n");
    printf("Current Version: %s\n\n", VERSION);

    while (1) {
        print_menu();

        int ch = getch();  
        // ESC key = 27
        if (ch == 27) {
            printf("\nENV_OESG_GPU_diag Exiting...\n\n");
            break;
        }

        printf("Selected Option: %c\n", ch);

        switch (ch) {
            case '1': ethernet_test(); break;
            case '2': discrete_in_test(); break;
            case '3': discrete_out_test(); break;
            case '4': program_pin_test(); break;
            case '5': rs232_test(); break;
            case '6': pbit_test(); break;
            case '7': data_ssd_test(); break;
            case '8': nvram_test(); break;
            default: printf("Invalid selection. Please try again.\n"); break;
        }

        printf("\n ============== Press any key to return to menu ==============\n\n");
        getch();  // 다음 메뉴로 넘어가기 전 멈춤
    }

    return 0;
}