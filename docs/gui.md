**GUI**: Web-based UI for IPC benchmarking

- `src/gui/server.py`: lightweight Python HTTP server (no external deps). Serves a simple page at `/` and an endpoint `/run?iters=N` that runs N IPC round-trips against `/tmp/microkernel_ipc.sock` and returns JSON with `avg_rtt_us`.

How to run (in WSL):

```bash
# start the kernel in background
make kernel
./build/kernel > /tmp/kernel.log 2>&1 &
# start the GUI server
python3 src/gui/server.py &
# open http://localhost:8000/ in your browser (on Windows)
```

Notes:
- GUI code expects the kernel IPC server to be listening at `/tmp/microkernel_ipc.sock`.
- This web GUI is intentionally dependency-free so it runs in minimal WSL installations.
