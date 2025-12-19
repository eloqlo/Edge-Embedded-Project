/*
 * main.c - 탐색 로봇 제어 처리부 (Raspberry Pi 4B)
 *
 * 재난 구조용 열화상 기반 인명 탐색 로봇
 *
 * 주요 기능 (요구사항명세서 FR1~FR8):
 * - FR1: RGB 카메라 영상 수집 및 RTSP 스트리밍 (1080p, 30fps, H.264)
 * - FR2: 열화상 카메라 데이터 수집 및 스트리밍 (80x60, 8Hz)
 * - FR3: 음성 수집 및 전송 (16kHz, 16-bit Mono)
 * - FR4: 구조대원 음성 수신 및 스피커 재생
 * - FR5: 센서 데이터 수신 및 처리 (UART 115200, 10Hz)
 * - FR6: 모터 제어 명령 수신 및 PWM 변환
 * - FR7: 자율 장애물 회피 (초음파 < 7cm 시 전진 차단)
 * - FR8: 모니터링 서버와의 통신 (TCP/IP, JSON)
 *
 * Protocol (docs/Protocol.md):
 * - COMMAND: 제어 명령 수신 (DRIVE, MIC, OBJECT_DETECTION, SYSTEM)
 * - TELEMETRY: 센서 데이터 송신 (co_ppm, obstacle_cm, rollover)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#include "lepton.h"
#include "camera.h"
#include "network.h"
#include "sensor.h"

#define SERVER_IP "192.168.0.100"  // Jetson Nano IP (설정 필요)
#define MAIN_LOOP_INTERVAL_US 100000  // 100ms (10Hz)

static volatile bool g_running = true;
static int g_lepton_fd = -1;
static pthread_mutex_t g_sensor_mutex = PTHREAD_MUTEX_INITIALIZER;
static sensor_data_t g_latest_sensor_data;
static bool g_obstacle_block_forward = false;

static void signal_handler(int sig)
{
    (void)sig;
    printf("\n종료 신호 수신, 시스템 종료 중...\n");
    g_running = false;
}

static void setup_signal_handlers(void)
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/*
 * 명령 처리 콜백 함수
 * Protocol.md 2.2 제어 명령 처리
 */
static void command_handler(command_type_t cmd)
{
    switch (cmd) {
    case CMD_DRIVE_FORWARD:
        // FR7: 장애물 감지 시 전진 차단
        if (g_obstacle_block_forward) {
            printf("[경고] 장애물 감지 - 전진 명령 차단됨\n");
            return;
        }
        printf("[제어] 전진\n");
        send_motor_command(0, 'F', 200);
        break;

    case CMD_DRIVE_BACKWARD:
        printf("[제어] 후진\n");
        send_motor_command(0, 'B', 200);
        break;

    case CMD_DRIVE_LEFT:
        printf("[제어] 좌회전\n");
        send_motor_command(0, 'L', 150);
        break;

    case CMD_DRIVE_RIGHT:
        printf("[제어] 우회전\n");
        send_motor_command(0, 'R', 150);
        break;

    case CMD_DRIVE_STOP:
        printf("[제어] 정지\n");
        send_motor_command(0, 'S', 0);
        break;

    case CMD_MIC_ON:
        printf("[기능] 마이크 스트리밍 시작\n");
        // TODO: 음성 스트리밍 시작
        break;

    case CMD_MIC_OFF:
        printf("[기능] 마이크 스트리밍 종료\n");
        // TODO: 음성 스트리밍 종료
        break;

    case CMD_OBJECT_DETECTION_ON:
        printf("[기능] 객체 탐지 활성화\n");
        break;

    case CMD_OBJECT_DETECTION_OFF:
        printf("[기능] 객체 탐지 비활성화\n");
        break;

    case CMD_SYSTEM_REBOOT:
        printf("[시스템] 재부팅 명령 수신\n");
        g_running = false;
        system("sudo reboot");
        break;

    default:
        printf("[경고] 알 수 없는 명령: %d\n", cmd);
        break;
    }
}

