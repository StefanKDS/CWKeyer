/**
 * @file    event_queue.h
 * @brief   Lock-free single-producer / single-consumer event queue.
 *
 * Used to pass events from interrupt context (keyer tick, audio DMA)
 * to the main loop. Each producer context needs its own queue.
 */
#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    EV_NONE = 0,
    EV_KEY_ELEMENT,   /**< paddle element sent, arg = '.' or '-'          */
    EV_KEY_CHAR,      /**< keyed character complete, arg = char           */
    EV_KEY_SPACE,     /**< word gap after keyed characters                */
    EV_DEC_CHAR,      /**< audio decoder character, arg = char            */
    EV_DEC_SPACE,     /**< audio decoder word gap                         */
    EV_PLAY_DONE,     /**< text playback finished                         */
} event_type_t;

typedef struct
{
    uint8_t type;     /**< event_type_t */
    char    arg;
} event_t;

#define EVQ_SIZE 64u  /* power of two */

typedef struct
{
    event_t           buf[EVQ_SIZE];
    volatile uint32_t head;   /**< written by producer */
    volatile uint32_t tail;   /**< written by consumer */
} event_queue_t;

static inline void evq_init(event_queue_t *q) { q->head = q->tail = 0; }

static inline bool evq_push(event_queue_t *q, event_type_t type, char arg)
{
    uint32_t h = q->head;
    if ((h - q->tail) >= EVQ_SIZE)
        return false;                        /* full: drop event */
    q->buf[h & (EVQ_SIZE - 1u)].type = (uint8_t)type;
    q->buf[h & (EVQ_SIZE - 1u)].arg  = arg;
    __sync_synchronize();
    q->head = h + 1u;
    return true;
}

static inline bool evq_pop(event_queue_t *q, event_t *ev)
{
    uint32_t t = q->tail;
    if (t == q->head)
        return false;
    *ev = q->buf[t & (EVQ_SIZE - 1u)];
    __sync_synchronize();
    q->tail = t + 1u;
    return true;
}

#endif
