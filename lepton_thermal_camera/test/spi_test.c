/*
 * 컴파일 방법:
 * gcc -o spi_test spi_test.c
 * 
 * 실행 방법 (root 권한 필요):
 * sudo ./spi_test
 * 
 * 주의사항:
 * 1. SPI를 활성화해야 합니다:
 *    sudo raspi-config
 *    → Interface Options → SPI → Enable
 * 
 * 2. 또는 /boot/config.txt 파일에 추가:
 *    dtparam=spi=on
 *    (재부팅 필요)
 * 
 * 3. SPI 디바이스 확인:
 *    ls -l /dev/spidev*
 * 
 * 4. 루프백 테스트:
 *    MOSI(GPIO 10)와 MISO(GPIO 9)를 점퍼선으로 연결하면
 *    보낸 데이터가 그대로 돌아옵니다.
 * 
 * 5. SPI 슬레이브 디바이스 연결 시:
 *    - 데이터시트에서 SPI 모드 확인
 *    - 최대 클럭 속도 확인
 *    - 통신 프로토콜 확인
 */

#include <stdio.h>          // printf(), perror() 함수
#include <stdlib.h>         // exit() 함수
#include <stdint.h>         // uint8_t, uint32_t 등
#include <string.h>         // memset() 함수
#include <fcntl.h>          // open() 함수
#include <unistd.h>         // close(), read(), write() 함수
#include <sys/ioctl.h>      // ioctl() 함수
#include <linux/spi/spidev.h>  // SPI 관련 정의

/*
 * SPI (Serial Peripheral Interface) 통신 기본 개념:
 * 
 * SPI는 마스터-슬레이브 구조의 동기식 시리얼 통신 방식입니다.
 * 
 * 주요 신호선 4개:
 * - MOSI (Master Out Slave In): 마스터 → 슬레이브 데이터 전송
 * - MISO (Master In Slave Out): 슬레이브 → 마스터 데이터 전송
 * - SCLK (Serial Clock): 클럭 신호 (마스터가 생성)
 * - CS/SS (Chip Select): 슬레이브 선택 신호 (LOW일 때 활성화)
 * 
 * 라즈베리파이 4B SPI 핀:
 * - SPI0 MOSI: GPIO 10 (핀 19)
 * - SPI0 MISO: GPIO 9  (핀 21)
 * - SPI0 SCLK: GPIO 11 (핀 23)
 * - SPI0 CE0:  GPIO 8  (핀 24)
 * - SPI0 CE1:  GPIO 7  (핀 26)
 */

// SPI 디바이스 파일 경로
#define SPI_DEVICE "/dev/spidev0.0"  // SPI0, CE0 사용

// SPI 설정 구조체
typedef struct {
    int fd;              // 파일 디스크립터
    uint8_t mode;        // SPI 모드 (0~3)
    uint8_t bits;        // 비트 수 (보통 8)
    uint32_t speed;      // 클럭 속도 (Hz)
    uint16_t delay;      // 전송 간 지연 시간
} spi_config_t;

/**
 * SPI 초기화 함수
 * 
 * @param config: SPI 설정 구조체 포인터
 * @return: 성공 시 0, 실패 시 -1
 */
int spi_init(spi_config_t *config)
{
    int ret;

    // 1. SPI 디바이스 파일 열기
    // O_RDWR: 읽기/쓰기 모드로 열기
    config->fd = open(SPI_DEVICE, O_RDWR);  // read/write 모드
    if (config->fd < 0) {
        perror("SPI 디바이스 열기 실패");
        return -1;
    }
    printf("SPI 디바이스 열기 성공: %s\n", SPI_DEVICE);

    // 2. SPI 모드 설정
    // ioctl(): 디바이스 제어 함수
    // SPI_IOC_WR_MODE: SPI 모드 쓰기
    ret = ioctl(config->fd, SPI_IOC_WR_MODE, &config->mode);
    if (ret < 0) {
        perror("SPI 모드 설정 실패");
        close(config->fd);
        return -1;
    }
    printf("SPI 모드 설정: %d\n", config->mode);

    // 3. 비트 수 설정 (보통 8비트)
    // SPI_IOC_WR_BITS_PER_WORD: 워드당 비트 수 설정
    ret = ioctl(config->fd, SPI_IOC_WR_BITS_PER_WORD, &config->bits);
    if (ret < 0) {
        perror("비트 수 설정 실패");
        close(config->fd);
        return -1;
    }
    printf("비트 수 설정: %d bits\n", config->bits);

    // 4. 클럭 속도 설정
    // SPI_IOC_WR_MAX_SPEED_HZ: 최대 클럭 속도 설정
    ret = ioctl(config->fd, SPI_IOC_WR_MAX_SPEED_HZ, &config->speed);
    if (ret < 0) {
        perror("클럭 속도 설정 실패");
        close(config->fd);
        return -1;
    }
    printf("클럭 속도 설정: %d Hz (%d kHz)\n", config->speed, config->speed / 1000);

    printf("SPI 초기화 완료!\n\n");
    return 0;
}

