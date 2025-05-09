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
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <termios.h>
#include <time.h>


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

#define RECEIVE_PORT 8080        // Listening port
#define SEND_PORT 8060        // Sending port
#define DATA_PORT 8070
#define BUFFER_SIZE 1024
#define OPCODE_SIZE 17  // OPCode 크기 정의
#define MAX_PARM_SIZE 256  // Parm 최대 크기 정의
#define BROADCAST_IP "192.168.10.255"  // 브로드캐스트 주소
#define SETTING_IP "192.168.10.110"  // IP 셋팅 주소
#define SSD_SMARTLOG_LENGTH 17  // SMARTLOG LENGTH
#define BOOT_SIL_MODE "1"  
#define BOOT_NOMAL_MODE "0"  
#define MAX_INTERFACE_NAME 16
#define ENV_TEMP_SIZE 5
#define ENV_VOLT_SIZE 5
#define ENV_NVRAM_VALUE_SIZE 7
#define CONFIG_FILE "/home/ethernet_test/ethernet_test_config"
#define SSD_FILE_PATH "/mnt/dataSSD/silTestFile"
#define JSON_FILE_PATH "/mnt/dataSSD/ssdLog_test.json"
#define HOLDUP_LOG_FILE_PATH "/mnt/dataSSD/HoldupLog.json"
#define PROGRAM_PIN_LOG_FILE_PATH "/mnt/dataSSD/ProgramPinErrorLog"
#define SERIAL_DEVICE "/dev/ttyTHS0"
#define ENV_SSD_VALUE_SIZE 16
#define MTU_SIZE 1300
#define WRITE_BUFFER_SIZE 268435456

//OPCODE 정의
#define OPCODE_DISCRETE_IN "OESG-GPU-ATP-0000"
#define OPCODE_DISCRETE_OUT "OESG-GPU-ATP-0100"
#define OPCODE_RESET_REASON "OESG-GPU-ATP-0110"
#define OPCODE_BOOT_CONDITION "OESG-GPU-ATP-0000" 
#define OPCODE_SYSTEM_COUNT "OESG-GPU-ATP-0120"
#define OPCODE_BIT_TEST_PBIT "OESG-GPU-ATP-0130"
#define OPCODE_BIT_TEST_CBIT "OESG-GPU-ATP-0140"
#define OPCODE_BIT_TEST_IBIT "CSRBMR0003" 
#define OPCODE_BIT_TEST_RESULT_SAVE "OESG-GPU-ATP-0150" 
#define OPCODE_OS_SSD_BITE_TEST "OESG-GPU-ATP-0180" 
#define OPCODE_INIT_NVRAM_SSD "OESG-GPU-ATP-0300"
#define OPCODE_ETHERNET_COPPER_OPTIC "OESG-GPU-ATP-0210"
#define OPCODE_ETHERNET_ENABLE_DISABLE "OESG-GPU-ATP-0200"
#define OPCODE_RS232_USB_EN_DISABLE "OESG-GPU-ATP-0240"  
#define OPCODE_PUSH_BUTTON_CHECK "OESG-GPU-ATP-0190"
#define OPCODE_WATCHDOG_TIMEOUT "OESG-GPU-ATP-0270"
#define OPCODE_HOLDUP_CHECK "OESG-GPU-ATP-0260"
#define OPCODE_GET_HW_INFO "OESG-GPU-ATP-0160"
#define OPCODE_SYSTEM_TIME "CSRSMR0007"
#define OPCODE_SSD_CAPACITY "OESG-GPU-ATP-0280"
#define OPCODE_PROGRAM_PIN_CHECK "OESG-GPU-ATP-0290"
#define OPCODE_SRRIAL_SPEED_CONTROL "OESG-GPU-ATP-0250"
#define OPCODE_ETHERNET_PORT_SETTING_CHANGE "OESG-GPU-ATP-0220"
#define OPCODE_BOOTING_TIME_CHECK "OESG-GPU-ATP-0310"


//ENV OPCODE 정의
#define OPCODE_ENV_DISCRETE "OESG-GPU-ATP-0040"
#define OPCODE_ENV_RS232 "OESG-GPU-ATP-0000"
#define OPCODE_ENV_ETHERNET "OESG-GPU-ATP-0050"
#define OPCODE_ENV_TEMPURATURE "OESG-GPU-ATP-0060"
#define OPCODE_ENV_NVRAM "OESG-GPU-ATP-0070"
#define OPCODE_ENV_SSD "OESG-GPU-ATP-0080"
#define OPCODE_ENV_POWER_MONITOR "OESG-GPU-ATP-0090"

//TTA OPCODE 정의
#define OPCODE_TTA_BULK_DATA "TTA0001"
#define OPCODE_TTA_SSD_WRITE "TTA0002"
#define OPCODE_TTA_CPU_CLOCK "TTA0003"

typedef struct {
    char OPCode[OPCODE_SIZE + 1]; // OPCode + NULL 문자
    uint16_t SequenceCount;       // SequenceCount (16비트 정수)
    uint16_t SizeofParm;          // SizeofParm (16비트 정수)
    char Parm[MAX_PARM_SIZE];     // Parm
} Message;


// 함수 프로토타입 추가
void processingDiscreteIn(const char *Parm, uint16_t sequenceCount);
void processingBootCondition(const char Parm[], uint16_t sequenceCount);
void processingResetReson(uint16_t sequenceCount);
void processingPushbutton(uint16_t sequenceCount);
void processingSystemCount(uint16_t sequenceCount);
void processingBitResult(const char Parm[], uint16_t sequenceCount, uint32_t requestBit);
void processingCBitResult(const char Parm[], uint16_t sequenceCount, uint32_t requestBit);
void processingBitSave(const char Parm[], uint16_t sequenceCount);
void processingDiscreteOut(const char Parm[], uint16_t sequenceCount);
void processingSSDBite(const char Parm[], uint16_t sequenceCount, uint8_t ssd_type);
void processingSystemTime(uint16_t sequenceCount);
void processingSSDCapacity(uint16_t sequenceCount);
void processingInitSSDNvram(const char Parm[], uint16_t sequenceCount);
void processingEthernetSetting(const char Parm[], uint16_t sequenceCount, uint8_t type);
void processingEthernetEnable(uint16_t sequenceCount);
void processingGetHwInfo(uint16_t sequenceCount);
void processingHoldupCheck(uint16_t sequenceCount);
void processingInitUsbRs232EnDisable(const char Parm[], uint16_t sequenceCount, uint8_t act);
void processingWatchdogTimeout(uint16_t sequenceCount);
void processingSerialSpeedControl(uint16_t sequenceCount);
void processingEnvDiscrete(uint16_t sequenceCount);
void processingEnvRs232(uint16_t sequenceCount);
void processingEnvEthernet(uint16_t sequenceCount);
void processingEnvTemperature(uint16_t sequenceCount);
void processingEnvNvram(const char Parm[], uint16_t sequenceCount);
void processingEnvSSD(const char Parm[], uint16_t sequenceCount);
void processingEnvPowerMonitor(uint16_t sequenceCount);
void processingTTABulkData(uint16_t sequenceCount);
void processingTTASsdWrite(uint16_t sequenceCount);
void processingTTACpuClock(uint16_t sequenceCount);
void processingBootingTimeCheck(uint16_t sequenceCount);
void processingProgramPinCheck(uint16_t sequenceCount);
void processingEthernetPortChange(uint16_t sequenceCount);

int init_recv_socket();
int init_send_socket();
void *receive_and_parse_message(void *arg);
void start_broadcast();
void stop_broadcast();
uint16_t parse_binary_string(const char *binary_str);
void uint16_to_binary_string(uint16_t value, char *output, size_t output_size);
void broadcastSendMessage(const Message *message); 
void updateConfigFile(const char *opcode, int status);
int readConfigFile(const char *opcode);
uint16_t readDiscreteIn(void);
void ensureConfigDirectoryExists();
void checkAndProcessOpcodes();
int configureSerialPort(int fd, speed_t baudRate);
void configureNetwork();
void *dataProcessingThread(void *arg);
void *ssdWriteProcessingThread(void *arg);
void logProgramPinError(void);
void ethernetInit(void);



// 전역 변수와 플래그 선언
int recv_sockfd = -1;
int send_sockfd = -1;
int keep_running = 1;
int uart_fd;
pthread_t receiveThread;
struct sockaddr_in servaddr, cliaddr;
static int stopThread = 1;  // 이더넷 수신 스레드 종료 플래그
static int ssdStopThread = 1;  // ssd 처리 스레드 종료 플래그
static int dataSocket;      // 8070 포트 수신 소켓
     
