/*
 * sensor.h - 센서 허브 통신 모듈
 *
 * STM32F103RB(센서 허브)와 UART 통신
 * - 보드레이트: 115200
 * - 데이터 형식: 8N1
 * - 수신 주기: 100ms (10Hz)
 *
 * 센서 데이터:
 * - 초음파 센서 (HC-SR04): 거리 측정 (cm)
 * - CO 센서: 일산화탄소 농도 (ppm)
 * - IMU 센서: 3축 자이로/가속도
 */
#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <stdbool.h>

#define SENSOR_UART_DEVICE "/dev/ttyAMA0"
#define SENSOR_UART_BAUDRATE 115200
#define SENSOR_UPDATE_INTERVAL_MS 100  // 10Hz

#define OBSTACLE_THRESHOLD_CM 7  // 장애물 경고 거리

typedef struct {
    int distance_cm;      // 초음파 센서 거리 (cm)
    int co_ppm;           // CO 농도 (ppm)
    float gyro_x;         // 자이로 X축 (°/s)
    float gyro_y;         // 자이로 Y축 (°/s)
    float gyro_z;         // 자이로 Z축 (°/s)
    float accel_x;        // 가속도 X축 (m/s²)
    float accel_y;        // 가속도 Y축 (m/s²)
    float accel_z;        // 가속도 Z축 (m/s²)
    uint32_t timestamp;   // 타임스탬프 (ms)
    uint16_t seq_num;     // 시퀀스 번호
} sensor_data_t;

typedef void (*sensor_callback_t)(const sensor_data_t *data);

// 센서 초기화 및 종료
int init_sensors(void);
void cleanup_sensors(void);

// 센서 데이터 읽기
int read_sensor_data(sensor_data_t *data);
int get_latest_sensor_data(sensor_data_t *data);

// 장애물 감지
bool is_obstacle_detected(void);
int get_obstacle_distance(void);

// 전복 감지
bool is_rollover_detected(void);

// 비동기 센서 읽기 스레드
int start_sensor_thread(sensor_callback_t callback);
void stop_sensor_thread(void);

// 모터 제어 명령 전송 (센서 허브로)
int send_motor_command(uint8_t motor_id, uint8_t action, uint8_t speed);

#endif /* SENSOR_H */