/**
 * SPI 데이터 전송/수신 함수 (Full Duplex)
 * 
 * SPI는 전이중(Full Duplex) 통신입니다.
 * 데이터를 보내는 동시에 데이터를 받습니다.
 * 
 * @param config: SPI 설정 구조체 포인터
 * @param tx_buf: 전송할 데이터 버퍼
 * @param rx_buf: 수신할 데이터 버퍼
 * @param len: 전송/수신 데이터 길이
 * @return: 성공 시 0, 실패 시 -1
 */
int spi_transfer(spi_config_t *config, uint8_t *tx_buf, uint8_t *rx_buf, int len)
{
    int ret;

    // SPI 전송 구조체 설정
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_buf,  // 전송 버퍼 주소
        .rx_buf = (unsigned long)rx_buf,  // 수신 버퍼 주소
        .len = len,                        // 전송/수신 길이
        .speed_hz = config->speed,         // 클럭 속도
        .delay_usecs = config->delay,      // 전송 간 지연
        .bits_per_word = config->bits,     // 비트 수
        .cs_change = 0,                    // CS 신호 유지 (0: 유지, 1: 해제)
    };

    // ioctl()로 SPI 전송 수행
    // SPI_IOC_MESSAGE(1): 하나의 SPI 메시지 전송
    ret = ioctl(config->fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1) {
        perror("SPI 전송 실패");
        return -1;
    }

    return 0;
}

/**
 * SPI 종료 함수
 */
void spi_close(spi_config_t *config)
{
    if (config->fd >= 0) {
        close(config->fd);
        printf("SPI 디바이스 닫기 완료\n");
    }
}

/**
 * 메인 함수 - SPI 통신 예제
 */
int main(void)
{
    spi_config_t spi;
    int ret;

    printf("=== 라즈베리파이 SPI 통신 예제 ===\n\n");

    // SPI 설정 초기화
    spi.mode = SPI_MODE_3;           // SPI 모드 3
    spi.bits = 8;                     // 8비트 전송
    spi.speed = 1000000;              // 1MHz (1,000,000 Hz)
    spi.delay = 0;                    // 지연 없음

    // SPI 초기화
    ret = spi_init(&spi);
    if (ret < 0) {
        printf("SPI 초기화 실패!\n");
        return 1;
    }

    // ===== 예제 1: 단일 바이트 전송/수신 =====
    printf("--- 예제 1: 단일 바이트 전송/수신 ---\n");
    uint8_t tx_data = 0xAB;           // 전송할 데이터
    uint8_t rx_data = 0x00;           // 수신할 데이터 버퍼

    printf("전송 데이터: 0x%02X\n", tx_data);
    
    ret = spi_transfer(&spi, &tx_data, &rx_data, 1);
    if (ret == 0) {
        printf("수신 데이터: 0x%02X\n", rx_data);
    }
    printf("\n");

    // ===== 예제 2: 다중 바이트 전송/수신 =====
    printf("--- 예제 2: 다중 바이트 전송/수신 ---\n");
    uint8_t tx_buffer[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t rx_buffer[4] = {0};

    printf("전송 데이터: ");
    for (int i = 0; i < 4; i++) {
        printf("0x%02X ", tx_buffer[i]);
    }
    printf("\n");

    ret = spi_transfer(&spi, tx_buffer, rx_buffer, 4);
    if (ret == 0) {
        printf("수신 데이터: ");
        for (int i = 0; i < 4; i++) {
            printf("0x%02X ", rx_buffer[i]);
        }
        printf("\n");
    }
    printf("\n");

    // ===== 예제 3: 읽기 전용 (더미 데이터 전송) =====
    printf("--- 예제 3: 읽기 전용 ---\n");
    uint8_t dummy_tx[3] = {0x00, 0x00, 0x00};  // 더미 데이터
    uint8_t read_rx[3] = {0};

    printf("더미 데이터 전송 후 수신...\n");
    
    ret = spi_transfer(&spi, dummy_tx, read_rx, 3);
    if (ret == 0) {
        printf("수신 데이터: ");
        for (int i = 0; i < 3; i++) {
            printf("0x%02X ", read_rx[i]);
        }
        printf("\n");
    }
    printf("\n");

    // ===== 예제 4: 쓰기 전용 (수신 무시) =====
    printf("--- 예제 4: 쓰기 전용 ---\n");
    uint8_t write_tx[2] = {0xBB, 0xCC};
    uint8_t write_rx[2] = {0};  // 수신 데이터는 무시

    printf("쓰기 전용 데이터: ");
    for (int i = 0; i < 2; i++) {
        printf("0x%02X ", write_tx[i]);
    }
    printf("\n");

    ret = spi_transfer(&spi, write_tx, write_rx, 2);
    if (ret == 0) {
        printf("전송 완료 (수신 데이터 무시)\n");
        for (int i = 0; i < 3; i++) {
            printf("0x%02X ", write_rx[i]);
        }
    }
    printf("\n");

    // SPI 종료
    spi_close(&spi);

    printf("=== 프로그램 종료 ===\n");
    return 0;
}