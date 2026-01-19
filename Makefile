CC := gcc
CFLAGS := -Wall -Wextra -Iinclude -g

KERNEL_SRC := src/kernel/kernel.c
SERVICES := src/services/console_service.c src/services/fs_service.c src/services/net_service.c

.PHONY: all kernel services clean

all: kernel services

kernel:
	mkdir -p build
	$(CC) $(CFLAGS) -o build/kernel $(KERNEL_SRC)

services:
	mkdir -p build
	$(CC) $(CFLAGS) -o build/console_service src/services/console_service.c
	$(CC) $(CFLAGS) -o build/fs_service src/services/fs_service.c
	$(CC) $(CFLAGS) -o build/net_service src/services/net_service.c

clean:
	rm -rf build
