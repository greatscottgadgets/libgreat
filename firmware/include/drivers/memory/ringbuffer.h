#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct ringbuffer_t {

    uint8_t *buffer;
    size_t size;

    uint64_t read_index;
    uint64_t write_index;

} ringbuffer_t;

void ringbuffer_init(ringbuffer_t *rb, void *buffer, size_t size);
int ringbuffer_enqueue(ringbuffer_t *rb, uint8_t element);
int ringbuffer_dequeue(ringbuffer_t *rb);
bool ringbuffer_full(ringbuffer_t *rb);
bool ringbuffer_empty(ringbuffer_t *rb);
int ringbuffer_enqueue_overwrite(ringbuffer_t *rb, uint8_t element);
uint32_t ringbuffer_data_available(ringbuffer_t *rb);

#endif
