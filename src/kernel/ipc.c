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
static pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    uint32_t id;
    int fd;
    char name[32];
} connection_entry_t;

#define MAX_SERVICES 8
static connection_entry_t service_registry[MAX_SERVICES];
#define MAX_CLIENTS 16
static connection_entry_t client_registry[MAX_CLIENTS];

static void register_entry(connection_entry_t *registry, int max, uint32_t id, int fd, const char *name_prefix) {
    if (id == MK_SERVICE_KERNEL) {
        return;
    }
    int inserted = 0;
    pthread_mutex_lock(&registry_lock);
    for (int i = 0; i < max; i++) {
        if (registry[i].id == 0 || registry[i].id == id) {
            registry[i].id = id;
            registry[i].fd = fd;
            if (name_prefix) {
                snprintf(registry[i].name, sizeof(registry[i].name), "%s%u", name_prefix, id);
            }
            inserted = 1;
            break;
        }
    }
    pthread_mutex_unlock(&registry_lock);
    if (!inserted) {
        fprintf(stderr, "[kernel] registry full for id %u\n", id);
    }
}

static int lookup_entry(connection_entry_t *registry, int max, uint32_t id) {
    int target_fd = -1;
    pthread_mutex_lock(&registry_lock);
    for (int i = 0; i < max; i++) {
        if (registry[i].id == id) {
            target_fd = registry[i].fd;
            break;
        }
    }
    pthread_mutex_unlock(&registry_lock);
    return target_fd;
}

static void register_service(uint32_t id, int fd) {
    register_entry(service_registry, MAX_SERVICES, id, fd, "svc-");
}

static void register_client(uint32_t id, int fd) {
    register_entry(client_registry, MAX_CLIENTS, id, fd, "client-");
}

static int lookup_service(uint32_t id) {
    return lookup_entry(service_registry, MAX_SERVICES, id);
}

static int lookup_client(uint32_t id) {
    return lookup_entry(client_registry, MAX_CLIENTS, id);
}

static int is_service_id(uint32_t id) {
    return id >= MK_SERVICE_CONSOLE && id <= MK_SERVICE_NET;
}

static void *client_thread_fn(void *arg) {
    int client = (intptr_t)arg;
    uint32_t service_id = 0;
    while (1) {
        msg_t header;
        ssize_t r = recv(client, &header, sizeof(header), MSG_WAITALL);
        if (r <= 0) break;
        uint32_t len = header.len;
        if (len > MK_MAX_PAYLOAD) {
            break;
        }
        uint8_t *payload = NULL;
        if (len) {
            payload = malloc(len);
            if (!payload) break;
            r = recv(client, payload, len, MSG_WAITALL);
            if (r <= 0) { free(payload); break; }
        }
        if (header.dst == MK_SERVICE_KERNEL && header.type == MK_CMD_REGISTER) {
            service_id = header.src;
            register_service(service_id, client);

            msg_t reply = { MK_SERVICE_KERNEL, service_id, MK_CMD_REGISTER, 0 };
            if (send(client, &reply, sizeof(reply), 0) != sizeof(reply)) {
                free(payload);
                break;
            }
        } else {
            if (service_id == 0) {
                register_client(header.src, client);
            }
            int target_fd = is_service_id(header.dst) ? lookup_service(header.dst) : lookup_client(header.dst);
            if (target_fd < 0) {
                msg_t reply = { MK_SERVICE_KERNEL, header.src, MK_CMD_ERROR, 0 };
                if (send(client, &reply, sizeof(reply), 0) != sizeof(reply)) {
                    free(payload);
                    break;
                }
            } else {
                if (send(target_fd, &header, sizeof(header), 0) != sizeof(header)) {
                    free(payload);
                    break;
                }
                if (len) {
                    if (send(target_fd, payload, len, 0) != (ssize_t)len) {
                        free(payload);
                        break;
                    }
                }
            }
        }
        free(payload);
    }
    pthread_mutex_lock(&registry_lock);
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (service_registry[i].fd == client) {
            service_registry[i].fd = -1;
            service_registry[i].id = 0;
            service_registry[i].name[0] = '\0';
        }
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_registry[i].fd == client) {
            client_registry[i].fd = -1;
            client_registry[i].id = 0;
            client_registry[i].name[0] = '\0';
        }
    }
    pthread_mutex_unlock(&registry_lock);
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

int ipc_register_service(uint32_t service_id) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path)-1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    msg_t header = { service_id, MK_SERVICE_KERNEL, MK_CMD_REGISTER, 0 };
    if (send(fd, &header, sizeof(header), 0) != sizeof(header)) {
        close(fd);
        return -1;
    }
    msg_t reply;
    if (recv(fd, &reply, sizeof(reply), MSG_WAITALL) != sizeof(reply)) {
        close(fd);
        return -1;
    }
    if (reply.type != MK_CMD_REGISTER) {
        close(fd);
        return -1;
    }
    return fd;
}
