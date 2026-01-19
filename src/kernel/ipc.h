#ifndef IPC_H
#define IPC_H

#include "../include/kernel.h"

// IPC function prototypes (stubs)
int ipc_send(uint32_t dst, const msg_t *m);
int ipc_recv(uint32_t *src, msg_t *m);

#endif // IPC_H
