/*
<배선>
    Lepton Thermal Camera Module    Raspberry Pi 4B
            VCC  ----------------->   3.3V
            GND  ----------------->   GND
            SCK  ----------------->   SPI0 SCLK (GPIO 11)
            MISO ----------------->   SPI0 MISO (GPIO 09)
            MOSI ----------------->   SPI0 MOSI (GPIO 10)
            CS   ----------------->   SPI0 CE0 (GPIO 08)
            SDA  ----------------->   I2C SDA0 (GPIO 00)
            SCL  ----------------->   I2C SCL0 (GPIO 01)
*/
#ifndef LEPTON_H
#define LEPTON_H

#include <stdint.h>

#define VOSPI_FRAME_SIZE (164)
#define MAX_LOOP_COUNT (1000000000)

#define LEPTON_WIDTH 80
#define LEPTON_HEIGHT 60
#define DEBUG_ID_CRC 2  // 2: ID 및 CRC 포함, 0: 순수 이미지 데이터만

// // 열화상 이미지 버퍼 (외부 접근용)
// extern uint16_t image[LEPTON_HEIGHT][LEPTON_WIDTH + DEBUG_ID_CRC];


int init_lepton(void);

int cleanup_lepton(int fd);

int lepton_capture(int fd);

void get_image(uint16_t (*cpy_image)[LEPTON_WIDTH]);

void print_image(int fd);

#endif