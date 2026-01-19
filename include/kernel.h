#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

// Minimal kernel API surface for the skeleton
void kernel_init(void);
void kernel_poll(void);

// Simple message structure (placeholder)
typedef struct {
    uint32_t src;
    uint32_t dst;
    uint32_t type;
    uint32_t len;
    void *payload;
} msg_t;

#endif // KERNEL_H
