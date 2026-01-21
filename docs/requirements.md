Group 18: Microkernel Operating System Design

Project Description

Design and implement a microkernel-based operating system with minimal kernel functionality and user-space services.

Specific Requirements

In addition to the common requirements, this group must:

- Implement minimal kernel with basic IPC mechanisms
- Move traditional kernel services to user space
- Implement message passing between components
- Create at least 3 user-space services
- Demonstrate fault isolation benefits
- Compare performance with monolithic design

Implementation Notes (Current Repo)

- Kernel (`build/kernel`) is a minimal IPC router that accepts registrations from services and forwards messages.
- User-space services (`build/console_service`, `build/fs_service`, `build/net_service`) each run as independent processes.
- IPC uses UNIX domain sockets; the kernel mediates all communication.
- Fault isolation: killing a service only affects its functionality, the kernel continues routing other services.
- Performance comparison: `build/bench_client bench N` measures IPC RTT; `build/bench_client monolithic N` measures in-process loop cost.
