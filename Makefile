CC := gcc
CFLAGS := -Wall -Wextra -Iinclude -g
LDFLAGS := -pthread

KERNEL_SRC := src/kernel/kernel.c
KERNEL_OBJ := src/kernel/ipc.c
SERVICES := src/services/console_service.c src/services/fs_service.c src/services/net_service.c
TOOLS := src/tools/bench_client.c

.PHONY: all kernel services clean

all: kernel services

kernel:
	mkdir -p build
	$(CC) $(CFLAGS) $(KERNEL_SRC) $(KERNEL_OBJ) -o build/kernel $(LDFLAGS)

services:
	mkdir -p build
	$(CC) $(CFLAGS) -o build/console_service src/services/console_service.c
	$(CC) $(CFLAGS) -o build/fs_service src/services/fs_service.c
	$(CC) $(CFLAGS) -o build/net_service src/services/net_service.c

tools:
	mkdir -p build
	$(CC) $(CFLAGS) -o build/bench_client $(TOOLS) $(LDFLAGS)

clean:
	rm -rf build