/*
 * 센서 데이터 콜백 함수
 * FR5: 센서 데이터 수신 및 처리
 * FR7: 자율 장애물 회피
 */
static void sensor_handler(const sensor_data_t *data)
{
    pthread_mutex_lock(&g_sensor_mutex);
    memcpy(&g_latest_sensor_data, data, sizeof(sensor_data_t));
    pthread_mutex_unlock(&g_sensor_mutex);

    // FR7.1~7.4: 장애물 감지 및 전진 차단
    if (data->distance_cm < OBSTACLE_THRESHOLD_CM && data->distance_cm > 0) {
        if (!g_obstacle_block_forward) {
            g_obstacle_block_forward = true;
            printf("[경고] 장애물 감지: %dcm - 전진 차단 활성화\n", data->distance_cm);
            // 즉시 정지 명령
            send_motor_command(0, 'S', 0);
        }
    } else {
        if (g_obstacle_block_forward) {
            g_obstacle_block_forward = false;
            printf("[정보] 장애물 해제 - 전진 차단 비활성화\n");
        }
    }
}

/*
 * 텔레메트리 데이터 생성 및 전송
 * Protocol.md 2.3 센서 데이터 (TELEMETRY)
 */
static int send_telemetry_data(void)
{
    telemetry_data_t telemetry;

    pthread_mutex_lock(&g_sensor_mutex);
    telemetry.co_ppm = g_latest_sensor_data.co_ppm;
    telemetry.obstacle_cm = g_latest_sensor_data.distance_cm;
    telemetry.rollover = is_rollover_detected();
    pthread_mutex_unlock(&g_sensor_mutex);

    return send_telemetry(&telemetry);
}

/*
 * 열화상 캡처 스레드
 * FR2: 열화상 카메라 데이터 수집 (8Hz)
 */
static void *thermal_capture_thread(void *arg)
{
    (void)arg;

    printf("[열화상] 캡처 스레드 시작 (8Hz)\n");

    while (g_running) {
        if (g_lepton_fd >= 0) {
            if (set_image(g_lepton_fd) < 0) {
                fprintf(stderr, "[열화상] 프레임 캡처 실패\n");
            }
        }
        usleep(125000);  // 8Hz = 125ms
    }

    printf("[열화상] 캡처 스레드 종료\n");
    return NULL;
}

/*
 * 시스템 초기화
 */
static int initialize_system(void)
{
    printf("========================================\n");
    printf("  재난 구조용 열화상 기반 인명 탐색 로봇\n");
    printf("  탐색 로봇 제어 처리부 (Raspberry Pi 4B)\n");
    printf("========================================\n\n");

    // FR2.1: 열화상 카메라 초기화 (SPI 10MHz, CS=0)
    printf("[초기화] 열화상 카메라 (Lepton)...\n");
    g_lepton_fd = init_lepton();
    if (g_lepton_fd < 0) {
        fprintf(stderr, "[오류] 열화상 카메라 초기화 실패\n");
        return -1;
    }
    printf("[완료] 열화상 카메라 초기화 성공 (fd=%d)\n", g_lepton_fd);

    // FR1: RGB 카메라 초기화
    printf("[초기화] RGB 카메라...\n");
    if (init_camera() < 0) {
        fprintf(stderr, "[오류] RGB 카메라 초기화 실패\n");
        close_lepton(g_lepton_fd);
        return -1;
    }
    printf("[완료] RGB 카메라 초기화 성공\n");

    // FR8: 네트워크 초기화 (Jetson Nano와 WiFi 통신)
    printf("[초기화] 네트워크 (서버: %s:%d)...\n", SERVER_IP, SERVER_PORT);
    if (init_network(SERVER_IP) < 0) {
        fprintf(stderr, "[오류] 네트워크 초기화 실패\n");
        cleanup_camera();
        close_lepton(g_lepton_fd);
        return -1;
    }
    printf("[완료] 네트워크 초기화 성공\n");

    // FR5.1: 센서 허브 초기화 (UART 115200, 8N1)
    printf("[초기화] 센서 허브 (STM32)...\n");
    if (init_sensors() < 0) {
        fprintf(stderr, "[오류] 센서 초기화 실패\n");
        cleanup_network();
        cleanup_camera();
        close_lepton(g_lepton_fd);
        return -1;
    }
    printf("[완료] 센서 허브 초기화 성공\n");

    return 0;
}

