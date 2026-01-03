#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stdlib.h>

#include "lepton.h"

#define OFFSET_SIZE 2
#define BUFFER_SIZE 100

typedef struct {
    uint16_t buffer[OFFSET_SIZE * BUFFER_SIZE][LEPTON_HEIGHT][LEPTON_WIDTH];
    size_t head;
    size_t tail;
    size_t count;
} LeptonRingBuffer;

int lepton_ringbuffer_is_available(LeptonRingBuffer* rb);
int lepton_ringbuffer_is_empty(LeptonRingBuffer* rb);
int lepton_ringbuffer_enqueue(LeptonRingBuffer* rb, const uint16_t image[][LEPTON_WIDTH]);
int lepton_ringbuffer_dequeue(LeptonRingBuffer* rb, uint16_t image[][LEPTON_WIDTH]);

#endif