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
#define BUFFER_SIZE 1024
#define OPCODE_SIZE 10  // OPCode 크기 정의
#define ENV_OPCODE_SIZE 7  // ENV_OPCode 크기 정의
#define MAX_PARM_SIZE 256  // Parm 최대 크기 정의
#define BROADCAST_IP "255.255.255.255"  // 브로드캐스트 주소
#define SETTING_IP "192.168.10.110"  // IP 셋팅 주소
#define SSD_SMARTLOG_LENGTH 17  // SMARTLOG LENGTH
#define BOOT_SIL_MODE "1"  
#define BOOT_NOMAL_MODE "0"  
#define MAX_INTERFACE_NAME 16
#define CONFIG_FILE "/home/ethernet_test/ethernet_test_config"

//OPCODE 정의
#define OPCODE_DISCRETE_IN "CSRICR0001"
#define OPCODE_DISCRETE_OUT "CSRICR0004"
#define OPCODE_BOOT_CONDITION "CSRSMR0005" 
#define OPCODE_BIT_TEST_PBIT "CSRBMR0001"
#define OPCODE_BIT_TEST_CBIT "CSRBMR0002"
#define OPCODE_BIT_TEST_IBIT "CSRBMR0003" 
#define OPCODE_BIT_TEST_RESULT_SAVE "CSRBMR0004" 
#define OPCODE_OS_SSD_BITE_TEST "CSRICR0006" 
#define OPCODE_INIT_NVRAM_SSD "CSRICR0005"
#define OPCODE_ETHERNET_COPPER_OPTIC "SSRICR0002"
#define OPCODE_RS232_USB_EN_DISABLE "CSRICR0002"  
#define OPCODE_PUSH_BUTTON_CHECK "SSRSMR0006"
#define OPCODE_WATCHDOG_TIMEOUT "CSRSMR0001"


typedef struct {
    char OPCode[OPCODE_SIZE + 1]; // OPCode + NULL 문자
    uint16_t SequenceCount;       // SequenceCount (16비트 정수)
    uint16_t SizeofParm;          // SizeofParm (16비트 정수)
    char Parm[MAX_PARM_SIZE];     // Parm
} Message;


