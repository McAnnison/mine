#include <stdio.h>
#include "../include/kernel.h"
#include "ipc.h"

void kernel_init(void) {
    printf("[kernel] init (skeleton)\n");
}

void kernel_poll(void) {
    // In a real microkernel this would handle scheduling and message routing.
}

int main(void) {
    kernel_init();
    printf("[kernel] running stub; not a real kernel.\n");
    return 0;
}
