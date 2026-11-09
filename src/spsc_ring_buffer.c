#include "spsc_ring_buffer.h"

void spsc_init(spsc_ringbuf_t *rb) {
    atomic_init(&rb->tail, 0);
    atomic_init(&rb->head, 0);
}

/**
 * enqueue an element. called only by the producer thread.
 * Returns true on success, false if full
 */
bool spsc_push(spsc_ringbuf_t *rb, void *item, size_t item_size) {
    size_t current_tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    size_t current_head = atomic_load_explicit(&rb->head, memory_order_acquire);

    if ((BUFFER_SIZE_BYTES - (current_tail - current_head)) < item_size) {
        return false;
    }
    
    size_t write_index = current_tail & BUFFER_MASK;
    size_t first_chunk = BUFFER_SIZE_BYTES - write_index;

    if (item_size <= first_chunk) {
        memcpy(&rb->data[write_index], item, item_size);
    } else {
        memcpy(&rb->data[write_index], item, first_chunk);
        memcpy(&rb->data[0], (const unsigned char *)item + first_chunk, item_size - first_chunk);
    }

    atomic_store_explicit(&rb->tail, current_tail + item_size, memory_order_release);
    return true;
}


/**
 * Dequeue an element. called only by consumer thread
 * Returns true on success, false if empty
 */
bool spsc_pop(spsc_ringbuf_t *rb, void *out_item, size_t item_size) {
    size_t current_head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    size_t current_tail = atomic_load_explicit(&rb->tail, memory_order_acquire);
     
    if ((current_tail - current_head) < item_size) {
        return false;
    }

    size_t read_index = current_head & BUFFER_MASK;
    size_t first_chunk = BUFFER_SIZE_BYTES - read_index;

    if (item_size <= first_chunk) {
        memcpy(out_item, &rb->data[read_index], item_size);
    } else {
        memcpy(out_item, &rb->data[read_index], first_chunk);
        memcpy((unsigned char *)out_item + first_chunk, &rb->data[0], item_size - first_chunk);
    }
    atomic_store_explicit(&rb->head, current_head + item_size, memory_order_release);
    return true;
}

