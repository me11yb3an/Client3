#ifndef BUILD_A_PACKET_QUEUE_H
#define BUILD_A_PACKET_QUEUE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Single owner: all client and QuickJS calls run on the game thread.
 * A byte stream, not a packet framing layer: preserves the 225 wire protocol.
 * Overflow is fatal to the connection; never silently discard protocol bytes. */
#define BA_QUEUE_CAPACITY (256u * 1024u)
typedef struct {
    uint8_t data[BA_QUEUE_CAPACITY];
    size_t head, size, peak;
    unsigned generation;
    int failed;
} BaQueue;
static inline void ba_queue_reset(BaQueue *q, unsigned generation) {
    q->head = q->size = q->peak = 0;
    q->generation = generation;
    q->failed = 0;
}
static inline int ba_queue_write(BaQueue *q, unsigned generation, const void *data, size_t n) {
    if (generation != q->generation || q->failed) return -1;
    if (n > BA_QUEUE_CAPACITY - q->size) { q->failed = 1; return -1; }
    size_t tail = (q->head + q->size) % BA_QUEUE_CAPACITY;
    size_t first = n < BA_QUEUE_CAPACITY - tail ? n : BA_QUEUE_CAPACITY - tail;
    if (n) { memcpy(q->data + tail, data, first); memcpy(q->data, (const uint8_t *)data + first, n - first); }
    q->size += n;
    if (q->size > q->peak) q->peak = q->size;
    return 0;
}
static inline size_t ba_queue_read(BaQueue *q, void *data, size_t n) {
    if (q->failed) return 0;
    if (n > q->size) n = q->size;
    size_t first = n < BA_QUEUE_CAPACITY - q->head ? n : BA_QUEUE_CAPACITY - q->head;
    if (n) { memcpy(data, q->data + q->head, first); memcpy((uint8_t *)data + first, q->data, n - first); }
    q->head = (q->head + n) % BA_QUEUE_CAPACITY;
    q->size -= n;
    return n;
}
#endif
