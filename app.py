#!/usr/bin/env python3
"""
Local file-share server.

Usage:
    python3 app.py /path/to/file

Then, on the same WiFi, your brother opens:
    http://<your-local-ip>:5000

Find your local IP with:  hostname -I   (or  ip addr show)
"""

import os
import sys
from datetime import datetime

from flask import Flask, abort, render_template, send_file

app = Flask(__name__)

# ---- figure out what file we're sharing ----

if len(sys.argv) < 2:
    print("Usage: python3 app.py /path/to/file")
    sys.exit(1)

FILE_PATH = os.path.abspath(sys.argv[1])

if not os.path.isfile(FILE_PATH):
    print(f"Error: no such file: {FILE_PATH}")
    sys.exit(1)

FILE_NAME = os.path.basename(FILE_PATH)


def human_size(num_bytes: int) -> str:
    size = float(num_bytes)
    for unit in ["B", "KB", "MB", "GB"]:
        if size < 1024:
            return f"{size:.1f} {unit}" if unit != "B" else f"{int(size)} {unit}"
        size /= 1024
    return f"{size:.1f} TB"


@app.route("/")
def index():
    stat = os.stat(FILE_PATH)
    return render_template(
        "index.html",
        filename=FILE_NAME,
        filesize=human_size(stat.st_size),
        modified=datetime.fromtimestamp(stat.st_mtime).strftime("%b %d, %Y"),
    )


@app.route("/download")
def download():
    # re-check in case the file moved/got deleted after the server started
    if not os.path.isfile(FILE_PATH):
        abort(404)
    return send_file(FILE_PATH, as_attachment=True, download_name=FILE_NAME)


if __name__ == "__main__":
    print("=" * 50)
    print(f"Sharing: {FILE_PATH}")
    print("On your brother's device (same WiFi), open:")
    print("    http://<this-machine-local-ip>:5000")
    print("Find your IP with: hostname -I")
    print("=" * 50)
    app.run(host="0.0.0.0", port=5000, debug=False)
