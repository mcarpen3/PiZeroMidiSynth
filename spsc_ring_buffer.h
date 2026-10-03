#ifndef SPSC_RING_BUFFER_H
#define SPSC_RING_BUFFER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#define BUFFER_SIZE_BYTES 256
#define BUFFER_MASK BUFFER_SIZE_BYTES - 1

typedef struct {
    unsigned char data[BUFFER_SIZE_BYTES];
    size_t capacity;

    _Alignas(CACHE_LINE_SIZE) atomic_size_t tail;
    _Alignas(CACHE_LINE_SIZE) atomic_size_t head;
} spsc_ringbuf_t;

void spsc_init(spsc_ringbuf_t *rb);
bool spsc_push(spsc_ringbuf_t *rb, void *item, size_t item_size);
bool spsc_pop(spsc_ringbuf_t *rb, void *out_item, size_t item_size);

#endif //SPSC_RING_BUFFER_H
