FROM fedora:43

RUN dnf install -y \
    gcc gcc-c++ make automake autoconf libtool pkgconf-pkg-config \
    gettext-devel glib2-devel \
    ibus-devel \
    sqlite-devel sqlite \
    libpinyin-devel \
    lua-devel \
    libsoup3-devel \
    json-glib-devel \
    opencc-devel \
    libnotify-devel \
    python3 python3-gobject python3-numpy \
    ibus ibus-gtk3 ibus-panel gtk3 mousepad adwaita-icon-theme \
    google-noto-sans-cjk-fonts google-noto-serif-cjk-fonts \
    tigervnc-server xorg-x11-server-Xvfb dbus-x11 \
    openbox \
    python3-websockify procps-ng \
    && dnf clean all

# Install noVNC + link websockify
RUN cd /opt && \
    curl -sL https://github.com/novnc/noVNC/archive/refs/tags/v1.5.0.tar.gz | tar xz && \
    mv noVNC-1.5.0 novnc && \
    ln -s /opt/novnc/vnc.html /opt/novnc/index.html && \
    mkdir -p /opt/novnc/utils/websockify && \
    echo '#!/bin/bash' > /opt/novnc/utils/websockify/run && \
    echo 'exec websockify "$@"' >> /opt/novnc/utils/websockify/run && \
    chmod +x /opt/novnc/utils/websockify/run

WORKDIR /build
COPY . /build

RUN ./autogen.sh --prefix=/usr \
    --enable-cloud-input-mode \
    --enable-opencc \
    --enable-lua-extension \
    --enable-english-input-mode \
    --enable-table-input-mode \
    --enable-libnotify \
    --enable-ai-input-mode \
    && make -j$(nproc) \
    && make install

EXPOSE 6080

COPY <<'EOF' /start.sh
#!/bin/bash

export DISPLAY=:1
export HOME=/root
export XDG_RUNTIME_DIR=/tmp/runtime
mkdir -p $XDG_RUNTIME_DIR

# 1. Virtual X server
Xvfb :1 -screen 0 1280x720x24 &
sleep 2

# 2. D-Bus session
eval $(dbus-launch --sh-syntax)
echo "$DBUS_SESSION_BUS_ADDRESS" > /tmp/dbus-addr
export DBUS_SESSION_BUS_ADDRESS

# 3. VNC server (local only, noVNC connects to it)
x0vncserver -display :1 -PasswordFile=none -SecurityTypes=None -rfbport=5901 -localhost &
sleep 1

# 4. noVNC web server (user connects here via browser)
/opt/novnc/utils/novnc_proxy --vnc localhost:5901 --listen 6080 &
sleep 1

# 5. Window manager
openbox &
sleep 1

# 6. IBus env
export GTK_IM_MODULE=ibus
export XMODIFIERS=@im=ibus
export QT_IM_MODULE=ibus

# 7. Start ibus-daemon
ibus-daemon -drx
for i in $(seq 1 15); do
    if ibus list-engine 2>/dev/null | grep -q smartpinyin; then
        echo "=== ibus-smartpinyin engine ready (${i}s) ==="
        break
    fi
    sleep 1
done

# 8. Start the candidate popup panel (ibus-daemon doesn't auto-start it in Xvfb)
GDK_BACKEND=x11 /usr/libexec/ibus-ui-gtk3 &
sleep 1

# 9. Configure smartpinyin as always-active default engine
ibus write-cache 2>/dev/null || true
gsettings set org.freedesktop.ibus.general preload-engines "['smartpinyin']" 2>/dev/null || true
# Activate smartpinyin immediately — no Ctrl+Space needed
sleep 1
ibus engine smartpinyin 2>/dev/null || true

echo ""
echo "=================================================="
echo "  Open in browser: http://localhost:6080"
echo "  Make sure HOST keyboard is in ENGLISH mode!"
echo "  Then just type pinyin (e.g. nihao) in mousepad."
echo "  Candidates will appear automatically."
echo "=================================================="
echo ""

# 9. Open GTK text editor
gsettings set org.xfce.mousepad.preferences.view use-default-font false 2>/dev/null || true
gsettings set org.xfce.mousepad.preferences.view font-name 'Noto Sans CJK SC 14' 2>/dev/null || true
mousepad &

# 10. Tail ibus engine log so podman logs shows it
touch /tmp/ibus-engine.log
(while true; do
    # capture engine stderr/stdout if ibus writes to syslog/journal
    if [ -f /root/.cache/ibus/smartpinyin/user-phrases.db ]; then
        echo "[monitor] user-phrases.db exists ($(date))" >> /tmp/ibus-engine.log
    fi
    sleep 10
done) &

tail -f /tmp/ibus-engine.log &

wait
EOF
RUN chmod +x /start.sh

CMD ["/start.sh"]

# --- Usage ---
#
# Build:
#   podman build -t ibus-smartpinyin-dev .
#
# Run:
#   podman run --rm -d --name ibus-test -p 6080:6080 --security-opt label=disable ibus-smartpinyin-dev
#
# Open in browser:
#   http://localhost:6080
#
# Stop:
#   podman stop ibus-test
