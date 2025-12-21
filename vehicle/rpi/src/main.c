#include <stdio.h>
#include <pthread.h>

#include "lepton.h"
#include "ringbuffer.h"

LeptonRingBuffer lepton_ring_buffer = { .head = 0, .tail = 0, .count = 0 };

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
        
        ret = lepton_ringbuffer_enqueue(&lepton_ring_buffer, pure_img);
        if (!ret)
        {
            continue; // 버퍼가 가득 참
        }

        usleep(37000); // 약 27Hz
    }
    cleanup_lepton(lepton_fd);
}

static void* lepton_transmit_thread(void* arg) {

    int ret;
    uint16_t transmit_image[LEPTON_HEIGHT][LEPTON_WIDTH];

    while(1)
    {
        ret = lepton_ringbuffer_dequeue(&lepton_ring_buffer, transmit_image);
        if (ret == false)
        {
            usleep(1000);   // 버퍼 비어있으니 1ms 대기
            continue;
        }
        
        // TODO transmit_image 전송 코드 작성
        // TODO Critical Section 처리 해야하나? --> multi-thread 다중 함수 동작 방식 명확하게 파악.
    }

}

int main(void){
    int ret = 0;
    

    pthread_t lepton_capture_thread_id;
    pthread_t lepton_transmit_thread_id;
    pthread_create(&lepton_capture_thread_id, NULL, lepton_capture_thread, NULL);
    pthread_create(&lepton_transmit_thread_id, NULL, lepton_transmit_thread, NULL);

    // TODO 적절한 종료 조건 추가 필요

    pthread_join(lepton_capture_thread_id, NULL);
    pthread_join(lepton_transmit_thread_id, NULL);
    return 0;
}