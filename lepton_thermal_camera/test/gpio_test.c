/*
    2025.12.05 작성: GPIO 21번 핀을 HIGH로 설정하는 예제 코드
    이 코드는 libgpiod 라이브러리를 사용하여 GPIO 21번 핀을 HIGH 상태로 설정합니다.
    5초 후에 LOW 상태로 변경하고 프로그램을 종료합니다.

    libgpiod 공식 도큐먼트 : https://libgpiod.readthedocs.io/en/latest/core_line_settings.html
*/

#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>

int main(void){
    struct gpiod_chip *chip;
    struct gpiod_line_request *request;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    struct gpiod_request_config *req_cfg;
    unsigned int offset = 21; // GPIO 21번 핀
    int ret;

    // 1. GPIO 칩 열기
    chip = gpiod_chip_open("/dev/gpiochip0");
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
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    // 3. 라인 설정 구성
    line_cfg = gpiod_line_config_new();
    if (!line_cfg) {
        perror("gpiod_line_config_new 실패\n");
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return 1;
    }
    gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings); // 1개의 라인 설정 추가

    // 4. 요청 구성 생성
    req_cfg = gpiod_request_config_new();
    if (!req_cfg) {
        perror("gpiod_request_config_new 실패\n");
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return 1;
    }
    gpiod_request_config_set_consumer(req_cfg, "gpio21_example");

    // 5. GPIO 라인 요청
    request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
     if (!request) {
        perror("gpiod_chip_request_lines 실패");
        gpiod_request_config_free(req_cfg);
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return 1;
    }
    printf("GPIO 21번을 HIGH로 설정합니다.\n");

    // 6. GPIO 21번 핀을 HIGH로 설정
    ret = gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_ACTIVE);
    if (ret < 0) {
        perror("gpiod_line_request_set_value 실패");
        gpiod_line_request_release(request);
        gpiod_request_config_free(req_cfg);
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return 1;
    }

     printf("GPIO 21번이 HIGH 상태입니다. 5초 동안 유지합니다.\n");
     sleep(5);

     // GPIO 21번을 LOW로 설정
    printf("GPIO 21번을 LOW로 설정합니다.\n");
    gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_INACTIVE);

    // 리소스 해제
    gpiod_line_request_release(request);
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    gpiod_chip_close(chip);

    printf("프로그램 종료\n");
    return 0;
}