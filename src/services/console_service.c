#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "../kernel/ipc.h"

static void run_service(int fd) {
    while (1) {
        msg_t header;
        ssize_t r = recv(fd, &header, sizeof(header), MSG_WAITALL);
        if (r <= 0) break;
        uint32_t len = header.len;
        if (len > MK_MAX_PAYLOAD) break;
        char *payload = NULL;
        if (len) {
            payload = calloc(len + 1, 1);
            if (!payload) break;
            if (recv(fd, payload, len, MSG_WAITALL) <= 0) { free(payload); break; }
        }
        int printable = 0;
        if (payload && len) {
            printable = 1;
            for (uint32_t i = 0; i < len; i++) {
                unsigned char ch = (unsigned char)payload[i];
                if (ch < 32 || ch > 126) {
                    printable = 0;
                    break;
                }
            }
        }
        if (printable) {
            printf("[console_service] message from %u: %s\n", header.src, payload);
        }
        msg_t reply = { MK_SERVICE_CONSOLE, header.src, MK_CONSOLE_CMD_ECHO, len };
        if (send(fd, &reply, sizeof(reply), 0) != sizeof(reply)) { free(payload); break; }
        if (len) {
            send(fd, payload, len, 0);
        }
        free(payload);
    }
}

int main(void) {
    printf("[console_service] started\n");
    int fd = ipc_register_service(MK_SERVICE_CONSOLE, "console");
    if (fd < 0) {
        fprintf(stderr, "[console_service] failed to register\n");
        return 1;
    }
    run_service(fd);
    close(fd);
    return 0;
}
