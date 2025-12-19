/*
 * network.h - 네트워크 통신 모듈
 *
 * Jetson Nano(모니터링 서버)와의 TCP/IP 소켓 통신
 * - 포트: 12345 (제어 및 센서 데이터)
 * - 프로토콜: JSON over TCP
 */
#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stdbool.h>

#define SERVER_PORT 12345
#define MAX_RECONNECT_ATTEMPTS 6
#define RECONNECT_INTERVAL_SEC 5
#define SENSOR_BUFFER_SIZE 50
#define TELEMETRY_INTERVAL_MS 100  // 10Hz

typedef enum {
    CMD_DRIVE_FORWARD,
    CMD_DRIVE_BACKWARD,
    CMD_DRIVE_LEFT,
    CMD_DRIVE_RIGHT,
    CMD_DRIVE_STOP,
    CMD_MIC_ON,
    CMD_MIC_OFF,
    CMD_OBJECT_DETECTION_ON,
    CMD_OBJECT_DETECTION_OFF,
    CMD_SYSTEM_REBOOT
} command_type_t;

typedef struct {
    int co_ppm;
    int obstacle_cm;
    bool rollover;
} telemetry_data_t;

typedef void (*command_callback_t)(command_type_t cmd);

// 네트워크 초기화 및 종료
int init_network(const char *server_ip);
void cleanup_network(void);

// 서버 연결 관리
int connect_to_server(void);
int reconnect_to_server(void);
bool is_connected(void);

// 데이터 송수신
int send_telemetry(const telemetry_data_t *data);
int receive_command(command_callback_t callback);

// 비동기 통신 스레드
int start_network_thread(command_callback_t callback);
void stop_network_thread(void);

#endif /* NETWORK_H */
