# ibus-smartpinyin

Smart Pinyin engine based on libpinyin for IBus.

This is a fork of [ibus-libpinyin](https://github.com/epico/ibus-libpinyin) (by Peng Huang, BYVoid, and Peng Wu) with enhanced user phrase learning. The original project provides a solid Pinyin/Bopomofo input method, but its phrase learning has limitations — custom words and names are not always persisted, and abbreviated pinyin often fails to suggest previously learned phrases.

**ibus-smartpinyin** adds a SQLite-based user phrase database that reliably remembers every phrase you type, promotes frequently used phrases to the top of the candidate list, and supports syllable-based prefix matching for faster input.

## Features

- All features of ibus-libpinyin (full/double pinyin, bopomofo, fuzzy matching, emoji, cloud input, etc.)
- SQLite-based user phrase learning with frequency-based promotion
- Syllable prefix matching for learned phrases
- Can be installed alongside the original ibus-libpinyin (separate engine names, GSettings schemas, and data directories)

## Building

```bash
# Dependencies (Fedora):
sudo dnf install gcc-c++ make automake autoconf libtool pkgconfig \
  gettext-devel ibus-devel sqlite-devel libpinyin-devel \
  lua-devel libsoup3-devel json-glib-devel opencc-devel libnotify-devel \
  python3-gobject

# Build:
./autogen.sh --prefix=/usr
make -j$(nproc)
sudo make install
```

### Configure flags

| Flag | Default | Description |
|------|---------|-------------|
| `--enable-cloud-input-mode` | no | Cloud candidate suggestions |
| `--enable-opencc` | no | Simplified/Traditional conversion |
| `--enable-lua-extension` | yes | Lua scripting support |
| `--enable-english-input-mode` | yes | English word input mode |
| `--enable-table-input-mode` | yes | Table-based input (wubi) |
| `--enable-libnotify` | yes | Desktop notifications |

## Container testing

```bash
podman build -t ibus-smartpinyin-dev .
podman run --rm -d --name ibus-test -p 6080:6080 --security-opt label=disable ibus-smartpinyin-dev
# Open http://localhost:6080 in browser
```

## License

GPLv3 — same as the original project. See [COPYING](COPYING).

## Credits

This project is a fork of [ibus-libpinyin](https://github.com/epico/ibus-libpinyin) by:
- Peng Huang
- BYVoid
- Peng Wu

Fork maintained by [黑鱼](https://github.com/xierongchuan).
