/*
 * WS2812b LED strip example (ab)using the SPI device
 * based on:
 * SPI testing utility (using spidev driver)
 *
 * Copyright (c) 2007  MontaVista Software, Inc.
 * Copyright (c) 2007  Anton Vorontsov <avorontsov@ru.mvista.com>
 * Copyright (c) 2016  penfold42 on github
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * Cross-compile with cross-gcc -I/path/to/cross-kernel/include
 *
 * Timings from: https://wp.josh.com/2014/05/13/ws2812-neopixels-are-not-so-finicky-once-you-get-to-know-them/
 * Symbol 	Parameter 		Min 	Typical 	Max 	Units
 * T0H 	0 code ,high voltage time       200 	350 	500 	nS
 * T1H 	1 code ,high voltage time       550 	700 		nS
 * TLD 	data, low voltage time          450 	600 	5,000 	nS
 * TLL 	latch, low voltage time        6,000 			ns
 *
 * If the SPI bus is running fast enough, you can (ab)use it to send the correctly timed
 * pulses on the MOSI pin. 
 *
 * If you target a bit period of 350nS, a SPI clock of 2,857,142 MHz _should_ work
 * - sending bit pattern 1000 => T0H 350nS, T0L 1050 nS
 * - sending bit pattern 1100 => T1H 700nS, T1L  700 nS
 * this meets the timing criteria and each pair of LED bits fits neatly in a single SPI byte
 *
 * The latch period of 6,000 nS requires 18 SPI bits of '0' (6000/350)
 * It's simpler just to send 3 bytes of 0.
 *
 * As of 12/5/2016 I've found that I really need a speed 3,800,000
 * I havent looked into why this doesnt match my calculations
 * If the speed is too low, the strip will be full white
 * If the speed is too high, the strip will be black or flickering black
 *
 *
 *
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <time.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))



static void pabort(const char *s)
{
	perror(s);
	abort();
}

static const char *device = "/dev/spidev0.0";
static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 3800000;
static uint16_t delay;

// each pair of WS2812 bits is sent as 1 spi byte
// looks up 2 bits of led data to get the spi pattern to send
// A ws2812 '0' is sent as 1000
// A ws2812 '1' is sent as 1100
// A ws2812 'reset/latch' is sent as 3 bytes of 00000000
uint8_t bitpair_to_byte[] = {
	0b10001000,
	0b10001100,
	0b11001000,
	0b11001100,
};

	uint8_t nled_data[] = {
		0x55, 0xaa, 0x55,
	};

// sample LED rainbow data Green, Red, Blue
	uint8_t led_data[] = {
		0,255,0,
		255,255,0,
		255,0,0,
		255,0,255,
		0,0,255,
		0,255,255,

		0,0,255,
		255,0,255,
		255,0,0,
		255,255,0,

	};

// number of physical leds
#define num_leds	60

// each led needs 3 bytes
// each led byte needs 4 SPI bytes
// +3 for the reset/latch pulse
uint8_t tx[num_leds*3*4+3];

static void make_spi_buffer(int offset)
{
	int ret;

	uint8_t led_colour=0;
	unsigned spi_ptr=0;
	unsigned led_num=0;
	int dimness = 1;

	for (led_num=0; led_num< num_leds ; led_num++ ) {
		for (led_colour=0; led_colour<3; led_colour++ ){
			unsigned which_led = (led_num+offset)%(ARRAY_SIZE(led_data)/3) * 3 + led_colour;
			unsigned a = led_data[which_led]/dimness;
//printf ("%d,", a);
			tx[spi_ptr++] = bitpair_to_byte[(a&0b11000000)>>6];
			tx[spi_ptr++] = bitpair_to_byte[(a&0b00110000)>>4];
			tx[spi_ptr++] = bitpair_to_byte[(a&0b00001100)>>2];
			tx[spi_ptr++] = bitpair_to_byte[(a&0b00000011)>>0];
		}
//printf ("\n");
	}

//printf ("num_leds %d spi_ptr %d \n", num_leds, spi_ptr);

}

static void transfer(int fd)
{
	int ret;

	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = 0,
		.len = ARRAY_SIZE(tx),
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		pabort("can't send spi message");
}

static void print_usage(const char *prog)
{
	printf("Usage: %s [-DsHON]\n", prog);
	puts("  -D --device   device to use (default /dev/spidev0.0)\n"
	     "  -s --speed    max speed (Hz) (default 3800000\n"
	     "  -H --cpha     clock phase\n"
	     "  -O --cpol     clock polarity\n"
	     "  -N --no-cs    no chip select signal\n"
	);
	exit(1);
}

static void parse_opts(int argc, char *argv[])
{
	while (1) {
		static const struct option lopts[] = {
			{ "device",  1, 0, 'D' },
			{ "speed",   1, 0, 's' },
			{ "cpha",    0, 0, 'H' },
			{ "cpol",    0, 0, 'O' },
			{ "no-cs",   0, 0, 'N' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "D:s:HON", lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'D':
			device = optarg;
			break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'H':
			mode |= SPI_CPHA;
			break;
		case 'O':
			mode |= SPI_CPOL;
			break;
		case 'N':
			mode |= SPI_NO_CS;
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
	printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

	while (1) {
		int i=0;
		for (i = 0 ; i<ARRAY_SIZE(led_data)/3; i++) {
			make_spi_buffer(i);
			transfer(fd);
			usleep(60000);
		}
	}
	close(fd);

	return ret;
}
