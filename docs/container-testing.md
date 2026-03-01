# Testing ibus-libpinyin in a Container

Fully isolated testing environment using Podman. No host modifications required.

## Prerequisites

- Podman
- Web browser

## Quick Start

```bash
# 1. Build
podman build -t ibus-libpinyin-dev .

# 2. Run
podman run --rm -d --name ibus-test -p 6080:6080 --security-opt label=disable ibus-libpinyin-dev

# 3. Open in browser
xdg-open http://localhost:6080

# 4. Stop when done
podman stop ibus-test
```

## How to Use

1. Open **http://localhost:6080** in your browser
2. Click **Connect**
3. Switch your **host keyboard to English** (important!)
4. Click inside the Mousepad editor
5. Type pinyin (e.g. `nihao`, `zhongguo`, `shijie`)
6. Candidate popup appears with Chinese characters
7. Select with number keys (1-9) or Space for the first candidate

## What's Inside the Container

- **Xvfb** -- virtual X11 display server
- **noVNC** -- web-based VNC client (port 6080)
- **IBus + libpinyin** -- built from source with all features enabled
- **Mousepad** -- lightweight GTK text editor for testing input
- **openbox** -- minimal window manager

No host D-Bus, IBus, or display server is shared. Everything runs in isolation.

## Build Flags

The container builds with all optional features:

```
--prefix=/usr
--enable-cloud-input-mode
--enable-opencc
--enable-lua-extension
--enable-english-input-mode
--enable-table-input-mode
--enable-libnotify
```

## Rebuilding After Code Changes

```bash
podman stop ibus-test 2>/dev/null; true
podman build -t ibus-libpinyin-dev .
podman run --rm -d --name ibus-test -p 6080:6080 --security-opt label=disable ibus-libpinyin-dev
```

## Debugging

```bash
# View container logs
podman logs ibus-test

# Shell into the container
podman exec -it ibus-test bash

# Inside the container, set up environment first:
export DISPLAY=:1
export DBUS_SESSION_BUS_ADDRESS=$(cat /tmp/dbus-addr)
export XDG_RUNTIME_DIR=/tmp/runtime

# Check active engine
ibus engine

# List available engines
ibus list-engine | grep libpinyin

# Restart ibus
ibus restart && sleep 3 && ibus engine libpinyin
```

## Troubleshooting

**Candidate popup not visible:**
The ibus-ui-gtk3 panel must be running. Check with:
```bash
podman exec ibus-test bash -c 'for f in /proc/*/comm; do c=$(cat $f 2>/dev/null); [[ "$c" == *ibus-ui* ]] && echo OK; done'
```

**SELinux errors:**
Always run with `--security-opt label=disable`.

**Host input method interferes:**
Make sure your host keyboard is set to **English/Latin** before typing in the browser. The host IBus processes keystrokes before they reach the browser.

**Port 6080 already in use:**
```bash
podman run --rm -d --name ibus-test -p 7080:6080 --security-opt label=disable ibus-libpinyin-dev
# Then open http://localhost:7080
```
