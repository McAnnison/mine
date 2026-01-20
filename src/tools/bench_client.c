// Bench client: connects to the kernel IPC socket, sends N messages and measures RTT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdint.h>

#define SOCKET_PATH "/tmp/microkernel_ipc.sock"

static inline uint64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
}

int main(int argc, char **argv) {
    int iterations = 10000;
    if (argc > 1) iterations = atoi(argv[1]);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }
    struct sockaddr_un addr;
    memset(&addr,0,sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path)-1);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("connect"); return 1; }

    uint64_t start = now_ns();
    for (int i=0;i<iterations;i++) {
        uint32_t hdr[4];
        hdr[0]=1; hdr[1]=2; hdr[2]=0x10; hdr[3]=8;
        uint64_t payload = (uint64_t)i;
        if (send(fd, hdr, sizeof(hdr), 0) != sizeof(hdr)) { perror("send hdr"); break; }
        if (send(fd, &payload, sizeof(payload), 0) != sizeof(payload)) { perror("send payload"); break; }
        // read echo
        uint32_t rhdr[4];
        if (recv(fd, rhdr, sizeof(rhdr), MSG_WAITALL) != sizeof(rhdr)) { perror("recv hdr"); break; }
        uint64_t rpayload;
        if (recv(fd, &rpayload, sizeof(rpayload), MSG_WAITALL) != sizeof(rpayload)) { perror("recv payload"); break; }
        if (rpayload != payload) { fprintf(stderr, "mismatch\n"); break; }
    }
    uint64_t end = now_ns();
    double avg_ns = (double)(end - start) / iterations;
    printf("iterations=%d avg RTT = %.1f us\n", iterations, avg_ns/1000.0);
    close(fd);
    return 0;
}
