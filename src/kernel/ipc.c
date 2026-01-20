// Simple IPC server using UNIX domain sockets.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <errno.h>
#include "ipc.h"

static int server_fd = -1;
static pthread_t server_thread;
static volatile int server_running = 0;

static void *client_thread_fn(void *arg) {
    int client = (intptr_t)arg;
    while (1) {
        uint32_t hdr[4];
        ssize_t r = recv(client, hdr, sizeof(hdr), MSG_WAITALL);
        if (r <= 0) break;
        // simple echo server: read payload if any and send back header+payload
        uint32_t len = hdr[3];
        void *payload = NULL;
        if (len > 0) {
            payload = malloc(len);
            if (!payload) break;
            r = recv(client, payload, len, MSG_WAITALL);
            if (r <= 0) { free(payload); break; }
        }
        // echo header
        send(client, hdr, sizeof(hdr), 0);
        if (len > 0) {
            send(client, payload, len, 0);
            free(payload);
        }
    }
    close(client);
    return NULL;
}

static void *server_thread_fn(void *arg) {
    (void)arg;
    while (server_running) {
        struct sockaddr_un addr;
        socklen_t len = sizeof(addr);
        int client = accept(server_fd, (struct sockaddr *)&addr, &len);
        if (client < 0) {
            if (errno == EINTR) continue;
            break;
        }
        pthread_t th;
        pthread_create(&th, NULL, client_thread_fn, (void*)(intptr_t)client);
        pthread_detach(th);
    }
    return NULL;
}

int ipc_server_start(void) {
    if (server_running) return 0;
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path)-1);
    unlink(IPC_SOCKET_PATH);
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        return -1;
    }
    if (listen(server_fd, 8) < 0) {
        close(server_fd);
        return -1;
    }
    server_running = 1;
    if (pthread_create(&server_thread, NULL, server_thread_fn, NULL) != 0) {
        server_running = 0;
        close(server_fd);
        return -1;
    }
    return 0;
}

int ipc_server_stop(void) {
    if (!server_running) return 0;
    server_running = 0;
    shutdown(server_fd, SHUT_RDWR);
    close(server_fd);
    unlink(IPC_SOCKET_PATH);
    pthread_join(server_thread, NULL);
    return 0;
}
