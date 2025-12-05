#include <gpiod.h>          // GPIO 제어를 위한 libgpiod 헤더
#include <stdio.h>          // printf(), perror() 함수
#include <stdlib.h>         
#include <stdint.h>         // uint8_t, uint32_t 등
#include <string.h>         // memset() 함수
#include <fcntl.h>          // open() 함수
#include <unistd.h>         // close(), read(), write() 함수
#include <sys/ioctl.h>      // ioctl() 함수
#include <linux/spi/spidev.h>  // SPI 관련 정의

// Lepton 2.5 Thermal Camera Start-up
int lepton_startup(void) 
{
    printf("=== Lepton 2.5 Thermal Camera Start-up Begin ===\n");
    
    struct gpiod_chip *chip;
    struct gpiod_line_request *request;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    struct gpiod_request_config *req_cfg;
    unsigned int reset_offset = 20;
    unsigned int power_offset = 21;
    int ret;

    // 1. GPIO 칩 열기
    chip = gpiod_chip_open("/dev/gpiochip0");   //TODO gpiochip 번호 확인
    if (!chip) {
        perror("gpiod_chip_open 실패\n");
        return 1;
    }
    
    // 2. 라인 설정 생성(출력 모드)
    settings = gpiod_line_settings_new();
    if (!settings) {
        perror("gpiod_line_settings_new 실패\n");
        gpiod_chip_close(chip);
        return 1;
    }
    //TODO settings 뭐뭐 추가해야할지 직접 확인하기 + 이 Setting 맞는지 확인하기
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    // 3. 라인 설정 구성
    line_cfg = gpiod_line_config_new(); //TODO 이게 뭔지 확인하기
    if (!line_cfg) {
        perror("gpiod_line_config_new 실패\n");
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return 1;
    }
    gpiod_line_config_add_line_settings(line_cfg, &power_offset, 1, settings); //TODO 이게 뭔지 확인하기

    // 4. 요청 구성 생성
    req_cfg = gpiod_request_config_new();   //TODO 이게 뭔지 확인하기
    if (!req_cfg) {
        perror("gpiod_request_config_new 실패\n");
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return 1;
    }
    gpiod_request_config_set_consumer(req_cfg, "gpio21_example");   //TODO 이뭔확

    // 5. GPIO 라인 요청
    request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);    //TODO
     if (!request) {
        perror("gpiod_chip_request_lines 실패");
        gpiod_request_config_free(req_cfg);
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return 1;
    }
    printf("[*] PWR_DWN_L 핀을 High로 설정합니다...\n");

    // 6. PWR_DWN_L 핀을 HIGH로 설정
    ret = gpiod_line_request_set_value(request, power_offset, GPIOD_LINE_VALUE_ACTIVE);     // TODO
    if (ret < 0) {
        perror("gpiod_line_request_set_value 실패");
        gpiod_line_request_release(request);
        gpiod_request_config_free(req_cfg);
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return 1;
    }

     // RESET_L 핀을 LOW로 설정
     printf("[*] RESET_L 핀을 LOW로 설정합니다.\n");
     //TODO RESET_L 핀 Low주는거 반복하기

    printf("[*] SCLK을 인가합니다.\n");
    // Todo: SCLK 인가 코드 작성 필요

    printf("[*] 1sec 대기합니다.\n");
    sleep(1);

    printf("[*] RESET_L 핀을 HIGH로 설정합니다.\n");
    gpiod_line_request_set_value(request, reset_offset, GPIOD_LINE_VALUE_ACTIVE);
    printf("=== Lepton 2.5 Thermal Camera Start-up Complete ===\n\n");

    // 리소스 해제
    gpiod_line_request_release(request);
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    gpiod_chip_close(chip);

    printf("프로그램 종료\n");
    return 0;
}


