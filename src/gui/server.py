#!/usr/bin/env python3
"""
Lightweight web GUI for the microkernel IPC bench.

Runs a tiny HTTP server on port 8000 that serves a static page and an endpoint
`/run?iters=N` which performs N IPC round-trips against the UNIX-domain socket
`/tmp/microkernel_ipc.sock` and returns JSON with average RTT in microseconds.

No external dependencies - works in WSL with Python 3.
"""
import http.server
import socketserver
import socket
import time
import json
import errno
from urllib.parse import urlparse, parse_qs

IPC_PATH = "/tmp/microkernel_ipc.sock"

PORT = 8000
MAX_ITERS = 100000
CONNECTION_ERRNOS = {
    errno.ENOENT,
    errno.ECONNREFUSED,
    errno.ECONNRESET,
    errno.EPIPE,
    errno.ETIMEDOUT,
}

HTML = """
<!doctype html>
<html>
  <head>
    <meta charset="utf-8" />
    <title>Microkernel IPC Bench</title>
    <style>
      :root {
        color-scheme: light;
        font-family: "Segoe UI", system-ui, -apple-system, sans-serif;
      }
      body {
        margin: 0;
        background: #f1f5f9;
        color: #0f172a;
      }
      .page {
        max-width: 760px;
        margin: 32px auto;
        padding: 0 20px;
      }
      .header {
        display: flex;
        flex-wrap: wrap;
        align-items: center;
        justify-content: space-between;
        gap: 16px;
        margin-bottom: 20px;
      }
      .title {
        margin: 0 0 6px;
        font-size: 28px;
        font-weight: 700;
      }
      .subtitle {
        margin: 0;
        color: #475569;
      }
      .status {
        display: flex;
        align-items: center;
        gap: 10px;
        font-weight: 600;
      }
      .badge {
        padding: 4px 12px;
        border-radius: 999px;
        font-size: 12px;
        letter-spacing: 0.3px;
        text-transform: uppercase;
      }
      .badge-connected {
        background: #dcfce7;
        color: #166534;
      }
      .badge-disconnected {
        background: #fee2e2;
        color: #991b1b;
      }
      .card {
        background: #ffffff;
        border-radius: 16px;
        padding: 20px;
        box-shadow: 0 12px 30px rgba(15, 23, 42, 0.08);
      }
      .controls {
        display: flex;
        flex-wrap: wrap;
        gap: 16px;
        align-items: flex-end;
      }
      .field {
        display: flex;
        flex-direction: column;
        gap: 6px;
        font-size: 14px;
        font-weight: 600;
      }
      input {
        padding: 10px 12px;
        border-radius: 10px;
        border: 1px solid #cbd5f5;
        font-size: 16px;
        width: 180px;
      }
      button {
        padding: 10px 16px;
        border-radius: 10px;
        border: none;
        font-size: 16px;
        font-weight: 600;
        background: #2563eb;
        color: #ffffff;
        cursor: pointer;
      }
      button:disabled {
        background: #94a3b8;
        cursor: not-allowed;
      }
      .hint {
        margin: 12px 0 0;
        color: #64748b;
        font-size: 13px;
      }
      .error {
        margin-top: 16px;
        padding: 12px 14px;
        background: #fee2e2;
        color: #991b1b;
        border-radius: 10px;
        font-weight: 600;
      }
      .output {
        margin-top: 16px;
        padding: 16px;
        background: #0f172a;
        color: #e2e8f0;
        border-radius: 12px;
        min-height: 120px;
        white-space: pre-wrap;
      }
    </style>
  </head>
  <body>
    <div class="page">
      <header class="header">
        <div>
          <h1 class="title">Microkernel IPC Bench</h1>
          <p class="subtitle">Run IPC round-trip benchmarks from your browser.</p>
        </div>
        <div class="status">
          <span>Status</span>
          <span id="status" class="badge badge-disconnected">Disconnected</span>
        </div>
      </header>
      <section class="card">
        <div class="controls">
          <label class="field">
            <span>Iterations</span>
            <input id="iters" type="number" min="1" max="__MAX_ITERS_PLACEHOLDER__" value="1000" />
          </label>
          <button id="run-btn" onclick="run()">Run</button>
        </div>
        <p class="hint">Max __MAX_ITERS_PLACEHOLDER__ iterations per run.</p>
        <div id="error" class="error" hidden></div>
        <pre id="out" class="output"></pre>
      </section>
    </div>
    <script>
      const MAX_ITERS = __MAX_ITERS_PLACEHOLDER__;
      const statusEl = document.getElementById('status');
      const runBtn = document.getElementById('run-btn');
      const outEl = document.getElementById('out');
      const errorEl = document.getElementById('error');
      const itersEl = document.getElementById('iters');

      runBtn.disabled = true;

      function setStatus(connected) {
        statusEl.textContent = connected ? 'Connected' : 'Disconnected';
        statusEl.className = connected ? 'badge badge-connected' : 'badge badge-disconnected';
        runBtn.disabled = !connected;
      }

      function showError(message) {
        errorEl.textContent = message;
        errorEl.hidden = false;
      }

      function clearError() {
        errorEl.textContent = '';
        errorEl.hidden = true;
      }

      function validateIterations() {
        const raw = itersEl.value.trim();
        if (!raw) {
          return { ok: false, message: `Iterations must be a positive integer up to ${MAX_ITERS}.` };
        }
        const value = Number(raw);
        if (!Number.isInteger(value) || value < 1 || value > MAX_ITERS) {
          return { ok: false, message: `Iterations must be a positive integer up to ${MAX_ITERS}.` };
        }
        return { ok: true, value };
      }

      async function fetchStatus() {
        try {
          const res = await fetch('/status');
          const data = await res.json();
          setStatus(Boolean(data.connected));
        } catch (err) {
          setStatus(false);
        }
      }

      async function run() {
        clearError();
        outEl.textContent = '';
        const validation = validateIterations();
        if (!validation.ok) {
          showError(validation.message);
          return;
        }
        runBtn.disabled = true;
        outEl.textContent = 'Running...';
        try {
          const res = await fetch('/run?iters=' + encodeURIComponent(validation.value));
          const data = await res.json();
          if (!res.ok) {
            throw new Error(data.error || 'Unable to run benchmark.');
          }
          outEl.textContent = JSON.stringify(data, null, 2);
        } catch (err) {
          outEl.textContent = '';
          showError(err.message);
        } finally {
          await fetchStatus();
        }
      }

      fetchStatus();
      setInterval(fetchStatus, 3000);
    </script>
  </body>
</html>
""".replace("__MAX_ITERS_PLACEHOLDER__", str(MAX_ITERS)).encode('utf-8')


