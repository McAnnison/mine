// Bench client: connects to the kernel IPC socket, sends N messages and measures RTT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdint.h>
#include "../include/kernel.h"

static const char *socket_path = "/tmp/microkernel_ipc.sock";

static uint32_t client_id(void) {
    return (uint32_t)getpid();
}

static int connect_ipc(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("connect"); close(fd); return -1; }
    return fd;
}

static int send_request(uint32_t dst, uint32_t type, const void *payload, uint32_t len, uint32_t expect_type) {
    int fd = connect_ipc();
    if (fd < 0) return 1;
    msg_t header = { client_id(), dst, type, len };
    if (send(fd, &header, sizeof(header), 0) != sizeof(header)) { perror("send hdr"); close(fd); return 1; }
    if (len && send(fd, payload, len, 0) != (ssize_t)len) { perror("send payload"); close(fd); return 1; }
    msg_t reply;
    if (recv(fd, &reply, sizeof(reply), MSG_WAITALL) != sizeof(reply)) { perror("recv hdr"); close(fd); return 1; }
    if (reply.type == MK_CMD_ERROR) {
        fprintf(stderr, "kernel error: service unavailable\n");
        close(fd);
        return 1;
    }
    if (reply.type != expect_type) {
        fprintf(stderr, "unexpected reply type %u\n", reply.type);
        close(fd);
        return 1;
    }
    if (reply.len) {
        if (reply.len > MK_MAX_PAYLOAD) { fprintf(stderr, "reply too large\n"); close(fd); return 1; }
        char *buffer = calloc(reply.len + 1, 1);
        if (!buffer) { fprintf(stderr, "alloc failed\n"); close(fd); return 1; }
        if (recv(fd, buffer, reply.len, MSG_WAITALL) != (ssize_t)reply.len) { perror("recv payload"); free(buffer); close(fd); return 1; }
        printf("reply: %s\n", buffer);
        free(buffer);
    }
    close(fd);
    return 0;
}

static inline uint64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: %s bench <iters> | console <msg> | fs <file> | net <payload> | monolithic <iters>\n", argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "bench") == 0) {
        int iterations = (argc > 2) ? atoi(argv[2]) : 10000;
        int fd = connect_ipc();
        if (fd < 0) return 1;
        uint64_t start = now_ns();
        for (int i = 0; i < iterations; i++) {
            msg_t header = { client_id(), MK_SERVICE_CONSOLE, MK_CONSOLE_CMD_ECHO, 8 };
            uint64_t payload = (uint64_t)i;
            if (send(fd, &header, sizeof(header), 0) != sizeof(header)) { perror("send hdr"); break; }
            if (send(fd, &payload, sizeof(payload), 0) != (ssize_t)sizeof(payload)) { perror("send payload"); break; }
            msg_t reply;
            if (recv(fd, &reply, sizeof(reply), MSG_WAITALL) != sizeof(reply)) { perror("recv hdr"); break; }
            uint64_t rpayload = 0;
            if (recv(fd, &rpayload, sizeof(rpayload), MSG_WAITALL) != (ssize_t)sizeof(rpayload)) { perror("recv payload"); break; }
            if (rpayload != payload) { fprintf(stderr, "mismatch\n"); break; }
        }
        uint64_t end = now_ns();
        double avg_ns = (double)(end - start) / iterations;
        printf("iterations=%d avg IPC RTT = %.1f us\n", iterations, avg_ns/1000.0);
        close(fd);
        return 0;
    }

    if (strcmp(argv[1], "monolithic") == 0) {
        int iterations = (argc > 2) ? atoi(argv[2]) : 10000;
        uint64_t start = now_ns();
        volatile uint64_t accumulator = 0;
        for (int i = 0; i < iterations; i++) {
            accumulator += (uint64_t)i;
        }
        uint64_t end = now_ns();
        double avg_ns = (double)(end - start) / iterations;
        printf("iterations=%d avg monolithic call = %.1f us\n", iterations, avg_ns/1000.0);
        return 0;
    }

    if (strcmp(argv[1], "console") == 0) {
        const char *msg = (argc > 2) ? argv[2] : "hello";
        return send_request(MK_SERVICE_CONSOLE, MK_CONSOLE_CMD_ECHO, msg, (uint32_t)strlen(msg), MK_CONSOLE_CMD_ECHO);
    }

    if (strcmp(argv[1], "fs") == 0) {
        const char *file = (argc > 2) ? argv[2] : "readme.txt";
        return send_request(MK_SERVICE_FS, MK_FS_CMD_READ, file, (uint32_t)strlen(file), MK_FS_CMD_READ);
    }

    if (strcmp(argv[1], "net") == 0) {
        const char *payload = (argc > 2) ? argv[2] : "ping";
        return send_request(MK_SERVICE_NET, MK_NET_CMD_PING, payload, (uint32_t)strlen(payload), MK_NET_CMD_PING);
    }

    fprintf(stderr, "unknown command\n");
    return 1;
}
