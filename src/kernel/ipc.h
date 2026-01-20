#ifndef IPC_H
#define IPC_H

#include "../include/kernel.h"

// UNIX domain socket path used for IPC (WSL / Linux)
#define IPC_SOCKET_PATH "/tmp/microkernel_ipc.sock"

// IPC function prototypes
int ipc_server_start(void);
int ipc_server_stop(void);

#endif // IPC_H
