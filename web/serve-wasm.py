#!/usr/bin/env python3
from pathlib import Path
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit


class Handler(SimpleHTTPRequestHandler):
    extensions_map = {
        **SimpleHTTPRequestHandler.extensions_map,
        ".wasm": "application/wasm",
    }

    def do_GET(self):
        parsed = urlsplit(self.path)
        if parsed.path.endswith(".wasm"):
            accept_encoding = self.headers.get("Accept-Encoding", "")
            compressed_path = Path(self.translate_path(parsed.path + ".gz"))
            if "gzip" in accept_encoding and compressed_path.is_file():
                self.send_response(200)
                self.send_header("Content-Type", "application/wasm")
                self.send_header("Content-Encoding", "gzip")
                self.send_header("Vary", "Accept-Encoding")
                self.send_header("Content-Length", str(compressed_path.stat().st_size))
                self.end_headers()
                with compressed_path.open("rb") as source:
                    self.copyfile(source, self.wfile)
                return

        super().do_GET()

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        super().end_headers()


if __name__ == "__main__":
    ThreadingHTTPServer(("127.0.0.1", 8000), Handler).serve_forever()