def check_ipc_connection(timeout=0.5):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
        s.settimeout(timeout)
        try:
            s.connect(IPC_PATH)
            return True, None
        except OSError as e:
            if e.errno in CONNECTION_ERRNOS:
                return False, 'IPC socket unavailable.'
            return False, str(e)


def ipc_roundtrip(fd, payload_value):
    # header: 4 uint32 fields
    hdr = (100).to_bytes(4, 'little') + (1).to_bytes(4, 'little') + (0x10).to_bytes(4, 'little') + (8).to_bytes(4, 'little')
    payload = int(payload_value).to_bytes(8, 'little')
    fd.sendall(hdr)
    fd.sendall(payload)
    # read echo
    rhdr = fd.recv(16)
    if len(rhdr) != 16:
        raise RuntimeError('short read hdr')
    rpayload = fd.recv(8)
    if len(rpayload) != 8:
        raise RuntimeError('short read payload')
    return int.from_bytes(rpayload, 'little')


class Handler(http.server.BaseHTTPRequestHandler):
    def _send_json(self, status_code, payload_obj):
        payload = json.dumps(payload_obj).encode('utf-8')
        self.send_response(status_code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == '/':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.end_headers()
            self.wfile.write(HTML)
            return

        if parsed.path == '/status':
            connected, error = check_ipc_connection()
            payload = {'connected': connected}
            if error:
                payload['error'] = error
            self._send_json(200, payload)
            return

        if parsed.path == '/run':
            try:
                qs = parse_qs(parsed.query)
                raw_iters = qs.get('iters', ['1000'])[0]
                try:
                    iters = int(raw_iters)
                except ValueError:
                    raise ValueError('Iterations must be an integer.')
                if iters < 1 or iters > MAX_ITERS:
                    raise ValueError(f'Iterations must be between 1 and {MAX_ITERS}.')
                # connect to IPC socket
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
                    s.settimeout(5.0)
                    s.connect(IPC_PATH)
                    start = time.perf_counter()
                    for i in range(iters):
                        ipc_roundtrip(s, i)
                    end = time.perf_counter()
                avg_us = (end - start) * 1e6 / iters
                self._send_json(200, {'iterations': iters, 'avg_rtt_us': avg_us})
            except ValueError as e:
                self._send_json(400, {'error': str(e)})
            except OSError as e:
                if e.errno in CONNECTION_ERRNOS:
                    self._send_json(503, {'error': 'IPC service unavailable. Check that the kernel IPC server is running.'})
                else:
                    self._send_json(500, {'error': str(e)})
            except Exception as e:
                self._send_json(500, {'error': str(e)})
            return

        self.send_response(404)
        self.end_headers()


def run_server(port=PORT):
    with socketserver.TCPServer(('0.0.0.0', port), Handler) as httpd:
        print(f'GUI server listening on http://0.0.0.0:{port}/')
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print('shutting down')


if __name__ == '__main__':
    run_server()
