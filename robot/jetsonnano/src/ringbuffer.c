#include "../include/ringbuffer.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>     // assert()

#define DEBUG



int lepton_ringbuffer_is_available(LeptonRingBuffer* rb)
{
    return (rb->count < BUFFER_SIZE) ? 1 : 0;
}

int lepton_ringbuffer_is_empty(LeptonRingBuffer* rb)
{
    return (rb->count == 0) ? 1 : 0;
}

int lepton_ringbuffer_enqueue(LeptonRingBuffer* rb, const uint16_t image[][LEPTON_WIDTH])
{
    if (lepton_ringbuffer_is_available(rb))
    {
        #ifdef DEBUG
        printf("HELLO!\n");
        #endif
        memcpy(rb->buffer[rb->head], image, sizeof(uint16_t)*LEPTON_HEIGHT*(LEPTON_WIDTH));
        rb->head = (rb->head + OFFSET_SIZE) % (OFFSET_SIZE * BUFFER_SIZE);
        rb->count++;
        return 1;
    }
    else
    {
        printf("RingBuffer is full, cannot enqueue image.\n");
        return 0; // 버퍼가 가득 참
    }
}

int lepton_ringbuffer_dequeue(LeptonRingBuffer* rb, uint16_t image[][LEPTON_WIDTH])
{
    if (lepton_ringbuffer_is_empty(rb))
    {
        printf("RingBuffer is empty, cannot dequeue image.\n");
        return 0; // 버퍼가 비어 있음
    }
    else
    {
        memcpy(image, rb->buffer[rb->tail], sizeof(uint16_t)*LEPTON_HEIGHT*(LEPTON_WIDTH));
        rb->tail = (rb->tail + OFFSET_SIZE) % (OFFSET_SIZE * BUFFER_SIZE);
        rb->count--;
        return 1;
    }
}


// -------------------- DEBUG 함수 -------------------- //
static int print_ringbuffer_status(LeptonRingBuffer* rb)
{
    printf("---- RingBuffer Status ----\n");
    printf("RingBuffer Status: head=%zu, tail=%zu, count=%zu\n", rb->head, rb->tail, rb->count);
    printf("RingBuffer buffer data: %04X %04X %04X ...\n", 
        rb->buffer[rb->tail][0][0], 
        rb->buffer[rb->tail][0][1], 
        rb->buffer[rb->tail][0][2]);
    printf("--------------------------\n");
    return 1;
}

static int print_image_data(uint16_t image[LEPTON_HEIGHT][LEPTON_WIDTH])
{
    for (size_t i = 0; i < 3; i++) {
        for (size_t j = 0; j < 3; j++) {
            printf("%04X ", image[i][j]);
        }
        printf("\n");
    }
    return 1;
}