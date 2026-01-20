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
from urllib.parse import urlparse, parse_qs

IPC_PATH = "/tmp/microkernel_ipc.sock"

PORT = 8000

HTML = b"""
<!doctype html>
<html>
  <head>
    <meta charset="utf-8" />
    <title>Microkernel IPC Bench</title>
  </head>
  <body>
    <h1>Microkernel IPC Bench</h1>
    <label>Iterations: <input id="iters" value="1000"/></label>
    <button onclick="run()">Run</button>
    <pre id="out"></pre>
    <script>
    async function run(){
      const iters = document.getElementById('iters').value;
      document.getElementById('out').textContent = 'Running...';
      const res = await fetch('/run?iters='+encodeURIComponent(iters));
      const data = await res.json();
      document.getElementById('out').textContent = JSON.stringify(data, null, 2);
    }
    </script>
  </body>
</html>
"""


def ipc_roundtrip(fd, payload_value):
    # header: 4 uint32 fields
    hdr = (1).to_bytes(4, 'little') + (2).to_bytes(4, 'little') + (0x10).to_bytes(4, 'little') + (8).to_bytes(4, 'little')
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
    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == '/':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.end_headers()
            self.wfile.write(HTML)
            return

        if parsed.path == '/run':
            try:
                qs = parse_qs(parsed.query)
                iters = int(qs.get('iters', ['1000'])[0])
                # connect to IPC socket
                s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                s.settimeout(5.0)
                s.connect(IPC_PATH)
                start = time.perf_counter()
                for i in range(iters):
                    ipc_roundtrip(s, i)
                end = time.perf_counter()
                s.close()
                avg_us = (end - start) * 1e6 / iters
                out = { 'iterations': iters, 'avg_rtt_us': avg_us }
                payload = json.dumps(out).encode('utf-8')
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)
            except Exception as e:
                self.send_response(500)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({'error': str(e)}).encode('utf-8'))
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
