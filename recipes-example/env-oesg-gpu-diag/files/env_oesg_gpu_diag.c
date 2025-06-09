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
#define CopperStart 200
#define CopperCount 6
#define OpticStart 206
#define OpticCount 5

int isOpticSet = 0;

pid_t optic_pids[OpticCount];  // 자식 PID 저장용
pid_t child_pids[CopperCount];  // 자식 PID 저장용

int kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;
    int hit = 0;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();
    if (ch != EOF) {
        ungetc(ch, stdin);
        hit = 1;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    return hit;
}

void print_menu() {
    char *settingValue = "";
    if( isOpticSet ){
        settingValue = "Optic";
    } else if ( !isOpticSet ) {
        settingValue = "Copper";
    }
    printf("==== QT Test Menu =====\n");
    printf("1. Ethernet Copper Test \n");
    printf("2. Ethernet Optic Test \n");
    printf("3. Discrete In (12CH) Test\n");
    printf("4. Discrete Out (12CH) Test\n");
    printf("5. Program Pin Test\n");
    printf("6. RS-232 Serial Test\n");
    printf("7. PBIT Test\n");
    printf("8. Data SSD R/W Test\n");
    printf("9. NVRAM R/W Test\n");
    printf("0. Change Ethernet Setting, Current : %s\n",settingValue );

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

int detect_current_ethernet_mode() {
    char *iface = checkEthernetInterface();
    char command[128] = {0};
    char result_buf[64] = {0};
    FILE *fp;

    snprintf(command, sizeof(command), "mdio-tool read %s 0x09 0x00", iface);

    fp = popen(command, "r");
    if (fp == NULL) {
        perror("Failed to run mdio-tool");
        return 0;  // 기본은 copper로 간주
    }

    if (fgets(result_buf, sizeof(result_buf), fp) != NULL) {
        unsigned int reg_value = 0;
        if (sscanf(result_buf, "%x", &reg_value) == 1) {
            unsigned int last_byte = reg_value & 0xFF;

            if (last_byte == 0x09) {
                pclose(fp);
                return 1;  // Optic
            } else if (last_byte == 0x0C || last_byte == 0x0F) {
                pclose(fp);
                return 0;  // Copper
            } else {
                printf("Unknown CMODE value: 0x%04X\n", reg_value);
            }
        } else {
            printf("Failed to parse mdio-tool output: %s\n", result_buf);
        }
    } else {
        printf("No output from mdio-tool\n");
    }

    pclose(fp);
    return 0;  // 기본 fallback: Copper
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

void ethernet_test_copper() {
    const char *base_ip = "192.168.10.";
    int i;

    printf("Starting persistent iperf3 clients to Copper (192.168.10.200~205)...\n");
    printf("Press any key to stop the test.\n");

    for (i = 0; i < CopperCount; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            // 자식 프로세스
            char ip[32];
            int port = 5002 + i;
            snprintf(ip, sizeof(ip), "%s%d", base_ip, CopperStart + i);

            char cmd[128];
            // 무제한 실행 (-t 생략 시 기본 10초지만, iperf3 서버에 따라 지속됨)
            snprintf(cmd, sizeof(cmd), "iperf3 -c %s -p %d -t 36000", ip, port);

            printf("Launching: %s\n", cmd);
            execl("/bin/sh", "sh", "-c", cmd, (char *) NULL);

            perror("execl failed");
            _exit(1);
        } else if (pid > 0) {
            child_pids[i] = pid;  // 부모는 PID 저장
        } else {
            perror("fork failed");
        }

    }

    // 사용자 키 입력 기다림
    while (!kbhit()) {
        usleep(500000); // 0.5초마다 체크
    }

    printf("\nKey pressed. Terminating iperf3 clients...\n");

    // 모든 자식 프로세스 종료
    for (i = 0; i < CopperCount; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGTERM);  // 정상 종료 시도
        }
    }

    // 종료 대기
    for (i = 0; i < CopperCount; i++) {
        int status;
        waitpid(child_pids[i], &status, 0);
    }

    printf("All iperf3 clients terminated.\n");
}

void ethernet_test_optic() {
    const char *base_ip = "192.168.10.";
    int i;
    uint8_t result = 0;

    

    printf("Running Ethernet Test to Optic (iperf3 to 192.168.10.206~210)...\n");
    printf("Press any key to stop the test.\n");

    for (i = 0; i < OpticCount; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            // 자식 프로세스
            char ip[32];
            int port = 5008 + i;
            snprintf(ip, sizeof(ip), "%s%d", base_ip, OpticStart + i);

            char cmd[128];
            snprintf(cmd, sizeof(cmd), "iperf3 -c %s -p %d -t 36000", ip, port);

            printf("Launching: %s\n", cmd);
            execl("/bin/sh", "sh", "-c", cmd, (char *) NULL);

            perror("execl failed");
            _exit(1);
        } else if (pid > 0) {
            optic_pids[i] = pid;
        } else {
            perror("fork failed");
        }
    }

    // 사용자 입력 대기
    while (!kbhit()) {
        usleep(500000);  // 0.5초 간격으로 체크
    }

    printf("\nKey pressed. Terminating iperf3 optic clients...\n");

    for (i = 0; i < OpticCount; i++) {
        if (optic_pids[i] > 0) {
            kill(optic_pids[i], SIGTERM);
        }
    }

    for (i = 0; i < OpticCount; i++) {
        int status;
        waitpid(optic_pids[i], &status, 0);
    }

    result =   // 포트 복구
    printf("Optic Ethernet tests completed.\n");
}

void change_ethernet_mode() {
    if ( isOpticSet ){
        printf("Set Ethernet Copper Port\n");
        setDefaultPort();
        isOpticSet = 0;
    } else if ( !isOpticSet ) {
        printf("Set Ethernet Optic Port\n");
        setOpticPort();
        isOpticSet = 1;
    }
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

    printf("Starting RS232 transmission (\"ABCDE\")...\n");
    printf("Press any key to stop.\n");

    while (1) {
        ssize_t len = write(fd, exampleMessage, strlen(exampleMessage));
        if (len != (ssize_t)strlen(exampleMessage)) {
            perror("Error writing to RS232");
            break;
        }

        printf("RS232 Sent: %s\n", exampleMessage);
        usleep(1000000);  

        if (kbhit()) {
            printf("Key pressed. Stopping RS232 transmission.\n");
            break;
        }
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
        bitString[31] = 0;
        bitString[30 - i] = (readBitValue & (1U << i)) ? '1' : '0';
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

   isOpticSet = detect_current_ethernet_mode();

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
            case '1': ethernet_test_copper(); break;
            case '2': ethernet_test_optic(); break;
            case '3': discrete_in_test(); break;
            case '4': discrete_out_test(); break;
            case '5': program_pin_test(); break;
            case '6': rs232_test(); break;
            case '7': pbit_test(); break;
            case '8': data_ssd_test(); break;
            case '9': nvram_test(); break;
            case '0': change_ethernet_mode(); break;

            default: printf("Invalid selection. Please try again.\n"); break;
        }

        printf("\n ============== Press any key to return to menu ==============\n\n");
        getch();  // 다음 메뉴로 넘어가기 전 멈춤
    }

    return 0;
}
