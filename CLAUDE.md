# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ibus-smartpinyin is a fork of [ibus-libpinyin](https://github.com/epico/ibus-libpinyin) — a Chinese Pinyin/Bopomofo input method engine for IBus, built on the libpinyin library. This fork adds enhanced user phrase learning via SQLite.

## Build Commands

```bash
# Bootstrap and configure (run once or after configure.ac changes)
./autogen.sh --prefix=/usr

# Configure with all optional features
./configure --enable-cloud-input-mode --enable-opencc --enable-lua-extension \
  --enable-english-input-mode --enable-table-input-mode --enable-libnotify

# Build
make -j$(nproc)

# Run tests (CI uses these)
make check
make distcheck
```

**Key dependencies:** ibus-devel, libpinyin-devel, sqlite-devel, lua-devel, libsoup3-devel, json-glib-devel, opencc-devel, libnotify-devel

## Container Testing

```bash
podman build -t ibus-smartpinyin-dev .
podman run --rm -d --name ibus-test -p 6080:6080 --security-opt label=disable ibus-smartpinyin-dev
# Access via browser at http://localhost:6080 (noVNC → Xvfb + IBus + mousepad)
```

## Key Identifiers

| What | Value |
|------|-------|
| IBus engine name | `smartpinyin` / `smartbopomofo` |
| IBus component | `org.freedesktop.IBus.SmartPinyin` |
| GSettings schema | `com.github.xierongchuan.ibus-smartpinyin.*` |
| Engine binary | `ibus-engine-smartpinyin` |
| Setup binary | `ibus-setup-smartpinyin` |
| User data dir | `~/.cache/ibus/smartpinyin/` |

## Architecture

### Engine Layer (GObject ↔ C++)

`PYMain.cc` is the entry point. It initializes `LibPinyinBackEnd`, `UserPhraseDatabase`, config singletons, and registers the IBus engine factory.

Two concrete engines exist: `PinyinEngine` and `BopomofoEngine` (both in `src/`), wrapping C++ classes in GObject callbacks for IBus.

### Editor System

Each engine maintains an array of editors indexed by mode:

| Mode | Editor | Trigger |
|------|--------|---------|
| MODE_INIT | FullPinyinEditor / DoublePinyinEditor / BopomofoEditor | Default |
| MODE_PUNCT | PunctEditor | Punctuation key |
| MODE_RAW | RawEditor | Direct input |
| MODE_ENGLISH | EnglishEditor | `v` key |
| MODE_TABLE | TableEditor | `u` key |
| MODE_EXTENSION | ExtEditor (Lua) | `i` key |
| MODE_SUGGESTION | SuggestionEditor | After commit |

Editors inherit: `Editor` → `PhoneticEditor` → `PinyinEditor` → `FullPinyinEditor` / `DoublePinyinEditor`

Editors communicate with the engine via a signal/slot system (`PYSignal.h`) — signals for commit text, preedit, lookup table updates, etc.

### Candidate Pipeline

`PhoneticEditor::updateCandidates()` calls each provider's `processCandidates()` to populate a shared `m_candidates` vector. All providers inherit `EnhancedCandidates<IEditor>` template.

Providers (in order): LibPinyinCandidates → UserPhraseCandidates → EmojiCandidates → EnglishCandidates → TraditionalCandidates → CloudCandidates → LuaCandidates

Each candidate has a `CandidateType` enum value used for dispatch in `selectCandidate()`.

### Conditional Features

Optional features are guarded by `#ifdef` and controlled by `configure.ac` flags:

| Define | Feature | Default |
|--------|---------|---------|
| `IBUS_BUILD_LUA_EXTENSION` | Lua scripting | yes |
| `ENABLE_CLOUD_INPUT_MODE` | Cloud candidates | no |
| `IBUS_BUILD_ENGLISH_INPUT_MODE` | English words | yes |
| `IBUS_BUILD_TABLE_INPUT_MODE` | Table input (wubi) | yes |
| `HAVE_OPENCC` | Simp/Trad conversion | no |
| `ENABLE_LIBNOTIFY` | Notifications | yes |

Both `src/Makefile.am` and source files use these defines. When adding code that touches optional features, wrap it in the appropriate `#ifdef`.

### Key Subsystems

- **libpinyin wrapper** (`PYLibPinyin.h/cc`): Singleton `LibPinyinBackEnd` manages pinyin instances and options. Core API: `pinyin_parse_more_full_pinyins`, `pinyin_guess_candidates`, `pinyin_choose_candidate`.
- **User phrase DB** (`PYUserPhraseDatabase.h/cc`): SQLite at `~/.cache/ibus/smartpinyin/user-phrases.db`. Tables: `user_phrases` (phrase, full_pinyin, freq) and `syllables` (per-syllable index). Queried via syllable prefix matching.
- **Config** (`PYPConfig.h/cc`): GSettings-backed. Schema in `data/com.github.xierongchuan.ibus-smartpinyin.gschema.xml`.
- **Setup UI** (`setup/main2.py`): Python GTK3 preferences dialog.

### Input Flow

1. Keypress → `Engine::processKeyEvent()` → active `Editor::processKeyEvent()`
2. Editor calls `insert()` → `updatePinyin()` (parses with libpinyin, caches syllable fragments in `m_pinyin_fragments`)
3. `updateCandidates()` → all providers populate `m_candidates`
4. `fillLookupTable()` → signal → IBus displays candidates
5. User selects → `selectCandidate()` dispatches to provider → learning + commit

## File Naming Convention

All source files use `PY` prefix. Pinyin-specific files add `P` (`PYP*`). Header/implementation pairs (`.h`/`.cc`).
