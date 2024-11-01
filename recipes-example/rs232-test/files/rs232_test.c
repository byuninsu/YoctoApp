#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>

#define UART_PORT "/dev/ttyTHS0"
#define BUFFER_SIZE 256

int uart_fd;
volatile int keep_running = 0; // 0: 멈춤, 1: 에코 모드 실행

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

    return 0;
}

void *echo_function(void *arg) {
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while (keep_running) { // keep_running이 1일 때 에코 모드 실행
        // Read from UART
        bytes_read = read(uart_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // Null-terminate the buffer

            // Echo back to UART
            write(uart_fd, buffer, bytes_read);
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: rs232-test [start/stop]\n");
        return 1;
    }

    uart_fd = open(UART_PORT, O_RDWR | O_NOCTTY | O_NDELAY);

    if (uart_fd == -1) {
        perror("Failed to open UART");
        return -1;
    }

    configure_uart(uart_fd);

    pthread_t echo_thread;

    if (strcmp(argv[1], "start") == 0) {
        keep_running = 1;
        pthread_create(&echo_thread, NULL, echo_function, NULL);
        printf("Echo mode started.\n");
    } else if (strcmp(argv[1], "stop") == 0) {
        keep_running = 0; 
        printf("Echo mode stopped.\n");
    } else {
        fprintf(stderr, "Invalid command. Enter 'start' or 'stop'.\n");
        close(uart_fd);
        return 1;
    }

    if (keep_running) {
        pthread_join(echo_thread, NULL); 
    }

    close(uart_fd);
    return 0;
}
