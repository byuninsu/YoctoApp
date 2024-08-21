/*
 * SPI testing utility (using spidev driver)
 *
 * Copyright (c) 2007  MontaVista Software, Inc.
 * Copyright (c) 2007  Anton Vorontsov <avorontsov@ru.mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * Cross-compile with cross-gcc -I/path/to/cross-kernel/include
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void pabort(const char *s)
{
	perror(s);
	abort();
}

static const char *device = "/dev/spidev2.0";
static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 500000;
static uint16_t delay;

uint8_t *tx;
int tx_len = 0;

static void transfer(int fd) {
    int ret;
    uint8_t tx_buf1[1] = { 0x9F }; // 명령어 전송 버퍼 (예: RDID 명령어)
    uint8_t tx_buf2[4] = { 0xFF, 0xFF, 0xFF, 0xFF }; // 더미 데이터 전송 버퍼
    uint8_t rx_buf[4] = { 0 }; // 수신 버퍼

    struct spi_ioc_transfer xfer[2];

    memset(&xfer, 0, sizeof(xfer));

    // 명령어 전송
    xfer[0].tx_buf = (uintptr_t)tx_buf1;
    xfer[0].rx_buf = 0;
    xfer[0].len = sizeof(tx_buf1);
    xfer[0].delay_usecs = delay;
    xfer[0].speed_hz = speed;
    xfer[0].bits_per_word = bits;
    xfer[0].cs_change = 0;

    // 더미 데이터 전송 및 데이터 수신
    xfer[1].tx_buf = (uintptr_t)tx_buf2;
    xfer[1].rx_buf = (uintptr_t)rx_buf;
    xfer[1].len = sizeof(tx_buf2);
    xfer[1].delay_usecs = delay;
    xfer[1].speed_hz = speed;
    xfer[1].bits_per_word = bits;
    xfer[1].cs_change = 0;

    ret = ioctl(fd, SPI_IOC_MESSAGE(2), xfer);
    if (ret < 1) {
        pabort("can't send spi message");
    }

    for (ret = 0; ret < sizeof(rx_buf); ret++) {
        if (!(ret % 6)) {
            puts("");
        }
        printf("%.2X ", rx_buf[ret]);
    }
    puts("");
}


static void print_usage(const char *prog)
{
	printf("Usage: %s [-DsbdlHOLC3t]\n", prog);
	puts("  -D --device   device to use (default /dev/spidev1.1)\n"
	     "  -s --speed    max speed (Hz)\n"
	     "  -d --delay    delay (usec)\n"
	     "  -b --bpw      bits per word\n"
	     "  -l --loop     loopback\n"
	     "  -H --cpha     clock phase\n"
	     "  -O --cpol     clock polarity\n"
	     "  -L --lsb      least significant bit first\n"
	     "  -C --cs-high  chip select active high\n"
	     "  -3 --3wire    SI/SO signals shared\n"
	     "  -t --txdata   data to transmit (hex bytes)\n");
	exit(1);
}

static void parse_opts(int argc, char *argv[])
{
	while (1) {
		static const struct option lopts[] = {
			{ "device",  1, 0, 'D' },
			{ "speed",   1, 0, 's' },
			{ "delay",   1, 0, 'd' },
			{ "bpw",     1, 0, 'b' },
			{ "loop",    0, 0, 'l' },
			{ "cpha",    0, 0, 'H' },
			{ "cpol",    0, 0, 'O' },
			{ "lsb",     0, 0, 'L' },
			{ "cs-high", 0, 0, 'C' },
			{ "3wire",   0, 0, '3' },
			{ "txdata",  1, 0, 't' },
			{ "no-cs",   0, 0, 'N' },
			{ "ready",   0, 0, 'R' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "D:s:d:b:lHOLC3t:", lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'D':
			device = optarg;
			break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'd':
			delay = atoi(optarg);
			break;
		case 'b':
			bits = atoi(optarg);
			break;
		case 'l':
			mode |= SPI_LOOP;
			break;
		case 'H':
			mode |= SPI_CPHA;
			break;
		case 'O':
			mode |= SPI_CPOL;
			break;
		case 'L':
			mode |= SPI_LSB_FIRST;
			break;
		case 'C':
			mode |= SPI_CS_HIGH;
			break;
		case '3':
			mode |= SPI_3WIRE;
			break;
		case 'N':
			mode |= SPI_NO_CS;
			break;
		case 'R':
			mode |= SPI_READY;
			break;
		case 't':
			// 16진수 문자열의 길이를 고려하여 tx_len 계산
			tx_len = strlen(optarg) / 2;
			tx = malloc(tx_len);
			for (int i = 0; i < tx_len; i++) {
				sscanf(&optarg[i * 2], "%2hhx", &tx[i]);
			}
			break;
		default:
			print_usage(argv[0]);
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int fd;

	parse_opts(argc, argv);

	if (!tx_len) {
		fprintf(stderr, "No txdata specified.\n");
		print_usage(argv[0]);
	}

	// tx_len이 1이고 tx[0]이 0x9F인 경우 tx_len을 1로 유지
	if (tx_len == 1 && tx[0] == 0x9F) {
		tx_len = 1;
	}

	fd = open(device, O_RDWR);
	if (fd < 0)
		pabort("can't open device");

	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");

	printf("spi mode: %d\n", mode);
	printf("bits per word: %d\n", bits);
	printf("max speed: %d Hz (%d KHz)\n", speed, speed / 1000);

	transfer(fd);

	close(fd);
	free(tx);

	return ret;
}