pthread_t dataThreadId; // 데이터 수신 스레드
pthread_t ssdThreadId; // SSD 수신 스레드

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
    char buffer[BUFFER_SIZE]; // 수신 버퍼
    Message msg;              // 메시지 구조체

    printf("Waiting to receive messages...\n");

    while (keep_running) { // 무한 대기 루프
        memset(buffer, 0, BUFFER_SIZE);

        ssize_t n = recvfrom(recv_sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) {
            perror("recvfrom failed");
            continue;
        }

        printf("\n Raw buffer: %.*s\n\n", (int)n, buffer);

        // 최소 OPCODE_SIZE 이상의 데이터가 있어야 메시지 처리 가능
        if (n >= OPCODE_SIZE) {
            memset(&msg, 0, sizeof(Message));
            strncpy(msg.OPCode, buffer, OPCODE_SIZE);
            msg.OPCode[OPCODE_SIZE] = '\0';

            uint16_t sequenceCount = 0, sizeofParm = 0;

           
            if (n >= OPCODE_SIZE + 1 + 4) { // 최소한 시퀀스카운트(2자리) + sizeofParm(2자리) 포함해야 함
                if (isdigit(buffer[OPCODE_SIZE + 1]) && isdigit(buffer[OPCODE_SIZE + 2])) {
                    sequenceCount = (uint16_t)((buffer[OPCODE_SIZE + 1] - '0') * 10 +
                                            (buffer[OPCODE_SIZE + 2] - '0'));
                }
                if (isdigit(buffer[OPCODE_SIZE + 3]) && isdigit(buffer[OPCODE_SIZE + 4])) {
                    sizeofParm = (uint16_t)((buffer[OPCODE_SIZE + 3] - '0') * 10 +
                                            (buffer[OPCODE_SIZE + 4] - '0'));
                }

                // Parm 데이터 추출
                if (sizeofParm > 0 && n >= (OPCODE_SIZE + 1 + 4 + sizeofParm)) {
                    strncpy(msg.Parm, buffer + OPCODE_SIZE + 1 + 4, sizeofParm);
                    msg.Parm[sizeofParm] = '\0';
                }
            }

            msg.SequenceCount = sequenceCount;
            msg.SizeofParm = sizeofParm;

            printf("----Received message----\n");
            printf("OPCode: %s\n", msg.OPCode);
            printf("SequenceCount: %u\n", msg.SequenceCount);
            printf("sizeofParm: %u\n", msg.SizeofParm);
            printf("Parm: %s\n", msg.SizeofParm > 0 ? msg.Parm : "(none)");
            printf("------------------------\n\n");

            // Parm 값이 없을 경우 응답 메시지 전송
            if (msg.SizeofParm == 0 && msg.SequenceCount == 0) {
                Message responseMsg = {0};
                strncpy(responseMsg.OPCode, msg.OPCode, OPCODE_SIZE);
                responseMsg.SequenceCount = msg.SequenceCount + 1;
                responseMsg.SizeofParm = 0;
                broadcastSendMessage(&responseMsg);
                printf("Sent response for Parm-less message\n");
            }

            // OPCode에 따라 적절한 처리 함수 호출
            if (memcmp(msg.OPCode, OPCODE_ENV_DISCRETE, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_ENV_DISCRETE message\n\n");
                processingEnvDiscrete(msg.SequenceCount + 1);
            } else if (memcmp(msg.OPCode, OPCODE_ENV_RS232, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_ENV_RS232 message\n\n");
                processingEnvRs232(msg.SequenceCount + 1);
            } else if (memcmp(msg.OPCode, OPCODE_ENV_ETHERNET, OPCODE_SIZE) == 0 && msg.SequenceCount != 0) {
                printf("Received OPCODE_ENV_ETHERNET message\n\n");
                processingEnvEthernet(msg.SequenceCount);
            } else if (memcmp(msg.OPCode, OPCODE_ENV_TEMPURATURE, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_ENV_TEMPURATURE message\n\n");
                processingEnvTemperature(msg.SequenceCount + 1);
            } else if (memcmp(msg.OPCode, OPCODE_ENV_NVRAM, OPCODE_SIZE) == 0 && msg.SequenceCount == 2) {
                printf("Received OPCODE_ENV_NVRAM message\n\n");
                processingEnvNvram(msg.Parm, msg.SequenceCount);
            } else if (memcmp(msg.OPCode, OPCODE_ENV_SSD, OPCODE_SIZE) == 0 && msg.SequenceCount == 2) {
                printf("Received OPCODE_ENV_SSD message\n\n");
                processingEnvSSD(msg.Parm, msg.SequenceCount);
            } else if (memcmp(msg.OPCode, OPCODE_ENV_POWER_MONITOR, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_ENV_POWER_MONITOR message\n\n");
                processingEnvPowerMonitor(msg.SequenceCount + 1);
            } else if (memcmp(msg.OPCode, OPCODE_DISCRETE_IN, OPCODE_SIZE) == 0 && sizeofParm != 0) {
                printf("Received OPCODE_DISCRETE_IN message\n\n");
                processingDiscreteIn(msg.Parm, msg.SequenceCount);
            } else if (memcmp(msg.OPCode, OPCODE_BOOT_CONDITION, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_BOOT_CONDITION message\n\n");
                processingBootCondition(msg.Parm, msg.SequenceCount + 1);
            } else if (memcmp(msg.OPCode, OPCODE_SYSTEM_COUNT, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_SYSTEM_COUNT message\n\n");
                processingSystemCount(msg.SequenceCount + 1);
            } else if (memcmp(msg.OPCode, OPCODE_PUSH_BUTTON_CHECK, OPCODE_SIZE) == 0) {
                processingPushbutton(msg.SequenceCount);
            } else if (memcmp(msg.OPCode, OPCODE_RESET_REASON, OPCODE_SIZE) == 0) {
                processingResetReson(msg.SequenceCount +1);
            } else if (memcmp(msg.OPCode, OPCODE_BIT_TEST_PBIT, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_BIT_TEST_PBIT message\n\n");
                uint32_t bit_state = 2;
                processingBitResult(msg.Parm, msg.SequenceCount + 1, bit_state);
            } else if (memcmp(msg.OPCode, OPCODE_BIT_TEST_CBIT, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_BIT_TEST_CBIT message\n\n");
                uint32_t bit_state = 3;
                processingCBitResult(msg.Parm, msg.SequenceCount + 1, bit_state);
            } else if (memcmp(msg.OPCode, OPCODE_BIT_TEST_IBIT, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_BIT_TEST_IBIT message\n\n");
                uint32_t bit_state = 4;
                processingBitResult(msg.Parm, msg.SequenceCount + 1, bit_state);
            } else if (memcmp(msg.OPCode, OPCODE_BIT_TEST_RESULT_SAVE, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_BIT_TEST_RESULT_SAVE message\n\n");
                processingBitSave(msg.Parm, msg.SequenceCount + 1);
            } else if (memcmp(msg.OPCode, OPCODE_DISCRETE_OUT, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_DISCRETE_OUT message\n\n");
                    processingDiscreteOut(msg.Parm, msg.SequenceCount+1);
            } else if (memcmp(msg.OPCode, OPCODE_GET_HW_INFO, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_GET_HW_INFO message\n\n");
                processingGetHwInfo(msg.SequenceCount + 1);
            } else if (memcmp(msg.OPCode, OPCODE_HOLDUP_CHECK, OPCODE_SIZE) == 0 ) {
                printf("Received OPCODE_HOLDUP_CHECK message\n\n");
                processingHoldupCheck(msg.SequenceCount);
            } else if (memcmp(msg.OPCode, OPCODE_OS_SSD_BITE_TEST, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_OS_SSD_BITE_TEST message\n\n");
                uint8_t ssd_type_os = 2;
                uint8_t ssd_type_data = 3;
                
                if( msg.SequenceCount == 17){
                    processingSSDBite(msg.Parm, msg.SequenceCount, ssd_type_data);
                } else if ( msg.SequenceCount == 0 ) {
                    processingSSDBite(msg.Parm, msg.SequenceCount + 1, ssd_type_os);
                }
            } else if (memcmp(msg.OPCode, OPCODE_INIT_NVRAM_SSD, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_INIT_NVRAM_SSD message\n\n");
                processingInitSSDNvram(msg.Parm, msg.SequenceCount + 1);
            } else if (memcmp(msg.OPCode, OPCODE_SSD_CAPACITY, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_SSD_CAPACITY message\n\n");
                processingSSDCapacity(msg.SequenceCount + 1);
            } else if (memcmp(msg.OPCode, OPCODE_ETHERNET_COPPER_OPTIC, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_ETHERNET_COPPER_OPTIC message\n\n");
                uint8_t copper = 1;
                uint8_t optic = 2;
                if (msg.SequenceCount == 2) {
                    processingEthernetSetting(msg.Parm, msg.SequenceCount, optic);
                } else if (msg.SequenceCount == 4) {
                    processingEthernetSetting(msg.Parm, msg.SequenceCount, copper);
                }
            } else if (memcmp(msg.OPCode, OPCODE_ETHERNET_ENABLE_DISABLE, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_ETHERNET_ENABLE_DISABLE message\n\n");
                if(msg.SequenceCount >= 2 ){
                    processingEthernetEnable(msg.SequenceCount);
                }
            } else if (memcmp(msg.OPCode, OPCODE_RS232_USB_EN_DISABLE, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_RS232_USB_EN_DISABLE message\n\n");
                uint8_t inputAct = 1, inputDeAct = 1;
                if (msg.SequenceCount == 3) {
                    processingInitUsbRs232EnDisable(msg.Parm, msg.SequenceCount, inputAct);
                } else if (msg.SequenceCount == 0) {
                    processingInitUsbRs232EnDisable(msg.Parm, msg.SequenceCount + 1, inputDeAct);
                }
            } else if (memcmp(msg.OPCode, OPCODE_WATCHDOG_TIMEOUT, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_WATCHDOG_TIMEOUT message\n\n");
                processingWatchdogTimeout(msg.SequenceCount);
            } else if (memcmp(msg.OPCode, OPCODE_SRRIAL_SPEED_CONTROL, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_SRRIAL_SPEED_CONTROL message\n\n");
                processingSerialSpeedControl(msg.SequenceCount);
            } else if (memcmp(msg.OPCode, OPCODE_BOOTING_TIME_CHECK, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_BOOTING_TIME_CHECK message\n\n");
                processingBootingTimeCheck(msg.SequenceCount);
            } else if (memcmp(msg.OPCode, OPCODE_PROGRAM_PIN_CHECK, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_PROGRAM_PIN_CHECK message\n\n");
                if( msg.SequenceCount == 2){
                    processingProgramPinCheck(msg.SequenceCount);
                }
            } else if (memcmp(msg.OPCode, OPCODE_ETHERNET_PORT_SETTING_CHANGE, OPCODE_SIZE) == 0) {
                printf("Received OPCODE_ETHERNET_PORT_SETTING_CHANGE message\n\n");
                processingEthernetPortChange(msg.SequenceCount);
            }
        }
    }
    return NULL;
}


// ENV Discrete 처리 함수
void processingEnvDiscrete(uint16_t sequenceCount) {
    printf("processingEnvDiscrete ++\n");

    // SequenceCount 증가
    sequenceCount++;

    // 현재 Discrete In 상태 읽기
    uint16_t discreteOutValue = readDiscreteIn();

    // Discrete In 상태를 Out 에 셋팅
    uint32_t result = setDiscreteOutAll(discreteOutValue);

    // Parm 길이 설정
    uint16_t parmLength = 1;

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    strncpy(message->OPCode, OPCODE_ENV_DISCRETE, OPCODE_SIZE);

    message->SequenceCount = sequenceCount;
    message->SizeofParm = parmLength;

    // 초기화 결과를 Parm에 저장
    message->Parm[0] = (result == 0) ? '0' : '1';  // 성공: '0', 실패: '1'


    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    printf("processingEnvDiscrete --\n");
}

// ENV RS-232 처리 함수
void processingEnvRs232(uint16_t sequenceCount) {
    printf("processingEnvRs232 ++\n");
    int isSuccess = 0;  

    // RS-232 디바이스 열기
    int fd = open("/dev/ttyTHS0", O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("Error opening /dev/ttyTHS0");
        return;
    }

    // 시리얼 포트 설정
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) {
        perror("Error getting tty attributes");
        close(fd);
        return;
    }

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8비트 문자
    tty.c_iflag &= ~IGNBRK;                      // 브레이크 조건 무시하지 않음
    tty.c_lflag = 0;                             // Canonical 모드 비활성화
    tty.c_oflag = 0;                             // 출력 처리 비활성화
    tty.c_cc[VMIN]  = 1;                         // 최소 1 문자 읽기
    tty.c_cc[VTIME] = 1;                         // 0.1초 타임아웃

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);      // 소프트웨어 흐름 제어 비활성화
    tty.c_cflag |= (CLOCAL | CREAD);             // 로컬 연결, 읽기 가능 활성화
    tty.c_cflag &= ~(PARENB | PARODD);           // 패리티 비활성화
    tty.c_cflag &= ~CSTOPB;                      // 1 Stop bit
    tty.c_cflag &= ~CRTSCTS;                     // 하드웨어 흐름 제어 비활성화

    cfsetospeed(&tty, B115200);  // 출력 속도 설정
    cfsetispeed(&tty, B115200);  // 입력 속도 설정

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("Error setting tty attributes");
        close(fd);
        return;
    }

    // 입력 버퍼 비우기
    if (tcflush(fd, TCIFLUSH) == -1) {
        perror("Failed to flush input buffer");
    }

    printf("Waiting for input RS232...\n");

    char validBuffer[20] = {0};  // 유효 문자만 저장할 버퍼
    int totalValid = 0;

    // 유효 문자로 20바이트 읽기
    while (totalValid < 20) {
        char temp[1] = {0};
        int bytesRead = read(fd, temp, 1);
        if (bytesRead > 0) {
            if (temp[0] != '\0') {  // 숫자나 알파벳만 필터링
                validBuffer[totalValid++] = temp[0];
                printf("Valid character added: %c\n", temp[0]);
            }
        } else if (bytesRead == 0) {
            printf("No data received, waiting...\n");
        } else if (bytesRead < 0) {
            perror("Error reading from /dev/ttyTHS0");
            close(fd);
            return;
        }
    }

    // 반복 패턴 찾기
    char detectedPattern[6] = {0};
    for (int i = 0; i <= totalValid - 5; i++) {
        int matchCount = 1;
        for (int j = i + 1; j <= totalValid - 5; j++) {
            if (strncmp(&validBuffer[i], &validBuffer[j], 5) == 0) {
                matchCount++;
            }
        }
        if (matchCount > 1) {
            strncpy(detectedPattern, &validBuffer[i], 5);
            break;
        }
    }

    if (strlen(detectedPattern) == 5) {
        printf("Detected pattern: %s\n", detectedPattern);
        isSuccess = 1;
    } else {
        printf("Failed to detect a valid pattern.\n");
    }

    // RS232로 전송
    if (write(fd, detectedPattern, 5) != 5) {
        perror("Error writing to RS232");
    } else {
        printf("Sent to RS232: %s\n", detectedPattern);
    }

    // 첫 번째 메시지 전송 (5개의 값을 Parm으로 보냄)
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        close(fd);
        return;
    }

    memset(message, 0, sizeof(Message));

    // SequenceCount 증가 후 RS232 읽은 값 송부
    sequenceCount++;

    strncpy(message->OPCode, OPCODE_ENV_RS232, OPCODE_SIZE);
    message->SequenceCount = sequenceCount;
    message->SizeofParm = 5; // Parm에 읽은 데이터 길이 설정
    strncpy(message->Parm, detectedPattern, 5);  // 10글자를 Parm에 복사

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 첫 번째 메시지 완료 후 메모리 해제
    free(message);

    // SequenceCount 증가 후 성공 메시지 전송
    sequenceCount++;

    // 성공/실패 메시지 초기화
    message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        close(fd);
        return;
    }

    memset(message, 0, sizeof(Message));
    strncpy(message->OPCode, OPCODE_ENV_RS232, OPCODE_SIZE);
    message->SequenceCount = sequenceCount;
    message->SizeofParm = 1; // 성공/실패 상태 길이

    // 초기화 결과를 Parm에 저장
    message->Parm[0] = (isSuccess) ? '0' : '1';  // 성공: '0', 실패: '1'


    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제 및 파일 디스크립터 닫기
    free(message);

    // 입력 버퍼 비우기
    if (tcflush(fd, TCIFLUSH) == -1) {
        perror("Failed to flush input buffer");
    }

    // 출력 버퍼 비우기
    if (tcflush(fd, TCOFLUSH) == -1) {
        perror("Failed to flush output buffer");
    }

    close(fd);

    printf("processingEnvRs232 --\n");
}

// ENV Ethernet 처리 함수
void processingEnvEthernet(uint16_t sequenceCount) {
    printf("processingEnvEthernet ++\n");

    uint8_t result = 0;

    uint16_t discreteValue = readDiscreteIn();
    int isHigh12 = discreteValue & (1 << 12);
    int isHigh13 = discreteValue & (1 << 13);
    int isHigh14 = discreteValue & (1 << 14);
    int isHigh15 = discreteValue & (1 << 15);

    if ( sequenceCount == 2 ) {
        printf("Condition met: Specific bits are HIGH.\n");
        // Optic 전환
        result = setOpticPort();
    } else if ( sequenceCount == 4 ) {
        result = setDefaultPort();
        printf("Condition met: Specific bits are HIGH.\n");
    }

    // SequenceCount 증가
    sequenceCount++;

    // Parm 길이 설정
    uint16_t parmLength = 1;

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    strncpy(message->OPCode, OPCODE_ENV_ETHERNET, OPCODE_SIZE);

    message->SequenceCount = sequenceCount;
    message->SizeofParm = parmLength;

    // 초기화 결과를 Parm에 저장
    message->Parm[0] = (result == 0) ? '0' : '1';  // 성공: '0', 실패: '1'

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    printf("processingEnvEthernet --\n");
}


// ENV Temperature 처리 함수
void processingEnvTemperature(uint16_t sequenceCount) {
    printf("processingEnvTemperature ++\n");

    // SequenceCount 증가
    sequenceCount++;

    // 현재 온도상태 읽기
    float tempValue = getTempSensorValue();

    // 온도 값을 문자열로 변환 (22.3 형식)
    char tempStr[ENV_TEMP_SIZE];
    snprintf(tempStr, sizeof(tempStr), "%.1f", tempValue); // 소수점 1자리

    // Parm 길이 설정
    uint16_t parmLength = strlen(tempStr); // 변환된 문자열 길이

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    strncpy(message->OPCode, OPCODE_ENV_TEMPURATURE, OPCODE_SIZE);

    message->SequenceCount = sequenceCount;
    message->SizeofParm = parmLength;

    // 온도 상태를 Parm에 저장
    strncpy(message->Parm, tempStr, parmLength);
    message->Parm[parmLength] = '\0'; // 문자열 종료 추가

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    printf("processingEnvTemperature --\n");
}

// ENV NVRAM 처리 함수
void processingEnvNvram(const char Parm[], uint16_t sequenceCount) {
    printf("processingEnvNvram ++\n");
    uint32_t testNvramAddress = 12;

    // 문자열 "0xFFFF"를 uint32_t로 변환
    uint32_t writeValue = (uint32_t)strtoul(Parm, NULL, 16);

    // NVRAM에 값 기록
    uint32_t result = WriteBitResult(testNvramAddress, writeValue);

    if (result == 0) { 
        printf("NVRAM Write Success! Value: 0x%X\n", writeValue);
    } else {
        fprintf(stderr, "Error: Failed to write to NVRAM.\n");
        return;
    }

    // SequenceCount 증가
    sequenceCount++;

    // NVRAM에서 값 읽기
    uint32_t readValue = ReadBitResult(testNvramAddress);

    // readValue를 문자열로 변환
    char nvramStr[ENV_NVRAM_VALUE_SIZE];
    snprintf(nvramStr, sizeof(nvramStr), "0x%X", readValue); // "0x" 포함

    // Parm 길이 설정
    uint16_t parmLength = strlen(nvramStr);

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    strncpy(message->OPCode, OPCODE_ENV_NVRAM, OPCODE_SIZE);

    message->SequenceCount = sequenceCount;
    message->SizeofParm = parmLength;

    // Parm 값 복사
    strncpy(message->Parm, nvramStr, sizeof(message->Parm) - 1);
    message->Parm[sizeof(message->Parm) - 1] = '\0'; // 안전한 종료 추가

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    printf("processingEnvNvram --\n");
}

// ENV SSD처리 함수
void processingEnvSSD(const char Parm[], uint16_t sequenceCount) {
    printf("processingEnvSSD ++\n");

    // 문자열 "DDEF"를 uint32_t로 변환
    uint32_t writeValue = (uint32_t)strtoul(Parm, NULL, 16);

    // SSD에 값 기록
    FILE *file = fopen(SSD_FILE_PATH, "w");
    if (!file) {
        fprintf(stderr, "Error: Failed to open file for writing: %s\n", SSD_FILE_PATH);
        return;
    }
    fprintf(file, "%X", writeValue); // 16진수 형식으로 기록
    fclose(file);

    printf("SSD Write Success! Value: 0x%X\n", writeValue);

    // SequenceCount 증가
    sequenceCount++;

    // SSD에서 값 읽기
    file = fopen(SSD_FILE_PATH, "r");
    if (!file) {
        fprintf(stderr, "Error: Failed to open file for reading: %s\n", SSD_FILE_PATH);
        return;
    }

    char buffer[ENV_SSD_VALUE_SIZE];
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        fprintf(stderr, "Error: Failed to read value from SSD file.\n");
        fclose(file);
        return;
    }
    fclose(file);

    // 읽은 값 처리
    uint32_t readValue = (uint32_t)strtoul(buffer, NULL, 16);

    // readValue를 문자열로 변환
    char ssdStr[ENV_SSD_VALUE_SIZE];
    snprintf(ssdStr, sizeof(ssdStr), "%X", readValue); // "0x" 없이 기록된 값

    // Parm 길이 설정
    uint16_t parmLength = strlen(ssdStr);

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    strncpy(message->OPCode, OPCODE_ENV_SSD, OPCODE_SIZE);

    message->SequenceCount = sequenceCount;
    message->SizeofParm = parmLength;

    // Parm 값 복사
    strncpy(message->Parm, ssdStr, sizeof(message->Parm) - 1);
    message->Parm[sizeof(message->Parm) - 1] = '\0'; // 안전한 종료 추가

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    printf("processingEnvSSD --\n");
}


// ENV PowerMonitor 처리 함수
void processingEnvPowerMonitor(uint16_t sequenceCount) {
    printf("processingEnvPowerMonitor ++\n");

    // SequenceCount 증가
    sequenceCount++;

    // 현재 온도상태 읽기
    float voltValue = getVoltageValue();

    // 전압 값을을 문자열로 변환 (22.3 형식)
    char voltStr[ENV_VOLT_SIZE];
    snprintf(voltStr, sizeof(voltStr), "%.1f", voltValue); // 소수점 1자리

    // Parm 길이 설정
    uint16_t parmLength = strlen(voltStr); // 변환된 문자열 길이

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    strncpy(message->OPCode, OPCODE_ENV_POWER_MONITOR, OPCODE_SIZE);

    message->SequenceCount = sequenceCount;
    message->SizeofParm = parmLength;

    // 온도 상태를 Parm에 저장
    strncpy(message->Parm, voltStr, parmLength);
    message->Parm[parmLength] = '\0'; // 문자열 종료 추가

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    printf("processingEnvPowerMonitore --\n");
}


// TTA BulkData 처리 함수
void processingTTABulkData(uint16_t sequenceCount) {
    printf("processingTTABulkData ++\n");

    if(sequenceCount == 0){
        stopThread = 0;
    }

    // 8070 포트로 수신 소켓 열기
    dataSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (dataSocket < 0) {
        perror("Data socket creation failed");
        return;
    }

    // SO_REUSEADDR 설정
    int opt = 1;
    if (setsockopt(dataSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(dataSocket);
        return;
    }

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(DATA_PORT);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(dataSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Socket bind failed");
        close(dataSocket);
        return;
    }

    if (pthread_create(&dataThreadId, NULL, dataProcessingThread, NULL) != 0) {
        perror("Thread creation failed");
        close(dataSocket);
        return;
    }

    // 시퀀스 카운트 확인
    if (sequenceCount == 3) {
        stopThread = 1;  // 스레드 종료 플래그 설정
        pthread_join(dataThreadId, NULL);  // 스레드 종료 대기
        close(dataSocket);  // 소켓 닫기
        dataSocket = -1;  // 소켓 초기화
        printf("Data processing thread stopped.\n");
        return;
    }

    printf("processingTTABulkData --\n");
}


//TTA Clcok 처리 함수
void processingTTACpuClock(uint16_t sequenceCount) {
    printf("processingTTACpuClock ++\n");

    // SequenceCount 증가
    sequenceCount++;

    // CPU 최대 클럭 읽기
    char cpuClocks[8][16] = {0}; // 각 CPU 클럭 저장
    char combinedClocks[MAX_PARM_SIZE] = {0}; // 모든 CPU 클럭을 결합

    for (int i = 0; i < 8; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);

        FILE *fp = fopen(path, "r");
        if (!fp) {
            perror("Failed to read CPU max frequency");
            return;
        }

        if (fgets(cpuClocks[i], sizeof(cpuClocks[i]), fp) == NULL) {
            perror("Failed to read CPU clock value");
            fclose(fp);
            return;
        }
        fclose(fp);

        // 공백 및 개행 문자 제거
        size_t len = strlen(cpuClocks[i]);
        if (len > 0 && cpuClocks[i][len - 1] == '\n') {
            cpuClocks[i][len - 1] = '\0';
        }

        // 클럭 값을 GHz로 변환하여 결합
        char formattedClock[16];
        snprintf(formattedClock, sizeof(formattedClock), "%.1f", atof(cpuClocks[i]) / 1e6);
        strcat(combinedClocks, formattedClock);
    }

    // Parm 길이 설정
    uint16_t parmLength = strlen(combinedClocks);

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));
    strncpy(message->OPCode, OPCODE_TTA_CPU_CLOCK, OPCODE_SIZE);
    message->SequenceCount = sequenceCount;
    message->SizeofParm = parmLength;
    strncpy(message->Parm, combinedClocks, parmLength);
    message->Parm[parmLength] = '\0'; // 문자열 종료 추가

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    printf("processingTTACpuClock --\n");
}

// TTA BulkData 처리 함수
void processingTTASsdWrite(uint16_t sequenceCount) {
    printf("processingTTASsdWrite ++\n");

    if (sequenceCount == 0) {
        if (ssdStopThread == 1) {  
            ssdStopThread = 0;    // 스레드 시작 플래그 설정
            int ret = pthread_create(&ssdThreadId, NULL, ssdWriteProcessingThread, NULL);
            if (ret != 0) {
                fprintf(stderr, "Thread creation failed with error code: %d\n", ret);
                return;
            }
            printf("ssd Data processing thread started.\n");
        } else {
            printf("Thread is already running.\n");
        }
    } else if (sequenceCount == 3) {
        if (ssdStopThread == 0) {  // 실행 중인 상태라면
            ssdStopThread = 1;     // 스레드 종료 플래그 설정
            pthread_join(ssdThreadId, NULL);  // 스레드 종료 대기
            printf("ssd Data processing thread stopped.\n");
        } else {
            printf("No thread running to stop.\n");
        }
    } else {
        printf("Invalid sequence count received: %d\n", sequenceCount);
    }

    printf("processingTTASsdWrite --\n");
}


// 데이터 수신 및 스루풋 계산 스레드
void *dataProcessingThread(void *arg) {
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    char buffer[MTU_SIZE];
    uint32_t totalBytes = 0;
    time_t startTime = time(NULL);
    uint16_t throughputCount = 2;
    int isFirstSecond = 1; // 첫 1초 여부 플래그

    // 파일 열기
    FILE *file = fopen("/mnt/dataSSD/received_data.bin", "wb");
    if (!file) {
        perror("Failed to open file for writing");
        return NULL;
    }

    while (!stopThread) {
        // 데이터 수신
        ssize_t bytesReceived = recvfrom(dataSocket, buffer, MTU_SIZE, 0, (struct sockaddr *)&clientAddr, &addrLen);
        if (bytesReceived > 0) {
            totalBytes += bytesReceived; // 모든 데이터를 카운트

            // SSD에 데이터 저장
            if (fwrite(buffer, 1, bytesReceived, file) != bytesReceived) {
                perror("Failed to write data to file");
                break;
            }
        } else if (bytesReceived < 0) {
            perror("recvfrom");
        }

        // 1초마다 스루풋 계산 및 전송
        time_t currentTime = time(NULL);
        if (currentTime - startTime >= 1) {
            uint32_t throughputMbps = (totalBytes * 8.0) / 1e6; // Mbps 계산
            printf("Throughput: %u Mbps\n", throughputMbps);
            totalBytes = 0; // 바이트 카운터 초기화

            if (isFirstSecond) {
                // 첫 1초는 스루풋 송신 생략
                printf("First second throughput ignored\n");
                isFirstSecond = 0; // 첫 1초 플래그 해제
            } else {
                // 메시지 생성
                Message *message = (Message *)malloc(sizeof(Message));
                if (!message) {
                    fprintf(stderr, "Error: Memory allocation failed.\n");
                    break;
                }

                memset(message, 0, sizeof(Message));
                strncpy(message->OPCode, OPCODE_TTA_BULK_DATA, OPCODE_SIZE);
                message->SequenceCount = throughputCount;
                snprintf(message->Parm, MAX_PARM_SIZE, "%u", throughputMbps);
                message->SizeofParm = strlen(message->Parm);

                // 메시지 브로드캐스트 송신
                broadcastSendMessage(message);
                free(message);

                throughputCount++;
            }

            startTime = currentTime; // 1초 주기 리셋
        }
    }

    // 파일 닫기
    fclose(file);
    return NULL;
}

void *ssdWriteProcessingThread(void *arg) {
    printf("ssdWriteProcessingThread start ++");
    uint32_t totalBytes = 0;
    time_t startTime = time(NULL);
    uint16_t throughputCount = 2;
    int isFirstSecond = 1; // 첫 1초 여부 플래그
    off_t prevFileSize = 0; // 이전 파일 크기
    char *writeBuffer = (char *)malloc(WRITE_BUFFER_SIZE);

    if (!writeBuffer) {
    perror("Failed to allocate writeBuffer");
    return NULL;
    }

    memset(writeBuffer, 'A', WRITE_BUFFER_SIZE);

    // 파일 열기
    FILE *file = fopen("/mnt/dataSSD/test_data.bin", "wb");
    if (!file) {
        perror("Failed to open file for writing");
        return NULL;
    }

    while (!ssdStopThread) {
        // SSD에 데이터 쓰기
        if (fwrite(writeBuffer, 1, WRITE_BUFFER_SIZE, file) != WRITE_BUFFER_SIZE) {
            perror("Failed to write data to file");
            break;
        }

        totalBytes += WRITE_BUFFER_SIZE; // 모든 데이터를 카운트

        // 1초마다 쓰기 속도 계산 및 출력/송신
        time_t currentTime = time(NULL);
        if (currentTime - startTime >= 1) {
            // 현재 파일 크기 가져오기
            struct stat fileStat;
            if (stat("/mnt/dataSSD/test_data.bin", &fileStat) == 0) {
                off_t currentFileSize = fileStat.st_size;

                // 1초 동안 쓰기 속도 계산 (MB/s)
                uint32_t writeSpeedMBps = (currentFileSize - prevFileSize) / (1024.0 * 1024.0);

                // 출력
                printf("Write Speed: %d MB/s\n", writeSpeedMBps);

                // 브로드캐스트 송신
                if (!isFirstSecond) {
                    // 메시지 생성
                    Message *message = (Message *)malloc(sizeof(Message));
                    if (!message) {
                        fprintf(stderr, "Error: Memory allocation failed.\n");
                        break;
                    }

                    memset(message, 0, sizeof(Message));
                    strncpy(message->OPCode, OPCODE_TTA_SSD_WRITE, OPCODE_SIZE);
                    message->SequenceCount = throughputCount;
                    snprintf(message->Parm, MAX_PARM_SIZE, "%d", writeSpeedMBps);
                    message->SizeofParm = strlen(message->Parm);

                    // 메시지 브로드캐스트 송신
                    broadcastSendMessage(message);
                    free(message);

                    throughputCount++;
                } else {
                    // 첫 1초는 스루풋 송신 생략
                    printf("First second write speed ignored\n");
                    isFirstSecond = 0;
                }

                // 파일 크기 업데이트
                prevFileSize = currentFileSize;
            } else {
                perror("Failed to get file size");
            }

            startTime = currentTime; // 1초 주기 리셋
        }
    }

    // 파일 닫기
    fclose(file);
    free(writeBuffer);
    printf("ssdWriteProcessingThread stop --");
    return NULL;
}



// Discrete IN 처리 함수
void processingDiscreteIn(const char Parm[], uint16_t sequenceCount) {
    printf("processingDiscreteIn ++\n");

    // SequenceCount 증가
    sequenceCount++;

    // 현재 상태 읽기
    uint32_t value = ReadBootModeStatus();

    // Parm 길이 설정 (2 바이트)
    uint16_t parmLength = 2;

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    // 메시지 필드 설정
    strncpy(message->OPCode, OPCODE_DISCRETE_IN, OPCODE_SIZE);
    message->SequenceCount = sequenceCount;
    message->SizeofParm = parmLength;

    // Parm 설정
    if (value == 0x01) {
        memcpy(message->Parm, "01", parmLength); // SIL 모드
    } else {
        memcpy(message->Parm, "00", parmLength); // NORMAL 모드
    }
    message->Parm[parmLength] = '\0'; // 널 문자 추가

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);
    printf("processingDiscreteIn --\n");
}

// Discrete OUT 처리 함수
void processingDiscreteOut(const char Parm[], uint16_t sequenceCount) {
    printf("processingDiscreteOut ++\n");
    uint16_t discreteOutValue;

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    sequenceCount ++;

    // (2) 2~11 번째 메시지 (ParmLength = 24, mm/ss/mmmm + discrete 16개 값 전송)
    for (int i = 0; i < 10; i++) {
        memset(message, 0, sizeof(Message));
        strncpy(message->OPCode, OPCODE_DISCRETE_OUT, OPCODE_SIZE);
        message->SequenceCount = sequenceCount + i;
        message->SizeofParm = 24;

        // 현재 상태 읽기 (각 메시지 전송 전에 항상 최신값으로 업데이트)
        discreteOutValue = readDiscreteIn();

        // 현재 시간 가져오기 (mm:ss:mmmm)
        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct tm *tm_info = localtime(&tv.tv_sec);
        snprintf(message->Parm, sizeof(message->Parm), "%02d%02d%04d", tm_info->tm_min, tm_info->tm_sec, (int)(tv.tv_usec / 1000)); // 밀리초까지 포함

        // discrete 16개 값을 2진 문자열로 변환 후 추가
        char discreteString[17]; // 16비트 + 널문자
        uint16_to_binary_string(discreteOutValue, discreteString, sizeof(discreteString));
        strncat(message->Parm, discreteString, sizeof(message->Parm) - strlen(message->Parm) - 1);

        // 메시지 브로드캐스트 전송
        broadcastSendMessage(message);
    }

    // (3) 12번째 메시지 (ParmLength = 2, 모드 전송)
    memset(message, 0, sizeof(Message));
    strncpy(message->OPCode, OPCODE_DISCRETE_OUT, OPCODE_SIZE);
    message->SequenceCount = sequenceCount + 10;
    message->SizeofParm = 2;

    // 현재 상태 읽기 (최신 값)
    discreteOutValue = readDiscreteIn();

    // discrete 5, 6번 비트 확인하여 00이면 일반 모드, 01이면 SIL 모드
    uint8_t bit5 = (discreteOutValue >> 5) & 1;
    uint8_t bit6 = (discreteOutValue >> 6) & 1;
    if (bit5 == 0 && bit6 == 0) {
        strncpy(message->Parm, "00", sizeof(message->Parm));
    } else if (bit5 == 1 && bit6 == 0) {
        strncpy(message->Parm, "01", sizeof(message->Parm));
    } else {
        strncpy(message->Parm, "XX", sizeof(message->Parm)); // 예외 처리
    }

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);
    printf("processingDiscreteOut --\n");
}



// SSD, NVRAM 초기화 처리 함수
void processingInitSSDNvram(const char Parm[], uint16_t sequenceCount) {
    printf("processingInitSSDNvram ++\n");

    // SequenceCount 증가
    sequenceCount++;

    // SSD 초기화 상태 읽기
    uint8_t initSSD = initializeDataSSD();

    // NVRAM 초기화 상태 읽기
    uint8_t initNVRAM = InitializeNVRAMToZero();

    // Parm 길이 설정
    uint16_t parmLength = 2;

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    // 메시지 필드 설정
    strncpy(message->OPCode, OPCODE_INIT_NVRAM_SSD, OPCODE_SIZE);

    message->SequenceCount = sequenceCount;
    message->SizeofParm = parmLength;

    // 초기화 결과를 Parm에 저장
    message->Parm[0] = (initSSD == 0) ? '1' : '0';  // SSD 초기화 결과
    message->Parm[1] = (initNVRAM == 0) ? '1' : '0'; // NVRAM 초기화 결과
    message->Parm[2] = '\0'; // Null-terminate 추가 (안전)

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);
     printf("processingInitSSDNvram --\n");
}

// 이더넷 구리,광 변경  처리 함수
void processingEthernetSetting(const char Parm[], uint16_t sequenceCount, uint8_t type) {
    printf("processingEthernetSetting ++\n");

    // SequenceCount 증가
    sequenceCount++;

    // 초기화 결과 변수
    uint8_t result = 0;

    uint16_t discreteValue = readDiscreteIn();
    int isHigh12 = discreteValue & (1 << 12);
    int isHigh13 = discreteValue & (1 << 13);
    int isHigh14 = discreteValue & (1 << 14);
    int isHigh15 = discreteValue & (1 << 15);

    if (type == 1) {
        // Default 포트 설정
        if ((isHigh13 && isHigh14 && isHigh15 && isHigh12 ==0) || (isHigh14 && isHigh15 && isHigh13 ==0 && isHigh12 ==0)) {
            result = setDefaultPort();
            printf("Condition met: Specific bits are HIGH.\n");
        }
    } else if (type == 2) {
        if (isHigh12 && (isHigh13 == 0 && isHigh14 == 0 && isHigh15 ==0)) {
            printf("Condition met: Specific bits are HIGH.\n");
            // Optic 전환
            result = setOpticPort();
        }

    } else {
        printf("Invalid type: %u\n", type);
        result = 1; // 잘못된 타입은 실패로 간주
    }

    // Parm 길이 설정
    uint16_t parmLength = 1;

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    // 메시지 필드 설정
    strncpy(message->OPCode, OPCODE_ETHERNET_COPPER_OPTIC, OPCODE_SIZE);

    message->SequenceCount = sequenceCount;
    message->SizeofParm = parmLength;

    // 초기화 결과를 Parm에 저장
    message->Parm[0] = (result == 0) ? '0' : '1';  // 성공: '0', 실패: '1'

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    printf("processingEthernetSetting --\n");
}

// 이더넷 구리,광 변경  처리 함수
void processingEthernetEnable(uint16_t sequenceCount) {
    printf("processingEthernetEnable ++\n");

    // 설정 결과 변수
    uint8_t result = 0; // 기본값은 실패(1)로 설정
    uint8_t portNumber = 0;
    uint16_t sendSequence = 0;

    sendSequence = sequenceCount;

    // SequenceCount 증가
    sendSequence++;

    // Parm 길이 설정
    uint16_t parmLength = 1;

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    // 메시지 필드 설정
    strncpy(message->OPCode, OPCODE_ETHERNET_ENABLE_DISABLE, OPCODE_SIZE);
    message->SequenceCount = sendSequence;
    message->SizeofParm = parmLength;
    
    // 초기화 결과를 Parm에 저장
    message->Parm[0] = (result == 0) ? '0' : '1';  // 성공: '0', 실패: '1'

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    // SequenceCount에 따라 포트 번호 및 동작 결정
    switch (sequenceCount) {
        case 2: portNumber = 2; result = setEthernetPort(portNumber, 0); break; // Disable
        case 4: portNumber = 2; result = setEthernetPort(portNumber, 1); break; // Enable
        case 6: portNumber = 3; result = setEthernetPort(portNumber, 0); break;
        case 8: portNumber = 3; result = setEthernetPort(portNumber, 1); break;
        case 10: portNumber = 4; result = setEthernetPort(portNumber, 0); break;
        case 12: portNumber = 4; result = setEthernetPort(portNumber, 1); break;
        case 14: portNumber = 5; result = setEthernetPort(portNumber, 0); break;
        case 16: portNumber = 5; result = setEthernetPort(portNumber, 1); break;
        case 18: portNumber = 6; result = setEthernetPort(portNumber, 0); break;
        case 20: portNumber = 6; result = setEthernetPort(portNumber, 1); break;
        case 22: portNumber = 7; result = setEthernetPort(portNumber, 0); break;
        case 24: portNumber = 7; result = setEthernetPort(portNumber, 1); break;
        case 26: result = setOpticPort(); break; // Copper -> Fiber 변환
        case 28: portNumber = 2; result = setEthernetPort(portNumber, 0); break; // Fiber Port 1 Disable
        case 30: portNumber = 2; result = setEthernetPort(portNumber, 1); break; // Fiber Port 1 Enable
        case 32: portNumber = 3; result = setEthernetPort(portNumber, 0); break; // Fiber Port 2 Disable
        case 34: portNumber = 3; result = setEthernetPort(portNumber, 1); break; // Fiber Port 2 Enable
        case 36: portNumber = 4; result = setEthernetPort(portNumber, 0); break; // Fiber Port 3 Disable
        case 38: portNumber = 4; result = setEthernetPort(portNumber, 1); break; // Fiber Port 3 Enable
        case 40: portNumber = 5; result = setEthernetPort(portNumber, 0); break; // Fiber Port 4 Disable
        case 42: portNumber = 5; result = setEthernetPort(portNumber, 1); break; // Fiber Port 4 Enable
        case 44: portNumber = 6; result = setEthernetPort(portNumber, 0); break; // Fiber Port 5 Disable
        case 46: portNumber = 6; result = setEthernetPort(portNumber, 1); break; // Fiber Port 5 Enable
        case 48: result = setDefaultPort(); break; // Fiber -> Copper 변환

        default: 
            printf("Invalid sequence count: %d\n", sequenceCount);
            return;
    }

    printf("processingEthernetEnable --\n");
}

void processingGetHwInfo(uint16_t sequenceCount) {
    printf("processingGetHwInfo ++\n");
    struct hwCompatInfo myInfo;

    // NVRAM에서 데이터 읽기
    uint32_t result = ReadHwCompatInfoFromNVRAM(&myInfo);
    if (result != 0) {
        fprintf(stderr, "NVRAM read failed with error code: %u\n", result);
        return;
    }

    printf("NVRAM read successful.\n");

    // 송신할 데이터 목록
    char *dataFields[] = {
        myInfo.supplier_part_no,
        myInfo.supplier_serial_no,
        myInfo.sw_part_number,
        myInfo.ssd0_model_no,
        myInfo.ssd1_model_no,
        myInfo.ssd0_serial_no,
        myInfo.ssd1_serial_no
    };

    size_t dataCount = sizeof(dataFields) / sizeof(dataFields[0]);

    // 각 데이터를 순차적으로 전송
    for (size_t i = 0; i < dataCount; i++) {
        // 메시지 동적 할당
        Message *message = (Message *)malloc(sizeof(Message));
        if (!message) {
            fprintf(stderr, "Error: Memory allocation failed.\n");
            return;
        }

        // 메시지 필드 초기화
        memset(message, 0, sizeof(Message));

        // OPCode 설정
        strncpy(message->OPCode, OPCODE_GET_HW_INFO, OPCODE_SIZE);

        // SequenceCount 설정
        message->SequenceCount = sequenceCount++;

        // SizeofParm 설정 (문자열 크기)
        message->SizeofParm = strlen(dataFields[i]);

        // Parm 설정 (문자열 데이터 복사)
        strncpy(message->Parm, dataFields[i], MAX_PARM_SIZE);

        // 메시지 브로드캐스트 전송
        broadcastSendMessage(message);

        // 메모리 해제
        free(message);
    }

    //결과 송신

    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    // OPCode 설정
    strncpy(message->OPCode, OPCODE_GET_HW_INFO, OPCODE_SIZE);

    // SequenceCount 설정
    message->SequenceCount = sequenceCount++;

    // Hardware 정보 일치 여부 확인
    uint8_t resultOfCheck = 0;
    resultOfCheck = CheckHwCompatInfo();

    // 결과를 문자열로 변환하여 저장
    snprintf(message->Parm, MAX_PARM_SIZE, "%u", resultOfCheck);
    message->SizeofParm = strlen(message->Parm);  // SizeofParm을 정확히 설정

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);


    printf("processingGetHwInfo --\n");
}

// Hold-up Module 체크
void processingHoldupCheck(uint16_t sequenceCount) {
    printf("processingHoldupCheck ++\n");

    if (sequenceCount == 1) {
        struct hwCompatInfo myInfo;

        // Holdup 상태 읽기 (JSON 파일에서)
        int X = 0; // 기본값 (전원 정상, 캐패시터 방전)
        FILE *file = fopen(HOLDUP_LOG_FILE_PATH, "r");
        if (file) {
            char line[256];
            char latestJson[256] = {0}; // 가장 최신 JSON 저장
            while (fgets(line, sizeof(line), file)) {
                strcpy(latestJson, line); // 마지막 줄을 latestJson에 저장
            }
            fclose(file);

            if (strlen(latestJson) > 0) {
                cJSON *root = cJSON_Parse(latestJson);
                if (root) {
                    cJSON *holdupState = cJSON_GetObjectItem(root, "holdup_state");
                    if (holdupState && cJSON_IsNumber(holdupState)) {
                        X = holdupState->valueint;
                    }
                    cJSON_Delete(root);
                }
            } else {
                printf("[ERROR] No valid JSON found in file.\n");
            }
        } else {
            printf("Warning: Could not open HoldupLog.json. Using default state.\n");
        }

        printf("HoldupModule state read: %d\n", X);

        // SequenceCount 증가
        sequenceCount += 2;

        // Parm 길이 설정
        uint16_t parmLength = 16;

        // 메시지 동적 할당
        Message *message = (Message *)malloc(sizeof(Message));
        if (!message) {
            fprintf(stderr, "Error: Memory allocation failed.\n");
            return;
        }

        // 메시지 필드 초기화
        memset(message, 0, sizeof(Message));

        // 메시지 필드 설정
        strncpy(message->OPCode, OPCODE_HOLDUP_CHECK, OPCODE_SIZE);

        message->SequenceCount = sequenceCount;
        message->SizeofParm = parmLength;

        // 현재 시간 가져오기
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);

        // Parm 값 설정 (X_YYYYMMDDHHMMSS)
        snprintf(message->Parm, parmLength, "%d_%04d%02d%02d%02d%02d%02d",
                 X, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec);

        printf("Generated Parm: %s\n", message->Parm);

        // 메시지 브로드캐스트 전송
        broadcastSendMessage(message);

        // 메모리 해제
        free(message);

        updateConfigFile(OPCODE_HOLDUP_CHECK, 0);

    } else {
        ensureConfigDirectoryExists();
        updateConfigFile(OPCODE_HOLDUP_CHECK, 1);
    }

    printf("processingHoldupCheck --\n");
}


// USB,RS232 설정 처리 함수
void processingInitUsbRs232EnDisable(const char Parm[], uint16_t sequenceCount, uint8_t act) {
    printf("processingInitUsbRs232EnDisable ++\n");
    uint8_t resultOfRs232 = 0;
    uint8_t resultOfUSB = 0;

    if (act != 1 && act != 0) {
    fprintf(stderr, "Invalid action: %u\n", act);
    return;
    }

    // SequenceCount 증가
    sequenceCount++;

    if( act == 1 ) {
        printf("ActivateRS232 ++\n");
        resultOfRs232 = ActivateRS232();
        printf("ActivateUSB ++\n");
        resultOfUSB = ActivateUSB();
        
    } else {
        printf("DeactivateRS232 ++\n");
        resultOfRs232 = DeactivateRS232();
        printf("DeactivateUSB ++\n");
        resultOfUSB = DeactivateUSB();
    }

    // Parm 길이 설정
    uint16_t parmLength = 1;

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    // 메시지 필드 설정
    strncpy(message->OPCode, OPCODE_RS232_USB_EN_DISABLE, OPCODE_SIZE);

    message->SequenceCount = sequenceCount;
    message->SizeofParm = parmLength;

    // 결과를 Parm에 저장
    if (resultOfRs232 == 0 && resultOfUSB == 0) {
        message->Parm[0] = '0'; // 성공
    } else {
        message->Parm[0] = '1'; // 실패
    }

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);
}


void processingBootCondition(const char Parm[], uint16_t sequenceCount) {
    printf("processingBootCondition ++\n");

        // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        perror("Failed to allocate memory for message");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    // Boot Condition 값 읽기
    uint8_t readBootConditionValue = ReadBootCondition();
    uint8_t sincValue = readBootConditionValue -2;

    if ( readBootConditionValue == 5 ){
        sincValue = 2;
    }

    // Parm 설정 (Boot Condition 값 사용)
    message->Parm[0] = '0' + sincValue;  // 정수를 ASCII로 변환
    message->Parm[1] = '\0';  // Null-terminate

    // Parm 길이 설정 (항상 1)
    uint16_t parmLength = 1;

    // OPCode 설정
    strncpy(message->OPCode, OPCODE_BOOT_CONDITION, OPCODE_SIZE);
    message->OPCode[OPCODE_SIZE] = '\0';  // Null-terminate

    // SequenceCount 증가 및 설정
    sequenceCount ++;
    message->SequenceCount = sequenceCount;

    // SizeofParm 설정
    message->SizeofParm = parmLength;

    // 메시지 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    printf("processingBootCondition --\n");
}

void processingPushbutton(uint16_t sequenceCount) {
    printf("processingPushbutton ++\n");

    if( sequenceCount == 0 ) {
        ensureConfigDirectoryExists();
        updateConfigFile(OPCODE_PUSH_BUTTON_CHECK, 1);
    } else if ( sequenceCount == 1 ) {
        // 메시지 동적 할당
        Message *message = (Message *)malloc(sizeof(Message));
        if (!message) {
            perror("Failed to allocate memory for message");
            return;
        }

        // 메시지 필드 초기화
        memset(message, 0, sizeof(Message));

        // Boot Condition 값 읽기
        uint8_t readBootConditionValue = ReadBootCondition();
        uint8_t sincValue = readBootConditionValue -2;

        printf( " readBootConditionValue : %d ", readBootConditionValue);

        if ( readBootConditionValue == 5 ){
            printf( " readBootConditionValue is 5");
            sincValue = 2;
        }

        // Parm 설정 (Boot Condition 값 사용)
        message->Parm[0] = '0' + sincValue;  // 정수를 ASCII로 변환
        message->Parm[1] = '\0';  // Null-terminate

        // Parm 길이 설정 (항상 1)
        uint16_t parmLength = 1;

        // OPCode 설정
        strncpy(message->OPCode, OPCODE_PUSH_BUTTON_CHECK, OPCODE_SIZE);
        message->OPCode[OPCODE_SIZE] = '\0';  // Null-terminate

        // SequenceCount 증가 및 설정
        sequenceCount ++;
        message->SequenceCount = sequenceCount;

        // SizeofParm 설정
        message->SizeofParm = parmLength;

        // 메시지 전송
        broadcastSendMessage(message);

        // 메모리 해제
        free(message);

        ensureConfigDirectoryExists();
        updateConfigFile(OPCODE_PUSH_BUTTON_CHECK, 0);
    }
    printf("processingPushbutton --\n");
}

void processingResetReson(uint16_t sequenceCount) {
    printf("processingResetReson ++\n");

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        perror("Failed to allocate memory for message");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    // Boot Condition 값 읽기
    uint8_t readBootConditionValue = ReadBootCondition();
    uint8_t sincValue = readBootConditionValue -2;

    printf( " readBootConditionValue : %d ", readBootConditionValue);

    if ( readBootConditionValue == 5 ){
        printf( " readBootConditionValue is 5");
        sincValue = 2;
    }

    // Parm 설정 (Boot Condition 값 사용)
    message->Parm[0] = '0' + sincValue;  // 정수를 ASCII로 변환
    message->Parm[1] = '\0';  // Null-terminate

    // Parm 길이 설정 (항상 1)
    uint16_t parmLength = 1;

    // OPCode 설정
    strncpy(message->OPCode, OPCODE_RESET_REASON, OPCODE_SIZE);
    message->OPCode[OPCODE_SIZE] = '\0';  // Null-terminate

    // SequenceCount 증가 및 설정
    sequenceCount ++;
    message->SequenceCount = sequenceCount;

    // SizeofParm 설정
    message->SizeofParm = parmLength;

    // 메시지 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);
    
    printf("processingResetReson --\n");
}


// SYSTEM Count 처리 함수
void processingSystemCount(uint16_t sequenceCount) {
    printf("processingSystemCount ++\n");

    // NVRAM에서 로그 읽기
    char readPowerOnCount[32];
    char readWatchdogRebootCount[32];
    char readButtonResetCount[32];
    char readCumalativeTime[32];

    snprintf(readPowerOnCount, sizeof(readPowerOnCount), "%u", ReadSystemLog(2));
    snprintf(readWatchdogRebootCount, sizeof(readWatchdogRebootCount), "%u", ReadSystemLog(6));
    snprintf(readButtonResetCount, sizeof(readButtonResetCount), "%u", ReadSystemLog(7));
    snprintf(readCumalativeTime, sizeof(readButtonResetCount), "%u", ReadCumulativeTime());

    // Message 구조체 선언
    Message message;

    // Power On Count 전송
    memset(&message, 0, sizeof(Message));
    sequenceCount++;
    strncpy(message.OPCode, OPCODE_SYSTEM_COUNT, OPCODE_SIZE);
    message.SequenceCount = sequenceCount;
    message.SizeofParm = strlen(readPowerOnCount);
    strncpy(message.Parm, readPowerOnCount, message.SizeofParm);
    message.Parm[message.SizeofParm] = '\0';
    broadcastSendMessage(&message);

    // Watchdog Reboot Count 전송
    memset(&message, 0, sizeof(Message));
    sequenceCount++;
    strncpy(message.OPCode, OPCODE_SYSTEM_COUNT, OPCODE_SIZE);
    message.SequenceCount = sequenceCount;
    message.SizeofParm = strlen(readWatchdogRebootCount);
    strncpy(message.Parm, readWatchdogRebootCount, message.SizeofParm);
    message.Parm[message.SizeofParm] = '\0';
    broadcastSendMessage(&message);

    // Button Reset Count 전송
    memset(&message, 0, sizeof(Message));
    sequenceCount++;
    strncpy(message.OPCode, OPCODE_SYSTEM_COUNT, OPCODE_SIZE);
    message.SequenceCount = sequenceCount;
    message.SizeofParm = strlen(readButtonResetCount);
    strncpy(message.Parm, readButtonResetCount, message.SizeofParm);
    message.Parm[message.SizeofParm] = '\0';
    broadcastSendMessage(&message);

    // 누적운용시간 전송
    memset(&message, 0, sizeof(Message));
    sequenceCount++;
    strncpy(message.OPCode, OPCODE_SYSTEM_COUNT, OPCODE_SIZE);
    message.SequenceCount = sequenceCount;
    message.SizeofParm = strlen(readCumalativeTime);
    strncpy(message.Parm, readCumalativeTime, message.SizeofParm);
    message.Parm[message.SizeofParm] = '\0';
    broadcastSendMessage(&message);


    printf("processingSystemCount --\n");
}

//BIT TEST 수행
void processingBitResult(const char Parm[], uint16_t sequenceCount, uint32_t requestBit) {
    printf("processingBitResult ++\n");

    // 요청된 BIT 수행
    RequestBit(requestBit);

    // BIT 결과값 읽기
    uint32_t readBitValue = ReadBitResult(requestBit);

    // BIT 항목 리스트 (테스트 항목 이름)
    const char *bitTestItems[] = {
        "GPU module",
        "SSD (Data store)",
        "GPIO Expander",
        "Ethernet MAC/PHY",
        "NVRAM",
        "Discrete Input",
        "Discrete Output",
        "RS232 Transceiver",
        "SSD (Boot OS)",
        "10G Ethernet Switch",
        "Optic Transceiver",
        "Temperature Sensor",
        "Power Monitor",
        "Holdup Module",
        "STM32 Status",
        "Lan 7800 Status",
        "AC/DC Status",
        "DC/DC Status",
        "USB A Status",
        "USB C Status"
    };

    // 항목 개수
    const size_t bitCount = sizeof(bitTestItems) / sizeof(bitTestItems[0]);

    // BIT 상태를 담을 버퍼 (2자리씩 20항목 => 40바이트)
    char bitResultString[bitCount * 2 + 1]; // 40 + NULL
    memset(bitResultString, '0', sizeof(bitResultString));
    bitResultString[bitCount * 2] = '\0'; // Null-terminate

    // BIT 결과값 각 항목별 처리
    for (size_t i = 0; i < bitCount; i++) {
        // 각 항목의 비트 상태 확인 (0: 정상, 1: 비정상)
        uint8_t bitStatus = (readBitValue >> i) & 0x01;

        // 2자리 설정 (정상: "00", 비정상: "01")
        if (bitStatus == 0) {
            strncpy(&bitResultString[i * 2], "00", 2);
        } else {
            strncpy(&bitResultString[i * 2], "01", 2);
        }
    }

    // 데이터 역순 변환
    char reversedBitResultString[bitCount * 2 + 1];
    for (size_t i = 0; i < bitCount * 2; i++) {
        reversedBitResultString[i] = bitResultString[bitCount * 2 - 1 - i];
    }
    reversedBitResultString[bitCount * 2] = '\0'; // Null-terminate

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        perror("Failed to allocate memory for message");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    // OPCode 설정 (요청된 BIT에 따라 결정)
    if (requestBit == 2) {
        strncpy(message->OPCode, OPCODE_BIT_TEST_PBIT, OPCODE_SIZE);
    } else if (requestBit == 3) {
        strncpy(message->OPCode, OPCODE_BIT_TEST_CBIT, OPCODE_SIZE);
    } else if (requestBit == 4) {
        strncpy(message->OPCode, OPCODE_BIT_TEST_IBIT, OPCODE_SIZE);
    }
    message->OPCode[OPCODE_SIZE] = '\0';

    // SequenceCount 증가 및 설정
    sequenceCount++;
    message->SequenceCount = sequenceCount;

    // Parm 설정 (역순으로 변환된 비트 결과 문자열 복사)
    strncpy(message->Parm, reversedBitResultString, sizeof(reversedBitResultString));
    message->Parm[sizeof(reversedBitResultString) - 1] = '\0'; // Null-terminate

    // SizeofParm 설정
    message->SizeofParm = strlen(message->Parm);

    // 메시지 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    printf("processingBitResult --\n");
}

//CBIT TEST 수행
void processingCBitResult(const char Parm[], uint16_t sequenceCount, uint32_t requestBit) {
    printf("processingCBitResult ++\n");

    // 요청된 BIT 수행
    RequestCBIT(requestBit);

    // BIT 결과값 읽기
    uint32_t readBitValue = ReadBitResult(requestBit);

    // BIT 항목 리스트 (테스트 항목 이름)
    const char *bitTestItems[] = {
        "GPIO Expander",
        "Ethernet PHY",
        "NVRAM",
        "Discrete Input",
        "Discrete Output",
        "RS232 Transceiver",
        "Ethernet Switch",
        "Temperature Sensor",
        "Power Monitor",
        "Holdup Module",
        "STM32 Status"
    };

    // 항목 개수
    const size_t bitCount = sizeof(bitTestItems) / sizeof(bitTestItems[0]);

    // BIT 상태를 담을 버퍼 (2자리씩 11항목 => 22바이트)
    char bitResultString[bitCount * 2 + 1]; // 22 + NULL
    memset(bitResultString, '0', sizeof(bitResultString));
    bitResultString[bitCount * 2] = '\0'; // Null-terminate

    // BIT 결과값 각 항목별 처리
    for (size_t i = 0; i < bitCount; i++) {
        // 각 항목의 비트 상태 확인 (0: 정상, 1: 비정상)
        uint8_t bitStatus = (readBitValue >> i) & 0x01;

        // 2자리 설정 (정상: "00", 비정상: "01")
        if (bitStatus == 0) {
            strncpy(&bitResultString[i * 2], "00", 2);
        } else {
            strncpy(&bitResultString[i * 2], "01", 2);
        }
    }

    // 데이터 역순 변환
    char reversedBitResultString[bitCount * 2 + 1];
    for (size_t i = 0; i < bitCount * 2; i++) {
        reversedBitResultString[i] = bitResultString[bitCount * 2 - 1 - i];
    }
    reversedBitResultString[bitCount * 2] = '\0'; // Null-terminate

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        perror("Failed to allocate memory for message");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    // OPCode 설정 (요청된 BIT에 따라 결정)
    if (requestBit == 2) {
        strncpy(message->OPCode, OPCODE_BIT_TEST_PBIT, OPCODE_SIZE);
    } else if (requestBit == 3) {
        strncpy(message->OPCode, OPCODE_BIT_TEST_CBIT, OPCODE_SIZE);
    } else if (requestBit == 4) {
        strncpy(message->OPCode, OPCODE_BIT_TEST_IBIT, OPCODE_SIZE);
    }
    message->OPCode[OPCODE_SIZE] = '\0';

    // SequenceCount 증가 및 설정
    sequenceCount++;
    message->SequenceCount = sequenceCount;

    // Parm 설정 (역순으로 변환된 비트 결과 문자열 복사)
    strncpy(message->Parm, reversedBitResultString, sizeof(reversedBitResultString));
    message->Parm[sizeof(reversedBitResultString) - 1] = '\0'; // Null-terminate

    // SizeofParm 설정
    message->SizeofParm = strlen(message->Parm);

    // 메시지 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    printf("processingCBitResult --\n");
}

void processingBitSave(const char Parm[], uint16_t sequenceCount) {
    printf("processingBitSave ++\n");

    const char logFilePath[] = "/mnt/dataSSD/BitErrorLog.json";

    // 파일 존재 여부 확인
    int fileExists = access(logFilePath, F_OK) == 0 ? 0 : 1;

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        perror("Failed to allocate memory for message");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    // OPCode 설정
    strncpy(message->OPCode, OPCODE_BIT_TEST_RESULT_SAVE, OPCODE_SIZE);
    message->OPCode[OPCODE_SIZE] = '\0';

    // SequenceCount 증가 및 설정
    sequenceCount++;
    message->SequenceCount = sequenceCount;

    // Parm 설정 (파일 존재 여부를 ASCII로 변환하여 설정: 0 또는 1)
    message->Parm[0] = '0' + fileExists;  // '0' (성공) 또는 '1' (실패)
    message->Parm[1] = '\0';

    // SizeofParm 설정
    message->SizeofParm = 1;

    // 메시지 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    printf("processingBitSave --\n");
}

void processingSSDBite(const char Parm[], uint16_t sequenceCount, uint8_t ssd_type) {
    printf("processingSSDBite ++\n");

    SSD_Status logValue = getSSDSmartLog(ssd_type);

    // 각 항목의 값을 배열로 정의
    const uint32_t values[] = {
        logValue.temperature,        // Temperature
        logValue.available_spare,    // Available Spare
        logValue.available_spare_threshold,
        logValue.percentage_used,
        logValue.data_units_read,
        logValue.data_units_written,
        logValue.host_read_commands,
        logValue.host_write_commands,
        logValue.power_cycles,
        logValue.power_on_hours,
        logValue.unsafe_shutdowns,
        logValue.media_and_data_errors,
        logValue.error_log_entries,
        logValue.critical_temp_time,
        logValue.controller_busy_time
    };

    // 총 항목 수 계산
    size_t totalEntries = sizeof(values) / sizeof(values[0]);

    for (size_t i = 0; i < totalEntries; i++) {

        // 메시지 동적 할당
        Message *message = (Message *)malloc(sizeof(Message));
        if (!message) {
            perror("Failed to allocate memory for message");
            return;
        }

        // 메시지 필드 초기화
        memset(message, 0, sizeof(Message));

        // OPCode 설정
        strncpy(message->OPCode, OPCODE_OS_SSD_BITE_TEST, OPCODE_SIZE);

        // SequenceCount 설정
        sequenceCount++;
        message->SequenceCount = sequenceCount;

        uint16_t parmLength = 0;

        // 첫 번째 메시지에서는 Parm을 보내지 않음
        parmLength = 0;
        message->Parm[0] = '\0';

        // Parm 값 설정 (현재 항목의 값만 포함)
        int ret = snprintf(message->Parm, MAX_PARM_SIZE, "%u", values[i]);
        if (ret < 0 || ret >= MAX_PARM_SIZE) {
            fprintf(stderr, "Warning: Parm value truncated for entry %zu\n", i);
            message->Parm[0] = '\0';
        } else {
            parmLength = strlen(message->Parm);
        }
        
        // SizeofParm 설정
        message->SizeofParm = parmLength;

        // 메시지 전송
        broadcastSendMessage(message);
    }

    printf("processingSSDBite --\n");
}

void processingSystemTime(uint16_t sequenceCount) {
    printf("processingSystemTime ++\n");

    int numberOfRequest = 3; //요청 수 (최근3개)
    char *lastTimes[numberOfRequest]; //최근 시간 데이터를 저장할 배열
    memset(lastTimes, 0, sizeof(lastTimes));

    // JSON 파일 열기

    FILE *file = fopen(JSON_FILE_PATH, "r");
    if (!file) {
        perror("Failed to open JSON file");
        return;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    long currentPos = fileSize;

    char line[256];
    int count = 0;

    //파일을 한 줄씩 읽어서 최근 시간데이터 추출
    while ( currentPos >0 && count < numberOfRequest ) {
        //한 번에 한 글자씩 역방향으로 이동
        fseek(file, --currentPos, SEEK_SET);
        if (fgetc(file) == '\n' || currentPos == 0) {
            //줄의 시작 지점을 찾으면 해당 줄 읽기
            fgets(line, sizeof(line), file);
            printf("Reading line : %s", line);

            char *time_ptr = strstr(line, "\"time\": \"");

            if (time_ptr) {
                time_ptr += strlen("\"time\": \""); // "time": " 이후로 주소 이동
                lastTimes[count] = (char *)malloc(9);
                sscanf(time_ptr, "%8s", lastTimes[count]);
                printf("Found Time : %s\n", lastTimes[count]);
                count ++;
            }
            //다시 현재위치로 파일 포인터 이동
            fseek(file, currentPos, SEEK_SET);
            }
    }

    fclose(file);

    printf("Extracted times:\n");
    for (int i = 0; i < numberOfRequest; i++) {
        if (lastTimes[i]) {
            printf("  Time[%d]: %s\n", i, lastTimes[i]);
        } else {
            printf("  Time[%d]: NULL\n", i);
        }
    }

    for (int i = 0; i < numberOfRequest && lastTimes[i]; i++) {

        // 메시지 동적 할당
        Message *message = (Message *)malloc(sizeof(Message));
        if (!message) {
            perror("Failed to allocate memory for message");
            return;
        }

        // 메시지 필드 초기화
        memset(message, 0, sizeof(Message));

        // OPCode 설정
        strncpy(message->OPCode, OPCODE_SYSTEM_TIME, OPCODE_SIZE);

        // SequenceCount 설정
        sequenceCount++;
        message->SequenceCount = sequenceCount;

        // SizeofParm 설정 8 고정값
        uint16_t parmLength = 8;
        message->SizeofParm = parmLength;

        // Parm 값 설정
        strncpy(message->Parm, lastTimes[i], parmLength);
    
        // 메시지 전송
        broadcastSendMessage(message);

        free(message);
    }

    for (int i = 0; i < numberOfRequest; i++) {
        if (lastTimes[i]) { // NULL인지 확인
            free(lastTimes[i]);
            lastTimes[i] = NULL; // 포인터를 NULL로 설정해 이중 해제 방지
        }
    }

    printf("processingSystemTime --\n");
}

void processingSSDCapacity(uint16_t sequenceCount) {
    printf("processingSSDCapacity ++\n");

    int numberOfRequest = 3; // 요청 수 (최근 3개)
    char *lastTimes[numberOfRequest]; // 최근 시간 데이터를 저장할 배열
    char *lastSSDCapa[numberOfRequest];
    char *sumData[numberOfRequest];

    memset(lastTimes, 0, sizeof(lastTimes));
    memset(lastSSDCapa, 0, sizeof(lastSSDCapa));
    memset(sumData, 0, sizeof(sumData));

    FILE *file = fopen(JSON_FILE_PATH, "r");
    if (!file) {
        perror("Failed to open JSON file");
        return;
    }

    fseek(file, 0, SEEK_END); // 파일 끝으로 이동
    long currentPos = ftell(file); // 현재 파일 포인터 위치 (끝)
    char line[256];
    int count = 0;

    // 파일 끝에서 시작하여 역순으로 읽기
    while (currentPos > 0 && count < numberOfRequest) {
        // 파일 포인터를 한 글자씩 뒤로 이동
        fseek(file, --currentPos, SEEK_SET);

        if (fgetc(file) == '\n' || currentPos == 0) {
            // 줄 시작 지점으로 파일 포인터 이동
            if (currentPos != 0) fseek(file, currentPos + 1, SEEK_SET);
            else fseek(file, currentPos, SEEK_SET);

            // 해당 줄 읽기
            if (fgets(line, sizeof(line), file)) {
                printf("Reading line: %s", line);

                // "time"과 "ssdCapacity" 값 추출
                char *time_ptr = strstr(line, "\"time\": \"");
                char *ssd_ptr = strstr(line, "\"ssdCapacity\": \"");

                if (time_ptr && ssd_ptr) {
                    time_ptr += strlen("\"time\": \"");
                    ssd_ptr += strlen("\"ssdCapacity\": \"");

                    lastTimes[count] = (char *)malloc(9); // "HHHHMMSS" 8글자 + NULL
                    lastSSDCapa[count] = (char *)malloc(4); // "XXX" 3글자 + NULL

                    if (lastTimes[count] && lastSSDCapa[count]) {
                        sscanf(time_ptr, "%8s", lastTimes[count]);
                        sscanf(ssd_ptr, "%3s", lastSSDCapa[count]);
                        printf("Found Time: %s, SSD Capacity: %s\n", lastTimes[count], lastSSDCapa[count]);
                        count++;
                    } else {
                        perror("Memory allocation failed");
                        break;
                    }
                }
            }

            // 다시 파일 포인터를 현재 위치로 복구
            fseek(file, currentPos, SEEK_SET);
        }
    }

    fclose(file);

    // 문자열 합치기
    for (int i = 0; i < count; i++) {
        sumData[i] = (char *)malloc(strlen(lastTimes[i]) + strlen(lastSSDCapa[i]) + 1);
        if (!sumData[i]) {
            perror("Memory allocation failed for sumData");
            break;
        }
        sprintf(sumData[i], "%s%s", lastTimes[i], lastSSDCapa[i]);
    }

    // 메시지 생성 및 전송
    for (int i = 0; i < count; i++) {
        Message *message = (Message *)malloc(sizeof(Message));
        if (!message) {
            perror("Failed to allocate memory for message");
            break;
        }

        memset(message, 0, sizeof(Message));
        strncpy(message->OPCode, OPCODE_SSD_CAPACITY, OPCODE_SIZE);

        sequenceCount++;
        message->SequenceCount = sequenceCount;

        uint16_t parmLength = 11;
        message->SizeofParm = parmLength;
        snprintf(message->Parm, parmLength + 1, "%s", sumData[i]);

        broadcastSendMessage(message);
        free(message);
    }

    // 메모리 해제
    for (int i = 0; i < numberOfRequest; i++) {
        if (lastTimes[i]) free(lastTimes[i]);
        if (lastSSDCapa[i]) free(lastSSDCapa[i]);
        if (sumData[i]) free(sumData[i]);
    }

    printf("processingSSDCapacity --\n");
}


void processingWatchdogTimeout(uint16_t sequenceCount) {
    printf("processingWatchdogTimeout ++\n");
    uint8_t timeoutSecond = 60;
    uint8_t settingValue = timeoutSecond / 2;
    uint8_t remainTime = 0;

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        perror("Failed to allocate memory for message");
        return;
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    if( sequenceCount == 0 ) {
        sequenceCount = sequenceCount+2 ;
        // 타임아웃 기간 30초 설정
        sendCommandTimeout(timeoutSecond);
        // Parm 설정: 타임아웃시간 전송(초단위)
        snprintf(message->Parm, MAX_PARM_SIZE, "%d", settingValue);

    } else if ( sequenceCount == 3 ) {
        sendStartWatchdog();

        remainTime = sendWatchdogRemainTime();
        // Parm 설정: 타임아웃 시간 전송(초단위)
        printf("received watchdog remain time : %d", remainTime);
        snprintf(message->Parm, MAX_PARM_SIZE, "%d", remainTime);
        sequenceCount++;
    } else if ( sequenceCount == 5 ) {
        // 디렉토리가 없는 경우 생성
        ensureConfigDirectoryExists();
        // 설정 파일에 테스트 지속 상태 기록
        updateConfigFile(OPCODE_WATCHDOG_TIMEOUT, 1);
        printf("Recorded OPCODE_WATCHDOG_TIMEOUT as active (1) in the config file.\n");

        remainTime = sendWatchdogRemainTime();
        // Parm 설정: 타임아웃 시간 전송(초단위)
        snprintf(message->Parm, MAX_PARM_SIZE, "%d", remainTime);

        sequenceCount++;
    }  else if ( sequenceCount == 7 ) {
        free(message);
        return;
    } else if ( sequenceCount == 8 ) {
        // Boot Condition 값 읽기
        uint8_t readBootConditionValue = ReadBootCondition();
        uint8_t sincValue = readBootConditionValue -2;

        if ( readBootConditionValue == 5 ){
            sincValue = 2;
        }
        // Parm 설정 (Boot Condition 값 사용)
        message->Parm[0] = '0' + sincValue;  // 정수를 ASCII로 변환
        message->Parm[1] = '\0';  // Null-terminate

        ensureConfigDirectoryExists();
        updateConfigFile(OPCODE_WATCHDOG_TIMEOUT, 0);

    }

    // OPCode 설정
    strncpy(message->OPCode, OPCODE_WATCHDOG_TIMEOUT, OPCODE_SIZE);
    message->OPCode[OPCODE_SIZE] = '\0';  // Null-terminate

    message->SequenceCount = sequenceCount;

    // SizeofParm 설정: Parm의 실제 데이터 길이 계산
    message->SizeofParm = strlen(message->Parm);

    // 메시지 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    if ( sequenceCount == 6 ) {

        sequenceCount ++;

        // 메시지 동적 할당
        Message *message = (Message *)malloc(sizeof(Message));
        if (!message) {
            perror("Failed to allocate memory for message");
            return;
        }

        // 메시지 필드 초기화
        memset(message, 0, sizeof(Message));

        // OPCode 설정
        strncpy(message->OPCode, OPCODE_WATCHDOG_TIMEOUT, OPCODE_SIZE);
        message->OPCode[OPCODE_SIZE] = '\0';  // Null-terminate

        message->SequenceCount = sequenceCount;

        remainTime = sendWatchdogRemainTime();
        // Parm 설정: 타임아웃 시간 전송(초단위)
        snprintf(message->Parm, MAX_PARM_SIZE, "%d", remainTime);

        // SizeofParm 설정: Parm의 실제 데이터 길이 계산
        message->SizeofParm = strlen(message->Parm);

        // 메시지 전송
        broadcastSendMessage(message);

        // 메모리 해제
        free(message);

    }

    printf("processingWatchdogTimeout --\n");
}

// RS-232 속도 변경 및 송신 함수
void processingSerialSpeedControl(uint16_t sequenceCount) {
    printf("processingSerialSpeedControl ++\n");

    int fd = open(SERIAL_DEVICE, O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("Error opening RS-232 device");
        return;
    }

    if (sequenceCount == 2) {
        // 115200bps로 설정 후 "ABCDE" 전송
        if (configureSerialPort(fd, B115200) != 0) {
            close(fd);
            return;
        }

        printf("Sending 'ABCDE' at 115200 bps...\n");
        if (write(fd, "ABCDE", 5) != 5) {
            perror("Error writing to RS-232");
        }
        usleep(100000);  // 100ms 대기

    } else if (sequenceCount == 3) {
        // 9600bps로 변경 후 "ABCDE" 전송
        if (configureSerialPort(fd, B9600) != 0) {
            close(fd);
            return;
        }

        printf("Sending 'ABCDE' at 9600 bps...\n");
        if (write(fd, "ABCDE", 5) != 5) {
            perror("Error writing to RS-232");
        }
        usleep(100000);  // 100ms 대기
    }

    // 버퍼 플러시
    if (tcflush(fd, TCIFLUSH) == -1) {
        perror("Failed to flush input buffer");
    }
    if (tcflush(fd, TCOFLUSH) == -1) {
        perror("Failed to flush output buffer");
    }

    close(fd);
    printf("processingSerialSpeedControl --\n");
}

void processingBootingTimeCheck(uint16_t sequenceCount) {
    printf("processingBootingTimeCheck ++\n");

    if( sequenceCount == 0 ) {
        ensureConfigDirectoryExists();
        updateConfigFile(OPCODE_BOOTING_TIME_CHECK, 1);

                // 메시지 동적 할당
                Message *message = (Message *)malloc(sizeof(Message));
                if (!message) {
                    perror("Failed to allocate memory for message");
                    return;
                }
        
                // 메시지 필드 초기화
                memset(message, 0, sizeof(Message));
        
        
                // Parm 길이 설정 (Parm 이 없는 관계로 0)
                uint16_t parmLength = 0;
        
                // OPCode 설정
                strncpy(message->OPCode, OPCODE_BOOTING_TIME_CHECK, OPCODE_SIZE);
                message->OPCode[OPCODE_SIZE] = '\0';  // Null-terminate
        
                // SequenceCount 증가 및 설정
                sequenceCount += 2;
                message->SequenceCount = sequenceCount;
        
                // SizeofParm 설정
                message->SizeofParm = parmLength;
        
                // 메시지 전송
                broadcastSendMessage(message);
        
                // 메모리 해제
                free(message);

                // 재부팅 시작
                printf("System is rebooting now...\n");
                system("reboot");
            
    } else if ( sequenceCount == 1 ) {
        // 메시지 동적 할당
        Message *message = (Message *)malloc(sizeof(Message));
        if (!message) {
            perror("Failed to allocate memory for message");
            return;
        }

        // 메시지 필드 초기화
        memset(message, 0, sizeof(Message));


        // Parm 길이 설정 (Parm 이 없는 관계로 0)
        uint16_t parmLength = 0;

        // OPCode 설정
        strncpy(message->OPCode, OPCODE_BOOTING_TIME_CHECK, OPCODE_SIZE);
        message->OPCode[OPCODE_SIZE] = '\0';  // Null-terminate

        // SequenceCount 증가 및 설정
        sequenceCount += 2;
        message->SequenceCount = sequenceCount;

        // SizeofParm 설정
        message->SizeofParm = parmLength;

        // 메시지 전송
        broadcastSendMessage(message);

        // 메모리 해제
        free(message);

        ensureConfigDirectoryExists();
        updateConfigFile(OPCODE_BOOTING_TIME_CHECK, 0);
    }
    printf("processingBootingTimeCheck --\n");
}

// **프로그램 핀 체크 (이더넷으로 결과 전송)**
void processingProgramPinCheck(uint16_t sequenceCount) {
    printf("processingProgramPinCheck ++\n");

    if (sequenceCount == 2) {
        ensureConfigDirectoryExists();
        updateConfigFile(OPCODE_PROGRAM_PIN_CHECK, 1);
        ethernetInit();  
        usleep(3000000);

        // **재부팅 시작**
        printf("System is rebooting now...\n");
        system("reboot");
    } 
    else if (sequenceCount == 3) {

        printf(" processingProgramPinCheck  sequenceCount == 3 \n");
        printf("Opening log file: %s\n", PROGRAM_PIN_LOG_FILE_PATH);


        if (access(PROGRAM_PIN_LOG_FILE_PATH, F_OK) != 0) {
            perror("Error: File does not exist");
            return;
        }
        if (access(PROGRAM_PIN_LOG_FILE_PATH, R_OK) != 0) {
            perror("Error: File exists but is not readable");
            return;
        }

        printf("Opening log file: %s\n", PROGRAM_PIN_LOG_FILE_PATH);


        // **프로그램 핀 로그 읽기**
        FILE *file = fopen(PROGRAM_PIN_LOG_FILE_PATH, "r");
        if (!file) {
            perror("Error opening log file");
            return;
        }

        char lastLog[64] = {0};  // 넉넉한 크기의 버퍼 선언
        char buffer[64];

        // **파일을 끝까지 읽어서 마지막 줄을 저장**
        while (fgets(buffer, sizeof(buffer), file)) {
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
                buffer[len - 1] = '\0';  // 개행 문자 제거
            }
            strncpy(lastLog, buffer, sizeof(lastLog) - 1);
            lastLog[sizeof(lastLog) - 1] = '\0';  // Null-terminate
        }

        fclose(file);  // 파일 닫기

        if (strlen(lastLog) == 0) {
            printf("No valid logs found.\n");
            return;
        }

        printf("Programpin log read: %s\n", lastLog);

        // **메시지 전송 준비**
        uint16_t parmLength = strlen(lastLog);
        if (parmLength > sizeof(((Message *)0)->Parm)) {
            parmLength = sizeof(((Message *)0)->Parm) - 1;
        }

        // **메시지 동적 할당**
        Message *message = (Message *)malloc(sizeof(Message));
        if (!message) {
            fprintf(stderr, "Error: Memory allocation failed.\n");
            return;
        }

        // **메시지 필드 초기화**
        memset(message, 0, sizeof(Message));

        // **메시지 필드 설정**
        strncpy(message->OPCode, OPCODE_PROGRAM_PIN_CHECK, sizeof(message->OPCode) - 1);
        message->SequenceCount = sequenceCount;
        message->SizeofParm = parmLength;

        // **Parm 값 설정 (최신 로그 값 적용)**
        strncpy(message->Parm, lastLog, parmLength);
        message->Parm[parmLength] = '\0';  // Null-terminate

        printf("Generated Parm: %s\n", message->Parm);

        // **메시지 브로드캐스트 전송**
        broadcastSendMessage(message);

        // **메모리 해제**
        free(message);

        // **설정 파일 업데이트**
        updateConfigFile(OPCODE_PROGRAM_PIN_CHECK, 0);
    }

    printf("processingProgramPinCheck --\n");
}

// Ethernet 설정 변경 함수
void processingEthernetPortChange(uint16_t sequenceCount) {
    printf("processingEthernetPortChange ++\n");

    const char *ETH_INTERFACE = checkEthernetInterface();
    char command[100];
    char parmContent[10] = {0};
    uint8_t result = 1;  // 기본값 1

    switch (sequenceCount) {
        case 2: case 10: case 18: case 26: case 34: case 42: case 52: case 60: case 68: case 76: case 84:
            snprintf(command, sizeof(command), "ethtool -s %s speed 10 duplex full autoneg off", ETH_INTERFACE);
            strcpy(parmContent, "10");
            break;

        case 4: case 12: case 20: case 28: case 36: case 44: case 54: case 62: case 70: case 78: case 86:
            snprintf(command, sizeof(command), "ethtool -s %s speed 100 duplex full autoneg off", ETH_INTERFACE);
            strcpy(parmContent, "100");
            break;

        case 6: case 14: case 22: case 30: case 38: case 46: case 56: case 64: case 72: case 80: case 88:
            snprintf(command, sizeof(command), "ethtool -s %s speed 100 duplex half autoneg off", ETH_INTERFACE);
            strcpy(parmContent, "Half");
            break;

        case 8: case 16: case 24: case 32: case 40: case 48: case 58: case 66: case 74: case 82: case 90:
            snprintf(command, sizeof(command), "ethtool -s %s speed 100 duplex full autoneg off", ETH_INTERFACE);
            strcpy(parmContent, "Full");
            break;

        case 50:
            printf("Switching from Copper to Fiber...\n");
            result = setOpticPort();
            sprintf(parmContent, "%d", result);  // result 값을 문자열로 변환
            break;

        case 92:
            printf("Switching from Fiber to Copper...\n");
            result = setDefaultPort();
            sprintf(parmContent, "%d", result);  // result 값을 문자열로 변환
            break;

        default:
            printf("Invalid sequence count: %d\n", sequenceCount);
            return;
    }

    // 실제 이더넷 설정 적용 (옵틱/디폴트 전환이 아닌 경우)
    if (sequenceCount != 50 && sequenceCount != 92) {
        system(command);
        printf("Ethernet setting applied: %s\n", parmContent);
    }

    usleep(5000000);

    // 응답 메시지 전송
    uint16_t parmLength = strlen(parmContent);
    if (parmLength > sizeof(((Message *)0)->Parm)) {
        parmLength = sizeof(((Message *)0)->Parm) - 1;
    }

    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    memset(message, 0, sizeof(Message));
    strncpy(message->OPCode, OPCODE_ETHERNET_PORT_SETTING_CHANGE, sizeof(message->OPCode) - 1);
    message->SequenceCount = sequenceCount + 1;  // 응답 시퀀스는 요청보다 +1 증가
    message->SizeofParm = parmLength;
    strncpy(message->Parm, parmContent, parmLength);
    message->Parm[parmLength] = '\0';

    printf("Generated Parm: %s\n", message->Parm);

    broadcastSendMessage(message);
    free(message);

    printf("processingEthernetPortChange --\n");
}


void broadcastSendMessage(const Message *message) {
    struct sockaddr_in broadcastAddr;
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(SEND_PORT);
    broadcastAddr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    // ASCII 변환
    char sequenceCountStr[6];
    char sizeofParmStr[6];
    snprintf(sequenceCountStr, sizeof(sequenceCountStr), "%02u", message->SequenceCount);
    snprintf(sizeofParmStr, sizeof(sizeofParmStr), "%02u", message->SizeofParm);

    // 문자열로 변환 (OPCode + "-" 추가)
    char sendBuffer[BUFFER_SIZE];
    int offset = snprintf(sendBuffer, sizeof(sendBuffer), "%s-%s%s", 
                          message->OPCode, 
                          sequenceCountStr, 
                          sizeofParmStr);

    // Parm 추가
    if (message->SizeofParm > 0) {
        snprintf(sendBuffer + offset, sizeof(sendBuffer) - offset, "%s", message->Parm);
    }

    // 데이터 출력
    printf("\n----Send message----\n");
    printf("OPCode: %.*s\n", OPCODE_SIZE, message->OPCode);
    printf("SequenceCount: %s\n", sequenceCountStr);
    printf("SizeofParm: %s\n", sizeofParmStr);
    if (message->SizeofParm > 0) {
        printf("Parm: %s\n", message->Parm);
    } else {
        printf("Parm: (none)\n");
    }
    printf("--------------------\n");

    // 메시지 전송
    int sendLen = sendto(send_sockfd, sendBuffer, strlen(sendBuffer), 0, 
                         (struct sockaddr *)&broadcastAddr, sizeof(broadcastAddr));
    if (sendLen < 0) {
        perror("Broadcast message send failed");
    } else {
        printf("Broadcast message sent: %s\n", sendBuffer);
    }
}


// RS232 Init Config
int configureSerialPort(int fd, speed_t baudRate) {
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) {
        perror("Error getting tty attributes");
        return -1;
    }

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8비트 문자
    tty.c_iflag &= ~IGNBRK;                      // 브레이크 조건 무시하지 않음
    tty.c_lflag = 0;                             // Canonical 모드 비활성화
    tty.c_oflag = 0;                             // 출력 처리 비활성화
    tty.c_cc[VMIN]  = 1;                         // 최소 1 문자 읽기
    tty.c_cc[VTIME] = 1;                         // 0.1초 타임아웃

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);      // 소프트웨어 흐름 제어 비활성화
    tty.c_cflag |= (CLOCAL | CREAD);             // 로컬 연결, 읽기 가능 활성화
    tty.c_cflag &= ~(PARENB | PARODD);           // 패리티 비활성화
    tty.c_cflag &= ~CSTOPB;                      // 1 Stop bit
    tty.c_cflag &= ~CRTSCTS;                     // 하드웨어 흐름 제어 비활성화

    cfsetospeed(&tty, baudRate);  
    cfsetispeed(&tty, baudRate);

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("Error setting tty attributes");
        return -1;
    }

    // 입력 버퍼 비우기
    if (tcflush(fd, TCIFLUSH) == -1) {
        perror("Failed to flush input buffer");
    }
    
    return 0;
}


void start_broadcast(void) {
    init_recv_socket();
    init_send_socket();

    configureNetwork();
    usleep(5000000);
    configureNetwork();

    usleep(1000000);

    printf("checkAndProcessOpcodes +++++++++++++++++++++++++++++++++++++++++++++++++++.\n");
    checkAndProcessOpcodes();

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

    readValue = (discreteIn15to8Value & 0xFF) <<8;
    readValue |= discreteIn7to0Value & 0xFF;

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

// 설정 파일에 데이터를 기록 
void updateConfigFile(const char *opcode, int status) {
    printf("updateConfigFile ++ \n");

    FILE *file = fopen(CONFIG_FILE, "r+"); // 읽기/쓰기 모드로 파일 열기
    if (!file) {
        if (errno == ENOENT) { // 파일이 없는 경우
            printf("Config file does not exist. Creating a new one.\n");
            file = fopen(CONFIG_FILE, "w+"); // 새 파일 생성
            if (!file) {
                perror("Failed to create config file");
                return;
            }
        } else {
            perror("Failed to open config file for reading and writing");
            return;
        }
    }

    char line[256];
    bool updated = false;
    FILE *tempFile = tmpfile(); // 임시 파일 생성
    if (!tempFile) {
        perror("Failed to create temporary file");
        fclose(file);
        return;
    }

    // 기존 파일 읽기 및 업데이트
    while (fgets(line, sizeof(line), file)) {
        char key[256];
        int value;

        if (sscanf(line, "%[^=]=%d", key, &value) == 2) {
            if (strcmp(key, opcode) == 0) {
                // OPCODE가 존재 -> 업데이트
                fprintf(tempFile, "%s=%d\n", opcode, status);
                updated = true;
            } else {
                // 기존 라인은 그대로 저장
                fprintf(tempFile, "%s", line);
            }
        }
    }

    if (!updated) {
        // OPCODE가 존재하지 않으면 새로 추가
        fprintf(tempFile, "%s=%d\n", opcode, status);
    }

    // 원래 파일 덮어쓰기
    rewind(file);
    rewind(tempFile);
    while (fgets(line, sizeof(line), tempFile)) {
        fputs(line, file);
    }

    // 파일 크기 조정
    if (ftruncate(fileno(file), ftell(file)) != 0) {
        perror("Failed to truncate file");
    }

    // 캐시 플러시 및 디스크 동기화
    fflush(file);  // 캐시 플러시
    fsync(fileno(file)); // 디스크 동기화

    fclose(file);
    fclose(tempFile);

    printf("updateConfigFile -- \n");
}

// 설정 파일에서 데이터 읽기
int readConfigFile(const char *opcode) {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (!file) {
        perror("Failed to open config file for reading");
        return -1; // 파일 없음
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char key[128];
        int value;
        if (sscanf(line, "%127[^=]=%d", key, &value) == 2) {
            if (strcmp(key, opcode) == 0) {
                fclose(file);
                return value; // 해당 OPCODE의 값 반환
            }
        }
    }

    fclose(file);
    return -1; // OPCODE를 찾지 못함
}

// 활성화된 OPCODE 확인 및 처리
void checkAndProcessOpcodes() {
    printf("checkAndProcessOpcodes IN ++++++++++++++++++++++\n");
    // 설정 파일이 있는지 확인
    struct stat buffer;
    if (stat(CONFIG_FILE, &buffer) != 0) {
        printf("Config file not found. Skipping OPCODE processing.\n");
        return;
    }

    // "CSRSMR0001" 상태 확인 (WATCHDOG_TIMEOUT OPCODE)
    int watchdogTimeoutStatus = readConfigFile(OPCODE_WATCHDOG_TIMEOUT);
    int pushButtonStatus = readConfigFile(OPCODE_PUSH_BUTTON_CHECK);
    int holdupStatus = readConfigFile(OPCODE_HOLDUP_CHECK);
    int bootingTimeStatus = readConfigFile(OPCODE_BOOTING_TIME_CHECK);
    int programPinStatus = readConfigFile(OPCODE_PROGRAM_PIN_CHECK);

    // OPCODE별 상태 출력
    printf("[DEBUG] OPCODE Status - WATCHDOG_TIMEOUT: %d, PUSH_BUTTON: %d, HOLDUP_CHECK: %d\n",
        watchdogTimeoutStatus, pushButtonStatus, holdupStatus);

    if (watchdogTimeoutStatus == 1) {
        printf("OPCODE_WATCHDOG_TIMEOUT is active. Executing processingWatchdogTimeout()...\n");
        processingWatchdogTimeout(8); // 초기 시퀀스 카운트 전달
    }

    if (pushButtonStatus == 1) {
        printf("OPCODE_PUSH_BUTTON_CHECK is active. Executing processingPushbutton()...\n");
        processingPushbutton(1); // 초기 시퀀스 카운트 전달
    }

    if (holdupStatus == 1) {
        printf("OPCODE_HOLDUP_CHECK is active. Executing processingPushbutton()...\n");
        processingHoldupCheck(1); // 초기 시퀀스 카운트 전달
    }

    if (bootingTimeStatus == 1) {
        printf("OPCODE_BOOTING_TIME_CHECK is active. Executing processingBootingTimeCheck()...\n");
        processingBootingTimeCheck(1); // 초기 시퀀스 카운트 전달
    } 

    if ( programPinStatus == 1) {
        printf("OPCODE_PROGRAM_PIN_CHECK is active. Executing processingProgramPinCheck()...\n");
        usleep(3000000);
        processingProgramPinCheck(3); // 3 시퀀스 카운트 전달
    } 

    if (watchdogTimeoutStatus != 1 && pushButtonStatus != 1 && holdupStatus != 1 && bootingTimeStatus != 1 && programPinStatus != 1) {
        printf("No active OPCODEs found.\n");
    }
}


// 디렉토리 존재 확인 및 생성
void ensureConfigDirectoryExists() {
    printf( " ensureConfigDirectoryExists ++ \n");

    struct stat st = {0};
    if (stat("/home/ethernet_test", &st) == -1) {
        mkdir("/home/ethernet_test", 0700);
    }

        printf( " ensureConfigDirectoryExists -- \n");
}

void configureNetwork() {
    char interface[MAX_INTERFACE_NAME] = {0};
    const char *downInterface = NULL;

    // 동적으로 이더넷 인터페이스 확인
    const char *selectedInterface = checkEthernetInterface();
    strncpy(interface, selectedInterface, MAX_INTERFACE_NAME - 1);

    // 설정할 인터페이스와 다운시킬 인터페이스를 결정
    if (strcmp(interface, "eth0") == 0) {
        downInterface = "eth1";
    } else if (strcmp(interface, "eth1") == 0) {
        downInterface = "eth0";
    } else {
        fprintf(stderr, "Unknown interface: %s\n", interface);
        return;
    }

    printf("Configuring network settings for interface: %s\n", interface);

    // Step 1: Set IP address for the selected interface
    char command[128] = {0};
    snprintf(command, sizeof(command), "ifconfig %s 192.168.10.110", interface);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to set IP address for %s.\n", interface);
        return;
    }

    // Step 2: Bring down the other interface
    // snprintf(command, sizeof(command), "ifconfig %s down", downInterface);
    // if (system(command) != 0) {
    //     fprintf(stderr, "Failed to bring down %s.\n", downInterface);
    //     return;
    // }

    // Step 3: Bring down dummy0
    if (system("ifconfig dummy0 down") != 0) {
        fprintf(stderr, "Failed to bring down dummy0.\n");
        return;
    }

    // Step 4: Add default gateway
    if (system("route add default gw 192.168.10.1") != 0) {
        fprintf(stderr, "Failed to add default gateway.\n");
        return;
    }

    // Step 5: Add route for 192.168.10.0/24
    snprintf(command, sizeof(command), "route add -net 192.168.10.0 netmask 255.255.255.0 dev %s", interface);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to add route for 192.168.10.0/24.\n");
        return;
    }

    printf("Network configuration complete.\n");
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


void logProgramPinError() {
    FILE *logFile;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[20];
    
    // 시간 형식 YYMMDDHHMMSS
    strftime(timestamp, sizeof(timestamp), "%y%m%d%H%M%S", t);

    // 로그 저장
    logFile = fopen(PROGRAM_PIN_LOG_FILE_PATH, "a");
    if (logFile) {
        fprintf(logFile, "%sPROGRAM_PIN_COMPAT_FAULT\n", timestamp);

        fflush(logFile);  // 캐시 플러시
        fsync(fileno(logFile)); // 디스크 동기화

        fclose(logFile);
    } else {
        perror("Failed to open log file");
    }
}

void ethernetInit(void) {
    
    uint16_t readValue = readDiscreteIn(); 

    int isHigh12 = readValue & (1 << 12);
    int isHigh13 = readValue & (1 << 13);
    int isHigh14 = readValue & (1 << 14);
    int isHigh15 = readValue & (1 << 15);

    if (isHigh12 && (isHigh13 == 0 && isHigh14 == 0 && isHigh15 == 0)) {
        printf("Ethernet init : Optic Set.\n");
    } 
    else if ((isHigh12 == 0 && isHigh13 && isHigh14 && isHigh15) ||  // 0111
                (isHigh12 == 0 && isHigh13  == 0&& isHigh14 && isHigh15 )) { // 0011
        printf("Ethernet init : Copper Set.\n");
    }
    else {
        printf("Ethernet init : Program Pin Compatibility Fault. Logging error...\n");
        logProgramPinError();
    }
    
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <broadcast> <start/stop> | <set> | <test usb enable/disable>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "broadcast") == 0) {
        if (argc < 3) {
            printf("Usage: %s broadcast <start/stop>\n", argv[0]);
            return 1;
        }
        if (strcmp(argv[2], "start") == 0) {
            start_broadcast();
        } else if (strcmp(argv[2], "stop") == 0) {
            stop_broadcast();
        } else {
            printf("Invalid command. Use 'start' or 'stop'.\n");
        }
    } else if (strcmp(argv[1], "set") == 0) {
        printf("Set the IpAddr : %s.\n", SETTING_IP);
        configureNetwork();
    } else if (strcmp(argv[1], "test") == 0) {
        if (argc < 4) {
            printf("Usage: %s test usb <enable/disable>\n", argv[0]);
            return 1;
        }
        if (strcmp(argv[2], "usb") == 0) {
            if (strcmp(argv[3], "enable") == 0) {
                ActivateUSB();
            } else if (strcmp(argv[3], "disable") == 0) {
                DeactivateUSB();
            } else {
                printf("Invalid command. Use 'enable' or 'disable'.\n");
            }
        } else if (strcmp(argv[2], "rs232") == 0) {
            printf("Invalid test command. Use 'usb'.\n");
            if (strcmp(argv[3], "enable") == 0) {
                ActivateRS232();
            } else if (strcmp(argv[3], "disable") == 0) {
                DeactivateRS232();
            } else {
                printf("Invalid command. Use 'enable' or 'disable'.\n");
            }
        }
    } else {
        printf("Invalid command. Use 'broadcast', 'set', or 'test'.\n");
    }

    return 0;
}
