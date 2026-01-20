Microkernel OS — Project Skeleton

Project Description

Design and implement a microkernel-based operating system with minimal kernel functionality and user-space services.

Structure

- docs/: project requirements and design notes
- include/: public headers for kernel and services
- src/kernel/: kernel skeleton and IPC interface
- src/services/: example user-space services (console, fs, network)
- build/: build outputs
- Makefile: build targets for kernel and services

Quick start

To build the skeleton (requires a C toolchain):

```sh
make
```

This creates simple binaries in `build/` (stubs only). See `docs/requirements.md` for project requirements.

Web GUI

There is a minimal web GUI that runs in WSL without external dependencies. Start the kernel and then the GUI server:

```bash
make kernel
./build/kernel > /tmp/kernel.log 2>&1 &
python3 src/gui/server.py &
# then open http://localhost:8000/ in your browser
```

Use the GUI to run IPC round-trip benchmarks and view results.
