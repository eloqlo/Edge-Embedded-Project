/*
 * lepton_stream.c - Lepton 2.5 실시간 열화상 스트리밍 프로그램
 *
 * 27fps 실시간 캡처 및 SDL2 기반 시각화
 *
 * 컴파일/실행:
 * gcc -o lepton_stream lepton_stream.c -lSDL2 -lpigpio -lm
 * sudo ./lepton_stream
 *
 * 조작법:
 *   ESC/q: 종료
 *   s: 현재 프레임 PGM 저장
 *   c: 컬러맵 변경 (IRON/RAINBOW/GRAYSCALE)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <SDL2/SDL.h>

// ==================== 설정 ====================
#define LEPTON_WIDTH    80
#define LEPTON_HEIGHT   60
#define DISPLAY_SCALE   8       // 80x60 -> 640x480
#define DISPLAY_WIDTH   (LEPTON_WIDTH * DISPLAY_SCALE)
#define DISPLAY_HEIGHT  (LEPTON_HEIGHT * DISPLAY_SCALE)

#define VOSPI_FRAME_SIZE    164
#define VOSPI_PACKETS_PER_FRAME 60

#define TARGET_FPS      27
#define FRAME_DELAY_MS  (1000 / TARGET_FPS)  // ~37ms

// SPI 설정
static const char *spi_device = "/dev/spidev0.0";
static uint8_t spi_mode = SPI_MODE_3;
static uint8_t spi_bits = 8;
static uint32_t spi_speed = 10000000;  // 10MHz

// ==================== 전역 변수 ====================
static volatile int running = 1;
static uint8_t frame_packet[VOSPI_FRAME_SIZE];
static uint16_t lepton_image[LEPTON_HEIGHT][LEPTON_WIDTH];
static uint16_t lepton_min = 0, lepton_max = 65535;

// 컬러맵 타입
typedef enum {
    COLORMAP_IRON = 0,
    COLORMAP_RAINBOW,
    COLORMAP_GRAYSCALE,
    COLORMAP_COUNT
} ColorMapType;

static ColorMapType current_colormap = COLORMAP_IRON;
static const char* colormap_names[] = {"IRON", "RAINBOW", "GRAYSCALE"};

// ==================== 컬러맵 정의 ====================
// IRON 컬러맵 (열화상 카메라 표준)
static const uint8_t iron_palette[256][3] = {
    {0,0,0}, {0,0,9}, {2,0,16}, {4,0,24}, {6,0,31}, {8,0,38}, {10,0,45}, {12,0,53},
    {14,0,60}, {17,0,67}, {19,0,74}, {21,0,82}, {23,0,89}, {25,0,96}, {27,0,103}, {29,0,111},
    {31,0,118}, {36,0,120}, {41,0,121}, {46,0,122}, {51,0,123}, {56,0,124}, {61,0,125}, {66,0,126},
    {71,0,128}, {76,1,129}, {81,1,130}, {86,2,131}, {91,2,132}, {96,3,133}, {101,3,134}, {106,4,135},
    {111,4,137}, {115,5,137}, {119,6,138}, {123,7,138}, {127,8,139}, {131,9,139}, {135,10,140}, {139,11,140},
    {143,12,141}, {147,13,141}, {151,14,142}, {155,15,142}, {159,16,143}, {163,17,143}, {167,18,144}, {171,19,144},
    {175,20,145}, {178,21,145}, {181,22,145}, {184,23,145}, {187,24,145}, {190,25,145}, {193,26,145}, {196,27,145},
    {199,28,146}, {202,29,146}, {205,30,146}, {208,31,146}, {211,32,146}, {214,33,146}, {217,34,146}, {220,35,146},
    {224,36,147}, {224,38,144}, {225,40,142}, {225,42,139}, {226,44,137}, {226,46,134}, {227,48,132}, {227,50,129},
    {228,52,127}, {228,54,124}, {229,56,122}, {229,58,119}, {230,60,117}, {230,62,114}, {231,64,112}, {231,66,109},
    {232,68,107}, {232,70,105}, {233,73,102}, {233,75,100}, {234,77,97}, {234,79,95}, {235,81,92}, {235,83,90},
    {236,85,87}, {236,87,85}, {237,89,82}, {237,91,80}, {238,93,77}, {238,95,75}, {239,97,72}, {239,99,70},
    {240,101,67}, {240,104,66}, {241,106,64}, {241,109,63}, {242,111,62}, {242,114,60}, {243,116,59}, {243,119,58},
    {244,121,56}, {244,124,55}, {245,126,54}, {245,129,52}, {246,131,51}, {246,134,50}, {247,136,48}, {247,139,47},
    {248,141,46}, {248,144,45}, {248,147,44}, {249,149,43}, {249,152,42}, {249,155,41}, {250,157,40}, {250,160,39},
    {250,163,38}, {251,165,38}, {251,168,37}, {251,171,36}, {252,173,35}, {252,176,34}, {252,179,33}, {253,181,32},
    {253,184,32}, {253,187,31}, {253,190,31}, {253,192,30}, {254,195,30}, {254,198,29}, {254,201,29}, {254,203,28},
    {254,206,28}, {254,209,27}, {255,211,27}, {255,214,27}, {255,217,26}, {255,219,26}, {255,222,25}, {255,225,25},
    {255,227,25}, {255,229,25}, {255,231,24}, {255,233,24}, {255,235,24}, {255,237,24}, {255,239,24}, {255,241,24},
    {255,243,24}, {255,245,23}, {255,247,23}, {255,249,23}, {255,250,23}, {255,251,24}, {255,252,24}, {255,253,25},
    {255,253,25}, {255,254,26}, {255,254,26}, {255,255,27}, {255,255,28}, {255,255,29}, {255,255,30}, {255,255,31},
    {255,255,32}, {255,255,33}, {255,255,34}, {255,255,35}, {255,255,36}, {255,255,37}, {255,255,38}, {255,255,40},
    {255,255,41}, {255,255,42}, {255,255,44}, {255,255,45}, {255,255,47}, {255,255,48}, {255,255,50}, {255,255,51},
    {255,255,53}, {255,255,55}, {255,255,57}, {255,255,59}, {255,255,61}, {255,255,63}, {255,255,65}, {255,255,67},
    {255,255,69}, {255,255,71}, {255,255,73}, {255,255,76}, {255,255,78}, {255,255,81}, {255,255,83}, {255,255,86},
    {255,255,88}, {255,255,91}, {255,255,94}, {255,255,97}, {255,255,100}, {255,255,103}, {255,255,106}, {255,255,109},
    {255,255,112}, {255,255,115}, {255,255,118}, {255,255,121}, {255,255,125}, {255,255,128}, {255,255,132}, {255,255,135},
    {255,255,139}, {255,255,143}, {255,255,147}, {255,255,151}, {255,255,155}, {255,255,159}, {255,255,163}, {255,255,167},
    {255,255,171}, {255,255,175}, {255,255,180}, {255,255,184}, {255,255,189}, {255,255,193}, {255,255,198}, {255,255,203},
    {255,255,207}, {255,255,212}, {255,255,217}, {255,255,222}, {255,255,227}, {255,255,232}, {255,255,237}, {255,255,242},
    {255,255,247}, {255,255,249}, {255,255,251}, {255,255,253}, {255,255,254}, {255,255,255}, {255,255,255}, {255,255,255},
    {255,255,255}, {255,255,255}, {255,255,255}, {255,255,255}, {255,255,255}, {255,255,255}, {255,255,255}, {255,255,255}
};

// ==================== 함수 선언 ====================
static void signal_handler(int sig);
static int spi_init(void);
static int read_frame(int fd);
static void apply_colormap(uint16_t value, uint8_t *r, uint8_t *g, uint8_t *b);
static void render_frame(SDL_Renderer *renderer, SDL_Texture *texture);
static void save_pgm(void);
static void print_usage(void);

// ==================== Signal Handler ====================
static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

// ==================== SPI 초기화 ====================
static int spi_init(void) {
    int fd = open(spi_device, O_RDWR);
    if (fd < 0) {
        perror("SPI 장치 열기 실패");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_MODE, &spi_mode) < 0 ||
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bits) < 0 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed) < 0) {
        perror("SPI 설정 실패");
        close(fd);
        return -1;
    }

    printf("SPI 초기화 완료: mode=%d, bits=%d, speed=%dMHz\n",
           spi_mode, spi_bits, spi_speed / 1000000);
    return fd;
}

// ==================== 프레임 읽기 ====================
// 동기화 상태 추적
static int synced = 0;

static int read_frame(int fd) {
    uint8_t tx[VOSPI_FRAME_SIZE] = {0};
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)frame_packet,
        .len = VOSPI_FRAME_SIZE,
        .speed_hz = spi_speed,
        .bits_per_word = spi_bits,
    };

    int discard_count = 0;
    const int max_discards = 200;
    int frame_started = 0;
    int last_packet = -1;

    // packet 0부터 59까지 순차적으로 읽기
    while (1) {
        if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
            perror("SPI read failed");
            return -1;
        }

        // discard 패킷 체크
        if ((frame_packet[0] & 0x0F) == 0x0F) {
            discard_count++;
            if (discard_count > max_discards) {
                synced = 0;
                return -2;
            }
            continue;
        }

        uint8_t packet_num = frame_packet[1];

        if (packet_num >= VOSPI_PACKETS_PER_FRAME) {
            continue;
        }

        // 픽셀 데이터 저장 (무조건 저장)
        for (int i = 0; i < LEPTON_WIDTH; i++) {
            lepton_image[packet_num][i] =
                (frame_packet[2*i + 4] << 8) | frame_packet[2*i + 5];
        }

        // packet 0 감지 → 새 프레임 시작
        if (packet_num == 0) {
            frame_started = 1;
            if (!synced) {
                printf("동기화 완료!\n");
                synced = 1;
            }
        }

        // packet 59 감지 → 프레임 완성
        if (frame_started && packet_num == 59) {
            return 0;  // 프레임 완성!
        }

        last_packet = packet_num;
        discard_count = 0;  // 유효 패킷 받으면 리셋
    }

    return -1;
}

// ==================== AGC (Automatic Gain Control) ====================
static void calculate_agc(void) {
    uint16_t min_val = 65535, max_val = 0;

    for (int y = 0; y < LEPTON_HEIGHT; y++) {
        for (int x = 0; x < LEPTON_WIDTH; x++) {
            uint16_t val = lepton_image[y][x];
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }
    }

    // 부드러운 전환을 위해 이동 평균 적용
    lepton_min = (lepton_min * 7 + min_val) / 8;
    lepton_max = (lepton_max * 7 + max_val) / 8;

    // 최소 범위 보장
    if (lepton_max - lepton_min < 100) {
        lepton_max = lepton_min + 100;
    }
}

// ==================== 컬러맵 적용 ====================
static void apply_colormap(uint16_t value, uint8_t *r, uint8_t *g, uint8_t *b) {
    // 정규화 (0-255)
    uint32_t range = lepton_max - lepton_min;
    if (range == 0) range = 1;

    uint32_t normalized = ((uint32_t)(value - lepton_min) * 255) / range;
    if (normalized > 255) normalized = 255;

    uint8_t idx = (uint8_t)normalized;

    switch (current_colormap) {
        case COLORMAP_IRON:
            *r = iron_palette[idx][0];
            *g = iron_palette[idx][1];
            *b = iron_palette[idx][2];
            break;

        case COLORMAP_RAINBOW:
            // HSV -> RGB (Hue = normalized value)
            {
                float h = (float)idx / 255.0f * 300.0f;  // 0-300도 (보라~빨강)
                float s = 1.0f, v = 1.0f;
                float c = v * s;
                float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
                float m = v - c;
                float rf, gf, bf;

                if (h < 60)       { rf = c; gf = x; bf = 0; }
                else if (h < 120) { rf = x; gf = c; bf = 0; }
                else if (h < 180) { rf = 0; gf = c; bf = x; }
                else if (h < 240) { rf = 0; gf = x; bf = c; }
                else if (h < 300) { rf = x; gf = 0; bf = c; }
                else              { rf = c; gf = 0; bf = x; }

                *r = (uint8_t)((rf + m) * 255);
                *g = (uint8_t)((gf + m) * 255);
                *b = (uint8_t)((bf + m) * 255);
            }
            break;

        case COLORMAP_GRAYSCALE:
        default:
            *r = *g = *b = idx;
            break;
    }
}

// ==================== 프레임 렌더링 ====================
static void render_frame(SDL_Renderer *renderer, SDL_Texture *texture) {
    uint32_t *pixels;
    int pitch;

    SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch);

    // AGC 계산
    calculate_agc();

    // 픽셀 데이터 생성 (스케일링 포함)
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        int src_y = y / DISPLAY_SCALE;
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            int src_x = x / DISPLAY_SCALE;

            uint8_t r, g, b;
            apply_colormap(lepton_image[src_y][src_x], &r, &g, &b);

            pixels[y * (pitch / 4) + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }

    SDL_UnlockTexture(texture);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

// ==================== PGM 저장 ====================
static void save_pgm(void) {
    char filename[64];
    int index = 0;

    do {
        snprintf(filename, sizeof(filename), "STREAM_%.4d.pgm", index++);
    } while (access(filename, F_OK) == 0 && index < 10000);

    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("파일 저장 실패: %s\n", filename);
        return;
    }

    uint16_t min_val = 65535, max_val = 0;
    for (int y = 0; y < LEPTON_HEIGHT; y++) {
        for (int x = 0; x < LEPTON_WIDTH; x++) {
            if (lepton_image[y][x] < min_val) min_val = lepton_image[y][x];
            if (lepton_image[y][x] > max_val) max_val = lepton_image[y][x];
        }
    }

    fprintf(f, "P2\n%d %d\n%u\n", LEPTON_WIDTH, LEPTON_HEIGHT, max_val - min_val);
    for (int y = 0; y < LEPTON_HEIGHT; y++) {
        for (int x = 0; x < LEPTON_WIDTH; x++) {
            fprintf(f, "%u ", lepton_image[y][x] - min_val);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    printf("저장됨: %s\n", filename);
}

// ==================== 사용법 출력 ====================
static void print_usage(void) {
    printf("\n=== Lepton 2.5 실시간 열화상 스트리밍 ===\n");
    printf("조작법:\n");
    printf("  ESC/q : 종료\n");
    printf("  s     : 현재 프레임 PGM 저장\n");
    printf("  c     : 컬러맵 변경 (IRON/RAINBOW/GRAYSCALE)\n");
    printf("==========================================\n\n");
}

// ==================== 메인 ====================
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    print_usage();

    // 시그널 핸들러 등록
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // SPI 초기화
    int spi_fd = spi_init();
    if (spi_fd < 0) {
        return 1;
    }

    // 동기화 (185ms 이상 대기)
    printf("Lepton 동기화 중...\n");
    usleep(300000);

    // SDL 초기화
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL 초기화 실패: %s\n", SDL_GetError());
        close(spi_fd);
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Lepton 2.5 Thermal Stream",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        DISPLAY_WIDTH, DISPLAY_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        printf("윈도우 생성 실패: %s\n", SDL_GetError());
        SDL_Quit();
        close(spi_fd);
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        DISPLAY_WIDTH, DISPLAY_HEIGHT);

    printf("스트리밍 시작 (목표: %d FPS)\n", TARGET_FPS);

    // 초기 화면 표시 (검은 화면으로 창 먼저 띄우기)
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    printf("창 표시됨. Lepton 프레임 대기 중...\n");

    // FPS 계산용 변수
    Uint32 frame_count = 0;
    Uint32 fps_timer = SDL_GetTicks();
    float current_fps = 0.0f;
    char title[128];
    int resync_count = 0;

    // 메인 루프
    while (running) {
        Uint32 frame_start = SDL_GetTicks();

        // 이벤트 처리 (항상 처리해서 창이 응답하도록)
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                    case SDLK_q:
                        running = 0;
                        break;
                    case SDLK_s:
                        save_pgm();
                        break;
                    case SDLK_c:
                        current_colormap = (current_colormap + 1) % COLORMAP_COUNT;
                        printf("컬러맵: %s\n", colormap_names[current_colormap]);
                        break;
                }
            }
        }

        // 프레임 읽기
        int ret = read_frame(spi_fd);
        if (ret == 0) {
            render_frame(renderer, texture);
            frame_count++;
            resync_count = 0;
        } else if (ret == -2) {
            // 재동기화 필요
            resync_count++;
            if (resync_count > 5) {
                printf("재동기화 시도 (%d)...\n", resync_count);
                usleep(185000);  // 185ms 대기 (VoSPI 스펙)
                resync_count = 0;
            }
        }

        // FPS 계산 및 표시 (1초마다 갱신)
        Uint32 now = SDL_GetTicks();
        if (now - fps_timer >= 1000) {
            current_fps = (float)frame_count * 1000.0f / (now - fps_timer);
            snprintf(title, sizeof(title),
                "Lepton 2.5 Thermal Stream - %.1f FPS [%s] (Min:%u Max:%u)",
                current_fps, colormap_names[current_colormap], lepton_min, lepton_max);
            SDL_SetWindowTitle(window, title);

            // 콘솔에도 상태 출력
            if (frame_count == 0) {
                printf("프레임 수신 대기 중... (동기화 상태: %s)\n",
                       synced ? "OK" : "대기");
            }

            frame_count = 0;
            fps_timer = now;
        }

        // 프레임 타이밍 조절
        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_DELAY_MS) {
            SDL_Delay(FRAME_DELAY_MS - frame_time);
        }
    }

    printf("\n스트리밍 종료\n");

    // 정리
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close(spi_fd);

    return 0;
}