/*
 * 시스템 정리
 */
static void cleanup_system(void)
{
    printf("\n[정리] 시스템 종료 중...\n");

    stop_sensor_thread();
    stop_network_thread();
    stop_rgb_streaming();
    stop_thermal_streaming();

    cleanup_sensors();
    cleanup_network();
    cleanup_camera();

    if (g_lepton_fd >= 0) {
        close_lepton(g_lepton_fd);
    }

    pthread_mutex_destroy(&g_sensor_mutex);

    printf("[완료] 시스템 종료 완료\n");
}

/*
 * 메인 함수
 */
int main(void)
{
    pthread_t thermal_thread;
    struct timespec last_telemetry_time;
    struct timespec current_time;

    setup_signal_handlers();

    // 시스템 초기화
    if (initialize_system() < 0) {
        fprintf(stderr, "[치명적 오류] 시스템 초기화 실패\n");
        return -1;
    }

    // 비동기 스레드 시작
    printf("\n[시작] 비동기 처리 스레드 시작...\n");

    // 센서 수신 스레드 (FR5)
    if (start_sensor_thread(sensor_handler) < 0) {
        fprintf(stderr, "[오류] 센서 스레드 시작 실패\n");
        cleanup_system();
        return -1;
    }

    // 네트워크 수신 스레드 (FR6, FR8)
    if (start_network_thread(command_handler) < 0) {
        fprintf(stderr, "[오류] 네트워크 스레드 시작 실패\n");
        cleanup_system();
        return -1;
    }

    // 열화상 캡처 스레드 (FR2)
    if (pthread_create(&thermal_thread, NULL, thermal_capture_thread, NULL) != 0) {
        fprintf(stderr, "[오류] 열화상 스레드 시작 실패\n");
        cleanup_system();
        return -1;
    }

    // RTSP 스트리밍 시작 (FR1.3, FR2.2)
    printf("[시작] RTSP 스트리밍...\n");
    start_rgb_streaming("rtsp://0.0.0.0:8554/rgb");
    start_thermal_streaming("rtsp://0.0.0.0:8554/thermal");

    printf("\n========================================\n");
    printf("  시스템 준비 완료 - 메인 루프 시작\n");
    printf("  종료: Ctrl+C\n");
    printf("========================================\n\n");

    clock_gettime(CLOCK_MONOTONIC, &last_telemetry_time);

    // 메인 루프
    while (g_running) {
        clock_gettime(CLOCK_MONOTONIC, &current_time);

        // FR5.4: 텔레메트리 송신 (10Hz)
        long elapsed_ms = (current_time.tv_sec - last_telemetry_time.tv_sec) * 1000 +
                          (current_time.tv_nsec - last_telemetry_time.tv_nsec) / 1000000;

        if (elapsed_ms >= TELEMETRY_INTERVAL_MS) {
            send_telemetry_data();
            last_telemetry_time = current_time;
        }

        // FR8.2: 연결 상태 확인 및 재연결
        if (!is_connected()) {
            printf("[네트워크] 연결 끊김 - 재연결 시도...\n");
            reconnect_to_server();
        }

        usleep(MAIN_LOOP_INTERVAL_US);
    }

    // 열화상 스레드 종료 대기
    pthread_join(thermal_thread, NULL);

    // 시스템 정리
    cleanup_system();

    return 0;
}
