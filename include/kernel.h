#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

// Minimal kernel API surface for the skeleton
void kernel_init(void);
void kernel_poll(void);

// Service identifiers
#define MK_SERVICE_KERNEL 0
#define MK_SERVICE_CONSOLE 1
#define MK_SERVICE_FS 2
#define MK_SERVICE_NET 3

// Kernel-level commands
#define MK_CMD_REGISTER 0x0001
#define MK_CMD_ERROR 0xFFFF

// Service command identifiers
#define MK_CONSOLE_CMD_ECHO 0x0010
#define MK_FS_CMD_READ 0x0020
#define MK_NET_CMD_PING 0x0030

// Simple IPC header (wire format)
typedef struct {
    uint32_t src;
    uint32_t dst;
    uint32_t type;
    uint32_t len;
} msg_t;

#define MK_MAX_PAYLOAD 4096

#endif // KERNEL_H
