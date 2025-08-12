#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>

void print_current_time() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    printf("%s | ", buf);
}

unsigned long get_fd_limit_for_pid(pid_t pid) {
    char path[64];
    char line[256];
    FILE *fp;
    unsigned long soft_limit = 0;

    snprintf(path, sizeof(path), "/proc/%d/limits", pid);

    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
        return 65535; // fallback
    }

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "Max open files")) {
            // "Max open files    65535   65535   files" 와 같은 형식
            char name[32];
            unsigned long hard_limit = 0;
            sscanf(line, "%31[^\t]%lu%lu", name, &soft_limit, &hard_limit);
            break;
        }
    }

    fclose(fp);

    if (soft_limit == 0) {
        fprintf(stderr, "Max open files entry not found or could not parse.\n");
        return 65535;
    }

    return soft_limit;
}

int get_container_pid(const char* container_name, char* pid_buf, size_t buf_size) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
        "docker inspect -f '{{.State.Pid}}' %s 2>/dev/null", container_name);

    FILE* fp = popen(cmd, "r");
    if (!fp) return -1;

    if (!fgets(pid_buf, buf_size, fp)) {
        pclose(fp);
        return -1;
    }
    pclose(fp);

    pid_buf[strcspn(pid_buf, "\n")] = 0;  // remove newline

    return (strlen(pid_buf) > 0) ? 0 : -1;
}

int get_fd_count(const char* pid_str) {
    char cmd[256];
    char out[32];
    snprintf(cmd, sizeof(cmd), "ls /proc/%s/fd 2>/dev/null | wc -l", pid_str);

    FILE* fp = popen(cmd, "r");
    if (!fp) return -1;

    if (!fgets(out, sizeof(out), fp)) {
        pclose(fp);
        return -1;
    }
    pclose(fp);

    return atoi(out);
}

void sendRS232Message(const char* message) {
    // if (!message) return;

    // int fd = open("/dev/ttyTHS0", O_RDWR | O_NOCTTY);
    // if (fd == -1) {
    //     perror("[RS232] Failed to open");
    //     return;
    // }

    // struct termios tty;
    // memset(&tty, 0, sizeof tty);

    // if (tcgetattr(fd, &tty) != 0) {
    //     perror("[RS232] Failed to get attributes");
    //     close(fd);
    //     return;
    // }

    // // 기본 설정 (115200 8N1)
    // tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    // tty.c_iflag &= ~IGNBRK;
    // tty.c_lflag = 0;
    // tty.c_oflag = 0;
    // tty.c_cc[VMIN]  = 1;
    // tty.c_cc[VTIME] = 1;
    // tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    // tty.c_cflag |= (CLOCAL | CREAD);
    // tty.c_cflag &= ~(PARENB | PARODD);
    // tty.c_cflag &= ~CSTOPB;
    // tty.c_cflag &= ~CRTSCTS;

    // cfsetospeed(&tty, B115200);
    // cfsetispeed(&tty, B115200);

    // if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    //     perror("[RS232] Failed to set attributes");
    //     close(fd);
    //     return;
    // }

    // write(fd, message, strlen(message));
    // write(fd, "\r\n", 2);
    // tcdrain(fd);     
    // close(fd);
}

int main() {
    const char* container = "sysman";
    char pid_str[32];

    if (get_container_pid(container, pid_str, sizeof(pid_str)) != 0) {
        printf("Failed to get PID for container: %s\n", container);
        return 1;
    }

    pid_t pid = atoi(pid_str);
    printf("Monitoring '%s' with PID %d\n", container, pid);

    while (1) {
        char fd_path[64];
        snprintf(fd_path, sizeof(fd_path), "/proc/%d/fd", pid);

        struct stat st;
        if (stat(fd_path, &st) == 0) {
            int fd_count = get_fd_count(pid_str);
            unsigned long max_fd = get_fd_limit_for_pid(pid);
            float ratio = (float)fd_count / (float)max_fd;

            char msg[128];
            snprintf(msg, sizeof(msg), "FD Count: %d / %lu (%.2f%%)", fd_count, max_fd, ratio * 100);

            printf("%s\n", msg);
            sendRS232Message(msg);
        } else {
            printf("Container PID %d no longer exists.\n", pid);
            break;
        }

        usleep(2000000);  // 2초
    }

    return 0;
}
