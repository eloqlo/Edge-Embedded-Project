#ifndef LEPTON_H
#define LEPTON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <limits.h>
#include <string.h>
#include <sys/select.h>
#include <pigpio.h>

int init_lepton(void);
int visualize_img(int fd);
int save_img(int fd);
int lepton_stream(int fd);

#endif