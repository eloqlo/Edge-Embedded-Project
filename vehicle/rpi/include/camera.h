/*
 * camera.h - RGB 카메라 모듈
 *
 * CSI 포트 RGB 카메라 제어
 * - 해상도: 1920x1080 (1080p)
 * - 프레임레이트: 30fps
 * - 인코딩: H.264 (4Mbps)
 * - 스트리밍: RTSP
 */
#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>
#include <stdbool.h>

#define CAMERA_WIDTH 1920
#define CAMERA_HEIGHT 1080
#define CAMERA_FPS 30
#define CAMERA_BITRATE 4000000  // 4Mbps
#define RTSP_PORT 8554

// RGB 카메라 초기화 및 종료
int init_camera(void);
void cleanup_camera(void);

// RTSP 스트리밍 제어
int start_rgb_streaming(const char *rtsp_url);
void stop_rgb_streaming(void);
bool is_rgb_streaming(void);

// 열화상 RTSP 스트리밍 (Lepton 데이터 기반)
int start_thermal_streaming(const char *rtsp_url);
void stop_thermal_streaming(void);
bool is_thermal_streaming(void);

// 프레임 캡처 (디버깅용)
int capture_rgb_frame(uint8_t *buffer, size_t size);

#endif /* CAMERA_H */
