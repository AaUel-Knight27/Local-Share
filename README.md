# Local File Share

A tiny one-page web app to hand a file to your brother over your home WiFi.
No internet, no accounts — just your local network.

## 1. One-time setup

You need Python 3 and Flask. On most Linux distros:

```bash
pip install flask --break-system-packages
```

(If you'd rather not touch system packages, make a venv instead:
`python3 -m venv venv && source venv/bin/activate && pip install flask`)

## 2. Find your local IP

```bash
hostname -I
```

This prints something like `192.168.1.23`. That's the address your brother
will type into his browser.

## 3. Run it, pointing at the file you want to share

```bash
python3 app.py /path/to/your/file.zip
```

You'll see:

```
Sharing: /path/to/your/file.zip
On your brother's device (same WiFi), open:
    http://<this-machine-local-ip>:5000
```

## 4. On your brother's device

Connect to the same WiFi, open a browser, and go to:

```
http://192.168.1.23:5000
```

(using whatever IP `hostname -I` gave you). He'll see the page with a
Download button — tapping it downloads the exact file you pointed the
server at.

## 5. Sharing a different file later

Stop the server (Ctrl+C) and re-run step 3 with a new path. The page
always reflects whichever file you launched it with.

## Notes

- This only works while your machine is on and the script is running, and
  only for devices on the same WiFi network — it isn't reachable from the
  internet.
- If your brother can't connect, check your firewall isn't blocking port
  5000: `sudo ufw allow 5000` (only needed if ufw is active).
- Want it to feel more permanent? You can add a `--port` option, but for a
  two-person setup the defaults here are normally enough.
