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
        char *filename = NULL;
        if (len) {
            filename = calloc(len + 1, 1);
            if (!filename) break;
            if (recv(fd, filename, len, MSG_WAITALL) <= 0) { free(filename); break; }
        }
        const char *request_name = filename && filename[0] ? filename : "(empty)";
        printf("[fs_service] read request for %s\n", request_name);
        char reply_text[160];
        snprintf(reply_text, sizeof(reply_text), "fs_data:%s", filename && filename[0] ? filename : "default.txt");
        uint32_t reply_len = (uint32_t)strlen(reply_text);
        msg_t reply = { MK_SERVICE_FS, header.src, MK_FS_CMD_READ, reply_len };
        if (send(fd, &reply, sizeof(reply), 0) != sizeof(reply)) { free(filename); break; }
        send(fd, reply_text, reply_len, 0);
        free(filename);
    }
}

int main(void) {
    printf("[fs_service] started\n");
    int fd = ipc_register_service(MK_SERVICE_FS, "fs");
    if (fd < 0) {
        fprintf(stderr, "[fs_service] failed to register\n");
        return 1;
    }
    run_service(fd);
    close(fd);
    return 0;
}
