#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "lepton.h"
#include "ringbuffer.h"

LeptonRingBuffer lepton_ring_buffer = { .head = 0, .tail = 0, .count = 0 };
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

// 카메라에서 이미지를 캡쳐해서 ring buffer에 이미지를 저장하는 것 까지 수행.
static void* lepton_capture_thread(void* arg) {
    int lepton_fd = init_lepton();
    int ret;
    uint16_t pure_img[LEPTON_HEIGHT][LEPTON_WIDTH];
    while(1)
    {
        ret = lepton_capture(lepton_fd);
        if (ret < 0) 
        {
            printf("Lepton 이미지 캡처 오류\n");
            continue;
        }
        get_image(pure_img);
        pthread_mutex_lock(&buffer_mutex);
        ret = lepton_ringbuffer_enqueue(&lepton_ring_buffer, pure_img);
        pthread_mutex_unlock(&buffer_mutex);
        if (!ret)
        {
            continue; // 버퍼가 가득 참
        }
        usleep(37000); // 약 27Hz, 꼭 필요한지 검토 필요.
    }
    cleanup_lepton(lepton_fd);
}

static void decompress_image(uint16_t flatten_image[], uint16_t original_image[][LEPTON_WIDTH])
{
    memcpy(flatten_image, &original_image[0][0], (size_t)(sizeof(uint16_t)*LEPTON_HEIGHT*LEPTON_WIDTH));
}

static void* lepton_transmit_thread(void* arg) {
    int ret;
    uint16_t transmit_image[LEPTON_HEIGHT][LEPTON_WIDTH];
    uint16_t flatten_image[LEPTON_HEIGHT * LEPTON_WIDTH];
    while(1)
    {
        pthread_mutex_lock(&buffer_mutex);
        ret = lepton_ringbuffer_dequeue(&lepton_ring_buffer, transmit_image);
        pthread_mutex_unlock(&buffer_mutex);
        if (ret == false)
        {
            usleep(37000);   // 27Hz에 맞춰서 sleep
            continue;
        }
        // MQTT로 transmit_image 바이트 배열 전송
        decompress_image(flatten_image, transmit_image);
        
    }

}

int main(void){
    int ret = 0;
    
    pthread_t lepton_capture_thread_id;
    pthread_t lepton_transmit_thread_id;
    pthread_create(&lepton_capture_thread_id, NULL, lepton_capture_thread, NULL);
    pthread_create(&lepton_transmit_thread_id, NULL, lepton_transmit_thread, NULL);

    pthread_join(lepton_capture_thread_id, NULL);
    pthread_join(lepton_transmit_thread_id, NULL);
    return 0;
}