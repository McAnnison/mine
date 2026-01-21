#include <stdio.h>
#include <unistd.h>
#include "../include/kernel.h"
#include "ipc.h"

void kernel_init(void) {
    printf("[kernel] init (minimal microkernel)\n");
}

void kernel_poll(void) {
    // IPC routing handled inside ipc.c server threads.
}

int main(void) {
    kernel_init();
    if (ipc_server_start() == 0) {
        printf("[kernel] IPC server started at %s\n", IPC_SOCKET_PATH);
    } else {
        fprintf(stderr, "[kernel] failed to start IPC server\n");
    }
    printf("[kernel] running; press Ctrl+C to exit.\n");
    // simple loop to keep process alive
    while (1) {
        pause();
    }
    ipc_server_stop();
    return 0;
}