// 함수 프로토타입 추가
void processingDiscreteIn(const char *Parm, uint16_t sequenceCount);
void processingBootCondition(const char Parm[], uint16_t sequenceCount);
void processingPushbutton(uint16_t sequenceCount);
void processingBitResult(const char Parm[], uint16_t sequenceCount, uint32_t requestBit);
void processingBitSave(const char Parm[], uint16_t sequenceCount);
void processingDiscreteOut(const char Parm[], uint16_t sequenceCount);
void processingInitSSDNvram(const char Parm[], uint16_t sequenceCount);
void processingEthernetSetting(const char Parm[], uint16_t sequenceCount, uint8_t type);
void processingInitUsbRs232EnDisable(const char Parm[], uint16_t sequenceCount, uint8_t act);
void processingWatchdogTimeout(uint16_t sequenceCount);
int init_recv_socket();
int init_send_socket();
void *receive_and_parse_message(void *arg);
void start_broadcast();
void stop_broadcast();
uint16_t parse_binary_string(const char *binary_str);
void uint16_to_binary_string(uint16_t value, char *output, size_t output_size);
void broadcastSendMessage(const Message *message); 
void processingSSDBite(const char Parm[], uint16_t sequenceCount, uint8_t ssd_type);
void updateConfigFile(const char *opcode, int status);
int readConfigFile(const char *opcode);
void ensureConfigDirectoryExists();
void checkAndProcessOpcodes();


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
    char buffer[BUFFER_SIZE]; // 수신 버퍼
    Message msg;              // 변환된 메시지 구조체

    printf("Waiting to receive messages...\n");

    while (keep_running) { // 무한 대기 루프
        // 수신 버퍼 초기화
        memset(buffer, 0, BUFFER_SIZE);

        // 데이터 수신
        ssize_t n = recvfrom(recv_sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) {
            perror("recvfrom failed");
            continue;
        }

        printf("\n Raw buffer: %.*s\n\n", (int)n, buffer);

        // Message 구조체 초기화
        memset(&msg, 0, sizeof(msg));
        strncpy(msg.OPCode, buffer, OPCODE_SIZE);
        msg.OPCode[OPCODE_SIZE] = '\0'; // 널 문자 추가

        uint16_t sequenceCount = 0;
        uint16_t sizeofParm = 0;

        // 데이터 파싱
        if (n >= OPCODE_SIZE + 4) {
            // SequenceCount 추출
            if (isdigit(buffer[OPCODE_SIZE]) && isdigit(buffer[OPCODE_SIZE + 1])) {
                sequenceCount = (uint16_t)((buffer[OPCODE_SIZE] - '0') * 10 + (buffer[OPCODE_SIZE + 1] - '0'));
            }
            // SizeofParm 추출
            if (isdigit(buffer[OPCODE_SIZE + 2]) && isdigit(buffer[OPCODE_SIZE + 3])) {
                sizeofParm = (uint16_t)((buffer[OPCODE_SIZE + 2] - '0') * 10 + (buffer[OPCODE_SIZE + 3] - '0'));
            }

            // Parm 데이터 확인 및 복사
            if (sizeofParm > 0 && n >= (OPCODE_SIZE + 4 + sizeofParm)) {
                strncpy(msg.Parm, buffer + OPCODE_SIZE + 4, sizeofParm);
                msg.Parm[sizeofParm] = '\0'; // 널 문자 추가
            }
        } else {
            printf("Buffer too short for parsing SequenceCount and SizeofParm.\n");
        }

        // 데이터 출력
        printf("----Received message----\n");
        printf("OPCode: %.*s\n", OPCODE_SIZE, msg.OPCode);
        printf("SequenceCount: %u\n", sequenceCount);
        printf("sizeofParm: %u\n", sizeofParm);

        if (sizeofParm > 0) {
            printf("Parm: %s\n", msg.Parm);
        } else {
            printf("Parm: (none)\n");
        }
        printf("------------------------\n\n");

        // `msg` 필드에 값 업데이트
        msg.SequenceCount = sequenceCount;
        msg.SizeofParm = sizeofParm;

        // Parm 값이 없는 경우: 응답 메시지 구성
        if (msg.SizeofParm == 0 && msg.SequenceCount == 0) {
            Message responseMsg = {0};

            // OPCode 복사
            strncpy(responseMsg.OPCode, msg.OPCode, OPCODE_SIZE);

            // SequenceCount 증가
            responseMsg.SequenceCount = msg.SequenceCount + 1;

            // SizeofParm 설정
            responseMsg.SizeofParm = 0;

            // 응답 메시지 전송
            broadcastSendMessage(&responseMsg);
            printf("Sent response for Parm-less message\n");
            //continue;
        }

        printf("Processing received data...\n");

        // `Parm` 값이 있을 경우 처리 함수 호출
        if (memcmp(msg.OPCode, OPCODE_DISCRETE_IN, OPCODE_SIZE) == 0) {
            if ( sizeofParm != 0 ) {
                printf("Received OPCODE_DISCRETE_IN message\n\n");
                processingDiscreteIn(msg.Parm, msg.SequenceCount);
            }
        } else if (memcmp(msg.OPCode, OPCODE_BOOT_CONDITION, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_BOOT_CONDITION message\n\n");
            processingBootCondition(msg.Parm, msg.SequenceCount +1 );
        } else if (memcmp(msg.OPCode, OPCODE_PUSH_BUTTON_CHECK, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_PUSH_BUTTON_CHECK message\n\n");
            processingPushbutton(msg.SequenceCount);
        } else if (memcmp(msg.OPCode, OPCODE_BIT_TEST_PBIT, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_BIT_TEST_PBIT message\n\n");
            uint32_t bit_state = 2;
            processingBitResult(msg.Parm, msg.SequenceCount +1, bit_state);
        } else if (memcmp(msg.OPCode, OPCODE_BIT_TEST_CBIT, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_BIT_TEST_CBIT message\n\n");
            uint32_t bit_state = 3;
            processingBitResult(msg.Parm, msg.SequenceCount +1, bit_state);
        } else if (memcmp(msg.OPCode, OPCODE_BIT_TEST_IBIT, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_BIT_TEST_IBIT message\n\n");
            uint32_t bit_state = 4;
            processingBitResult(msg.Parm, msg.SequenceCount +1, bit_state);
        } else if (memcmp(msg.OPCode, OPCODE_BIT_TEST_RESULT_SAVE, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_BIT_TEST_RESULT_SAVE message\n\n");
            processingBitSave(msg.Parm, msg.SequenceCount +1);
        } else if (memcmp(msg.OPCode, OPCODE_DISCRETE_OUT, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_DISCRETE_OUT message\n\n");
            processingDiscreteOut(msg.Parm, msg.SequenceCount +1);
        } else if (memcmp(msg.OPCode, OPCODE_OS_SSD_BITE_TEST, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_OS_SSD_BITE_TEST message\n\n");
            uint8_t ssd_type = 2;
            processingSSDBite(msg.Parm, msg.SequenceCount +1, ssd_type);
        } else if (memcmp(msg.OPCode, OPCODE_INIT_NVRAM_SSD, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_INIT_NVRAM_SSD message\n\n");
            processingInitSSDNvram(msg.Parm, msg.SequenceCount +1);
        } else if (memcmp(msg.OPCode, OPCODE_ETHERNET_COPPER_OPTIC, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_ETHERNET_COPPER_OPTIC message\n\n");
            uint8_t copper = 1;
            uint8_t optic = 2;
            if ( msg.SequenceCount == 2) {
                processingEthernetSetting(msg.Parm, msg.SequenceCount, optic);
            }
        } else if (memcmp(msg.OPCode, OPCODE_RS232_USB_EN_DISABLE, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_RS232_USB_EN_DISABLE message\n\n");
            uint8_t inputAct = 1; 
            uint8_t inputDeAct = 1; 

            if( msg.SequenceCount == 3 ) {
                processingInitUsbRs232EnDisable(msg.Parm, msg.SequenceCount, inputAct);
            } else if ( msg.SequenceCount == 0 ) {
                processingInitUsbRs232EnDisable(msg.Parm, msg.SequenceCount+1 , inputDeAct);
            }
        } else if (memcmp(msg.OPCode, OPCODE_WATCHDOG_TIMEOUT, OPCODE_SIZE) == 0) {
            printf("Received OPCODE_WATCHDOG_TIMEOUT message\n\n");
            processingWatchdogTimeout(msg.SequenceCount +1);
        }
    }
    return NULL;
}

// Discrete IN 처리 함수
void processingDiscreteIn(const char Parm[], uint16_t sequenceCount) {
    printf("processingDiscreteOut ++\n");

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
}

// Discrete OUT 처리 함수
void processingDiscreteOut(const char Parm[], uint16_t sequenceCount) {
    printf("processingDiscreteOut ++\n");

    // SequenceCount 증가
    sequenceCount++;

    // 현재 상태 읽기
    uint32_t discreteOutValue = getDiscreteOutAll();

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

    strncpy(message->OPCode, OPCODE_DISCRETE_OUT, OPCODE_SIZE);
    //message->OPCode[OPCODE_SIZE - 1] = '\0'; // 널 문자 추가 (OPCODE_SIZE는 널 포함 크기라고 가정)

    message->SequenceCount = sequenceCount;
    message->SizeofParm = parmLength;

    // Parm 값을 2진 문자열로 변환
    char valueOfString[17]; // 16비트 + '\0'
    uint16_to_binary_string((uint16_t)discreteOutValue, valueOfString, sizeof(valueOfString));

    // 결과를 Parm에 복사
    memcpy(message->Parm, valueOfString, parmLength);

    // 메시지 브로드캐스트 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);
}

// SSD, NVRAM 초기화 처리 함수
void processingInitSSDNvram(const char Parm[], uint16_t sequenceCount) {
    printf("processingInitSSDNvram ++\n");

    // SequenceCount 증가
    sequenceCount++;

    // SSD 초기화 상태 읽기
    uint8_t initSSD = initializeDataSSD();

    // NVRAM 초기화 상태 읽기
    uint8_t initNVRAM = InitializeNVRAMToFF();

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
}

// 이더넷 구리,광 변경  처리 함수
void processingEthernetSetting(const char Parm[], uint16_t sequenceCount, uint8_t type) {
    printf("processingEthernetSetting ++\n");

    // SequenceCount 증가
    sequenceCount++;

    // 초기화 결과 변수
    uint8_t result = 0;

    if (type == 1) {
        // Default 포트 설정
        result = setDefaultPort();
    } else if (type == 2) {
        // Optic 전환
        result = setOpticPort();
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
    }
    printf("processingPushbutton --\n");
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
        "Power Monitor"
    };

    // 항목 개수
    const size_t bitCount = sizeof(bitTestItems) / sizeof(bitTestItems[0]);

    // BIT 상태를 담을 버퍼 (2자리씩 13항목 => 26바이트)
    char bitResultString[bitCount * 2 + 1]; // 26 + NULL
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
        logValue.controller_busy_time,
        logValue.power_cycles,
        logValue.power_on_hours,
        logValue.unsafe_shutdowns,
        logValue.media_and_data_errors,
        logValue.error_log_entries,
        logValue.warning_temp_time,
        logValue.critical_temp_time,
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


void processingWatchdogTimeout(uint16_t sequenceCount) {
    printf("processingWatchdogTimeout ++\n");
    uint8_t timeoutSecond = 60;
    uint8_t settingValue = timeoutSecond / 2;  

    // 메시지 동적 할당
    Message *message = (Message *)malloc(sizeof(Message));
    if (!message) {
        perror("Failed to allocate memory for message");
        return;
    }

    if( sequenceCount == 0 ) {
        // 타임아웃 기간 30초 설정
        sendCommandTimeout(timeoutSecond);
        // Parm 설정: 타임아웃시간 전송(초단위)
        snprintf(message->Parm, MAX_PARM_SIZE, "%d", settingValue);

    } else if ( sequenceCount == 3 ) {
        sendStartWatchdog();
        // Parm 설정: 타임아웃 시간 전송(초단위)
        snprintf(message->Parm, MAX_PARM_SIZE, "%d", settingValue);
    } else if ( sequenceCount == 5 ) {
        // 디렉토리가 없는 경우 생성
        ensureConfigDirectoryExists();
        // 설정 파일에 테스트 지속 상태 기록
        updateConfigFile(OPCODE_WATCHDOG_TIMEOUT, 1);
        printf("Recorded OPCODE_WATCHDOG_TIMEOUT as active (1) in the config file.\n");

    } else if ( sequenceCount == 7 ) {
        // Boot Condition 값 읽기
        uint8_t readBootConditionValue = ReadBootCondition();
        uint8_t sincValue = readBootConditionValue -2;

        if ( readBootConditionValue == 5 ){
            sincValue = 2;
        }
        // Parm 설정 (Boot Condition 값 사용)
        message->Parm[0] = '0' + sincValue;  // 정수를 ASCII로 변환
        message->Parm[1] = '\0';  // Null-terminate
    }

    // 메시지 필드 초기화
    memset(message, 0, sizeof(Message));

    // OPCode 설정
    strncpy(message->OPCode, OPCODE_WATCHDOG_TIMEOUT, OPCODE_SIZE);
    message->OPCode[OPCODE_SIZE] = '\0';  // Null-terminate

    // SequenceCount 증가 및 설정
    sequenceCount++;
    message->SequenceCount = sequenceCount;

    // SizeofParm 설정: Parm의 실제 데이터 길이 계산
    message->SizeofParm = strlen(message->Parm);

    // 메시지 전송
    broadcastSendMessage(message);

    // 메모리 해제
    free(message);

    printf("processingWatchdogTimeout --\n");
}


void broadcastSendMessage(const Message *message) {
    struct sockaddr_in broadcastAddr;
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(SEND_PORT);
    broadcastAddr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    // ASCII 변환
    char sequenceCountStr[3];
    char sizeofParmStr[3];
    snprintf(sequenceCountStr, sizeof(sequenceCountStr), "%02u", message->SequenceCount);
    snprintf(sizeofParmStr, sizeof(sizeofParmStr), "%02u", message->SizeofParm);

    // 문자열로 변환
    char sendBuffer[BUFFER_SIZE];
    int offset = snprintf(sendBuffer, sizeof(sendBuffer), "%s%s%s", 
                        message->OPCode, 
                        sequenceCountStr, 
                        sizeofParmStr);

    // Parm 추가
    if (message->SizeofParm > 0) {
        snprintf(sendBuffer + offset, sizeof(sendBuffer) - offset, "%s", message->Parm);
    }

    // 데이터 출력
    printf("\n\n----Send message----\n");
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

    checkAndProcessOpcodes();

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

// 설정 파일에 데이터를 기록
void updateConfigFile(const char *opcode, int status) {
    FILE *file = fopen(CONFIG_FILE, "a");
    if (!file) {
        perror("Failed to open config file for writing");
        return;
    }

    // OPCODE와 상태를 기록
    fprintf(file, "%s=%d\n", opcode, status);
    fclose(file);
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
    // 설정 파일이 있는지 확인
    struct stat buffer;
    if (stat(CONFIG_FILE, &buffer) != 0) {
        printf("Config file not found. Skipping OPCODE processing.\n");
        return;
    }

    // "CSRSMR0001" 상태 확인 (WATCHDOG_TIMEOUT OPCODE)
    int watchdogTimeoutStatus = readConfigFile("CSRSMR0001");
    int pushButtonStatus = readConfigFile("CSRSMR0006");

    if (watchdogTimeoutStatus == 1) {
        printf("OPCODE CSRSMR0001 is active. Executing processingWatchdogTimeout()...\n");
        processingWatchdogTimeout(7); // 초기 시퀀스 카운트 전달
    }

    if (pushButtonStatus == 1) {
        printf("OPCODE CSRSMR0006 is active. Executing processingPushbutton()...\n");
        processingPushbutton(1); // 초기 시퀀스 카운트 전달
    }

    if (watchdogTimeoutStatus != 1 && pushButtonStatus != 1) {
        printf("No active OPCODEs found.\n");
    }
}


// 디렉토리 존재 확인 및 생성
void ensureConfigDirectoryExists() {
    struct stat st = {0};
    if (stat("/home/ethernet_test", &st) == -1) {
        mkdir("/home/ethernet_test", 0700);
    }
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
    snprintf(command, sizeof(command), "ifconfig %s down", downInterface);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to bring down %s.\n", downInterface);
        return;
    }

    // Step 3: Bring down dummy0
    if (system("ifconfig dummy0 down") != 0) {
        fprintf(stderr, "Failed to bring down dummy0.\n");
        return;
    }

    usleep(10000);

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
