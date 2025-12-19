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

#define LEPTON_WIDTH 80
#define LEPTON_HEIGHT 60
#define LEPTON_PIXELS (LEPTON_WIDTH * LEPTON_HEIGHT)
#define LEPTON_FRAME_BYTES (LEPTON_PIXELS * 2)

#define LEPTON_MAGIC 0x4C455054  // "LEPT" in ASCII

#define LEPT_MAX_PAYLOAD 1200

typedef struct {
    uint32_t magic;       // LEPT_MAGIC (network byte order)
    uint32_t frame_id;    // sequence (network byte order)
    uint16_t chunk_id;    // 0..chunk_cnt-1 (network byte order)
    uint16_t chunk_cnt;   // total chunks (network byte order)
    uint16_t payload_len; // bytes in this packet (network byte order)
    uint16_t flags;       // reserved (network byte order)
} lept_hdr_t;

// 열화상 이미지 버퍼 (외부 접근용)
extern uint16_t image[LEPTON_HEIGHT][LEPTON_WIDTH];

// Lepton 카메라 초기화 및 종료
int init_lepton(void);
void close_lepton(int fd);

// 열화상 이미지 캡처
int set_image(int fd);

// 열화상 데이터 접근
uint16_t* get_thermal_image(void);
int get_thermal_frame(uint8_t *buffer, size_t size);

#endif