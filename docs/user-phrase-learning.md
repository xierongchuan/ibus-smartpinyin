# User Phrase Learning & Matching System

## Problem

ibus-libpinyin relies on the libpinyin library for its candidate dictionary. While libpinyin has a built-in user dictionary that learns from selections, it has significant limitations:

1. **Custom words/names** (e.g. 谢榕川) are not always learnable — the user can select individual characters, but the system doesn't reliably persist the combination as a phrase.
2. **Abbreviated pinyin** (e.g. `xrc` for 谢榕川) never suggests previously learned phrases, because libpinyin's user dictionary doesn't support prefix-based matching on individual syllables.
3. **Frequency-based ranking** is opaque and hard to control — even after selecting a phrase many times, it may not rise to the top of the candidate list.

## Solution Overview

A custom **UserPhraseDatabase** (SQLite) stores learned phrases with their pinyin syllables decomposed into individual rows. This enables:

- **Prefix matching**: typing `xrc` matches syllables `x%`, `r%`, `c%` against stored syllables `xie`, `rong`, `chuan`.
- **Frequency tracking**: each selection increments a counter; higher-frequency phrases are promoted higher in the candidate list.
- **Promotion of built-in words**: phrases that already exist in libpinyin's dictionary (e.g. 形容词) can be moved to the top of the list based on user frequency data.

## Architecture

```
┌──────────────────┐     ┌─────────────────────┐
│  User types      │     │  FullPinyinEditor    │
│  "xierongchuan"  │────>│  updatePinyin()      │
│                  │     │  - parse pinyin       │
│                  │     │  - cache fragments    │
│                  │     │    in m_pinyin_       │
│                  │     │    fragments          │
└──────────────────┘     └─────────┬─────────────┘
                                   │
                                   v
                         ┌─────────────────────┐
                         │  PhoneticEditor      │
                         │  update()            │
                         │  - guess_candidates  │
                         │  - updateCandidates  │
                         └─────────┬─────────────┘
                                   │
              ┌────────────────────┼────────────────────┐
              v                    v                    v
   ┌──────────────────┐  ┌──────────────────┐  ┌──────────────┐
   │LibPinyinCandidates│  │UserPhraseCandidates│  │Other providers│
   │processCandidates()│  │processCandidates()│  │(Emoji, Cloud, │
   │                  │  │- read cached       │  │ English, etc.)│
   │                  │  │  fragments          │  │              │
   │                  │  │- query SQLite DB    │  │              │
   │                  │  │- promote/insert     │  │              │
   └──────────────────┘  └──────────────────┘  └──────────────┘
              │                    │
              v                    v
   ┌──────────────────┐  ┌──────────────────┐
   │selectCandidate() │  │selectCandidate() │
   │- rememberUser    │  │- incrementFreq   │
   │  Input()         │  │  in SQLite DB    │
   │- learnPhrase()   │  │                  │
   │  in SQLite DB    │  │                  │
   └──────────────────┘  └──────────────────┘
```

### New Files

| File | Purpose |
|------|---------|
| `src/PYUserPhraseDatabase.h` | SQLite database class declaration |
| `src/PYUserPhraseDatabase.cc` | SQLite database implementation |
| `src/PYPUserPhraseCandidates.h` | Candidate provider class declaration |
| `src/PYPUserPhraseCandidates.cc` | Candidate matching & promotion logic |

### Modified Files

| File | Changes |
|------|---------|
| `src/PYLibPinyin.cc` | Added `extractSyllablesFromInstance()`, learning in `rememberUserInput()`/`rememberCloudInput()` |
| `src/PYPPhoneticEditor.h` | Added `m_pinyin_fragments` field, `friend class UserPhraseCandidates` |
| `src/PYPPhoneticEditor.cc` | Added `UserPhraseCandidates` to candidate pipeline, `CANDIDATE_USER_PHRASE` to select/remove dispatch |
| `src/PYPFullPinyinEditor.cc` | Fragment caching in `updatePinyin()` |
| `src/PYPLibPinyinCandidates.cc` | Added learning calls for LONGER and SORT_WITHOUT_SENTENCE candidate paths |
| `src/PYPEnhancedCandidates.h` | Added `CANDIDATE_USER_PHRASE` enum value |
| `src/PYPConfig.cc` | Added `rememberEveryInput` config option |
| `data/com.github.libpinyin.ibus-libpinyin.gschema.xml` | Added `remember-every-input` GSettings key |
| `src/Makefile.am` | Added new source files to build |

## SQLite Database Schema

Location: `~/.cache/ibus/libpinyin/user-phrases.db`

```sql
CREATE TABLE user_phrases (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    phrase TEXT NOT NULL,           -- e.g. "谢榕川"
    full_pinyin TEXT NOT NULL,     -- e.g. "xie'rong'chuan"
    syllable_count INTEGER,        -- e.g. 3
    freq INTEGER DEFAULT 1,        -- selection count
    last_used INTEGER,             -- unix timestamp
    UNIQUE(phrase, full_pinyin)
);

CREATE TABLE syllables (
    phrase_id INTEGER NOT NULL,
    position INTEGER NOT NULL,     -- 0-indexed syllable position
    syllable TEXT NOT NULL,        -- e.g. "xie", "rong", "chuan"
    PRIMARY KEY (phrase_id, position),
    FOREIGN KEY (phrase_id) REFERENCES user_phrases(id) ON DELETE CASCADE
);

CREATE INDEX idx_syllables_prefix ON syllables(syllable, position);
CREATE INDEX idx_phrases_freq ON user_phrases(freq DESC);
```

For example, learning 谢榕川 from input `xierongchuan` produces:

| Table | Data |
|-------|------|
| `user_phrases` | `id=1, phrase="谢榕川", full_pinyin="xie'rong'chuan", syllable_count=3, freq=1` |
| `syllables` | `(1, 0, "xie"), (1, 1, "rong"), (1, 2, "chuan")` |

## Syllable Extraction

### The Problem

When the user types `xierongchuan`, the raw text `m_text` contains no apostrophes — it's a continuous string. Naive splitting by `'` produces a single element `["xierongchuan"]`, making it impossible to identify individual syllables.

### The Solution: `pinyin_get_right_pinyin_offset`

After `pinyin_parse_more_full_pinyins()` parses the input, the libpinyin instance knows the syllable boundaries internally. We extract them using `pinyin_get_right_pinyin_offset()`:

```cpp
static std::vector<std::string>
extractSyllablesFromInstance (pinyin_instance_t *instance, const gchar *raw_text)
{
    std::vector<std::string> syllables;
    size_t len = strlen (raw_text);
    size_t parsed = pinyin_get_parsed_input_length (instance);

    size_t begin = 0;
    while (begin < parsed && begin < len) {
        size_t end = 0;
        if (!pinyin_get_right_pinyin_offset (instance, begin, &end))
            break;
        if (end <= begin)
            break;
        if (end > len)
            end = len;
        syllables.push_back (std::string (raw_text + begin, end - begin));
        begin = end;
    }
    return syllables;
}
```

This walks the offset chain: `0 → 3 → 7 → 12`, extracting `["xie", "rong", "chuan"]`.

Works for all input forms:
- `xierongchuan` → `["xie", "rong", "chuan"]`
- `xie'rong'chuan` → `["xie", "rong", "chuan"]`
- `xrc` → `["x", "r", "c"]`

### Why Not `pinyin_get_pinyin_key_rest`?

The `pinyin_get_pinyin_key_rest(instance, index, &pos)` API only returns parsed **keys**, not syllable boundaries in the raw text. For continuous full pinyin like `xierongchuan`, it returns only 1 key (the first parsed syllable), not 3. It works for abbreviated input (`xrc` → 3 keys) but fails for the common case. `pinyin_get_right_pinyin_offset` correctly walks all syllable boundaries regardless of input form.

## Fragment Caching

### The Problem

The `updatePinyin()` → `update()` → `updateCandidates()` call chain has a critical ordering issue:

1. `updatePinyin()` calls `pinyin_parse_more_full_pinyins()` — syllable offsets are valid here.
2. `update()` calls `pinyin_guess_candidates()` — this **invalidates** pinyin key rest positions.
3. `updateCandidates()` calls `UserPhraseCandidates::processCandidates()` — too late to read syllable offsets.

### The Solution

Cache the pinyin fragments in `m_pinyin_fragments` (a `std::vector<std::string>` field on `PhoneticEditor`) during `updatePinyin()`, **before** `pinyin_guess_candidates` runs:

```cpp
// In FullPinyinEditor::updatePinyin():
m_pinyin_fragments.clear ();
size_t frag_begin = 0;
size_t text_len = m_text.length ();
size_t parsed_len = pinyin_get_parsed_input_length (m_instance);
while (frag_begin < parsed_len && frag_begin < text_len) {
    size_t frag_end = 0;
    if (!pinyin_get_right_pinyin_offset (m_instance, frag_begin, &frag_end))
        break;
    if (frag_end <= frag_begin)
        break;
    if (frag_end > text_len)
        frag_end = text_len;
    m_pinyin_fragments.push_back (
        std::string (m_text.c_str () + frag_begin, frag_end - frag_begin));
    frag_begin = frag_end;
}
```

`UserPhraseCandidates::processCandidates()` then reads from `m_editor->m_pinyin_fragments` instead of querying the libpinyin instance directly.

## Learning Flow

Learning happens in `LibPinyinBackEnd::rememberUserInput()`, which is called from `LibPinyinCandidates::selectCandidate()` when a candidate is selected.

```
User selects candidate
    │
    v
LibPinyinCandidates::selectCandidate()
    │
    ├── For NBEST_MATCH: pinyin_choose_candidate → pinyin_get_sentence → rememberUserInput
    ├── For LONGER/LONGER_USER: pinyin_choose_candidate → rememberUserInput
    ├── For SORT_WITHOUT_SENTENCE: pinyin_choose_candidate → rememberUserInput
    └── For NORMAL at end: pinyin_choose_candidate → pinyin_get_sentence → rememberUserInput
    │
    v
LibPinyinBackEnd::rememberUserInput(instance, pinyin, phrase)
    │
    ├── 1. pinyin_reset(instance)              // clean state
    ├── 2. pinyin_parse_more_full_pinyins()    // re-parse
    ├── 3. extractSyllablesFromInstance()       // extract BEFORE mutation
    ├── 4. pinyin_remember_user_input()         // save to libpinyin's dict
    └── 5. UserPhraseDatabase::learnPhrase()   // save to custom SQLite DB
```

Key ordering constraint: `extractSyllablesFromInstance()` must run **before** `pinyin_remember_user_input()`, which may mutate the instance and invalidate pinyin offsets.

### `learnPhrase()` Logic

```
learnPhrase(phrase, syllables)
    │
    ├── Build full_pinyin: "xie" + "'" + "rong" + "'" + "chuan"
    │
    ├── Try UPDATE existing: freq = freq + 1
    │   └── If changes > 0: return (phrase already known, frequency bumped)
    │
    └── INSERT new phrase + syllable rows in transaction
        ├── INSERT OR IGNORE into user_phrases
        ├── Check sqlite3_changes() > 0 (guard against OR IGNORE skip)
        ├── Get last_insert_rowid
        └── INSERT syllable rows: (phrase_id, position, syllable)
```

## Matching Flow

Matching happens in `UserPhraseCandidates::processCandidates()`, which runs during the candidate update pipeline.

```
updateCandidates()
    │
    ├── LibPinyinCandidates::processCandidates()   // fills base candidates
    │
    └── UserPhraseCandidates::processCandidates()
        │
        ├── Read cached m_pinyin_fragments: ["x", "r", "c"]
        │   (requires >= 2 fragments, single-syllable input is skipped)
        │
        ├── UserPhraseDatabase::matchPhrases(fragments)
        │   │
        │   └── Dynamic SQL query:
        │       SELECT p.phrase, p.full_pinyin, p.freq
        │       FROM user_phrases p
        │       JOIN syllables s0 ON s0.phrase_id=p.id AND s0.position=0
        │       JOIN syllables s1 ON s1.phrase_id=p.id AND s1.position=1
        │       JOIN syllables s2 ON s2.phrase_id=p.id AND s2.position=2
        │       WHERE p.syllable_count = 3
        │         AND s0.syllable LIKE 'x%'
        │         AND s1.syllable LIKE 'r%'
        │         AND s2.syllable LIKE 'c%'
        │       ORDER BY p.freq DESC, p.last_used DESC
        │       LIMIT 10
        │
        └── Promote/insert matched phrases into candidate list
```

### Matching Examples

| Input | Fragments | Matches |
|-------|-----------|---------|
| `xierongchuan` | `["xie", "rong", "chuan"]` | 谢榕川 (exact match) |
| `xrc` | `["x", "r", "c"]` | 谢榕川, 形容词 (prefix match) |
| `xie'r'c` | `["xie", "r", "c"]` | 谢榕川, 形容词 (mixed match) |
| `ni` | `["ni"]` | *(skipped — single syllable)* |

## Candidate Promotion Logic

After `matchPhrases()` returns results sorted by `freq DESC`, each matched phrase is either promoted (if already in the candidate list from libpinyin) or inserted as a new entry.

### Frequency-Based Positioning

```
For each matched phrase (ordered by freq DESC):
    │
    ├── freq >= 3:  target = position 0 (top of list)
    └── freq < 3:   target = after first NBEST_MATCH candidate
    │
    target += count   // offset by previously promoted phrases
    │
    ├── If phrase already exists in candidate list:
    │   ├── If current position > target: MOVE it up to target
    │   └── If current position <= target: leave in place
    │   └── Always increment count
    │
    └── If phrase is new (not in libpinyin's dictionary):
        ├── Create EnhancedCandidate with type CANDIDATE_USER_PHRASE
        ├── Insert at target position
        └── Increment count
```

### Why Move Instead of Skip?

Built-in dictionary words like 形容词 already appear in the candidate list from `LibPinyinCandidates`. Without the move logic, selecting 形容词 via `xrc` would increment its frequency in the SQLite DB but never change its position in the candidate list — it would stay wherever libpinyin put it (often position 3-4).

By moving existing candidates up to the target position, frequently-used built-in words are promoted to the top just like custom user phrases.

### Why Always Increment Count?

Consider two phrases matched for `xrc`:
1. 形容词 (freq=20) — target position 0
2. 谢榕川 (freq=6) — target position 0

Without the count offset, both would try to insert at position 0, and 谢榕川 would end up before 形容词. With `count`, 谢榕川 targets position 1, maintaining the correct frequency-based order.

Even when a phrase is already at or before its target position (e.g. 形容词 is already at position 0 from libpinyin's NBEST), `count` must still be incremented so that subsequent lower-frequency phrases insert after it.

## Selecting User Phrase Candidates

When a `CANDIDATE_USER_PHRASE` is selected:

```cpp
int
UserPhraseCandidates::selectCandidate (EnhancedCandidate & enhanced)
{
    // Increment frequency in SQLite DB
    UserPhraseDatabase::instance ().incrementFrequency (
        up.phrase.c_str (), up.full_pinyin.c_str ());

    return SELECT_CANDIDATE_DIRECT_COMMIT;  // commit text and reset
}
```

When a promoted built-in candidate is selected (its type remains `CANDIDATE_NBEST_MATCH`, `CANDIDATE_NORMAL`, etc.), the selection goes through `LibPinyinCandidates::selectCandidate()`, which calls `rememberUserInput()` → `learnPhrase()`, incrementing the frequency.

## Removing User Phrase Candidates

User phrases can be removed with `Ctrl+D`:

```cpp
gboolean
UserPhraseCandidates::removeCandidate (EnhancedCandidate & enhanced)
{
    UserPhraseDatabase::instance ().removePhrase (
        up.phrase.c_str (), up.full_pinyin.c_str ());
    return TRUE;  // triggers updatePinyin + update
}
```

The SQLite `ON DELETE CASCADE` foreign key ensures syllable rows are automatically deleted when the phrase row is removed.

## Visual Distinction

User phrase candidates and custom user phrases are displayed in blue in the lookup table:

```cpp
// In PhoneticEditor::fillLookupTable():
if (CANDIDATE_USER == candidate.m_candidate_type ||
    CANDIDATE_USER_PHRASE == candidate.m_candidate_type)
    text.appendAttribute (IBUS_ATTR_TYPE_FOREGROUND, 0x000000ef, 0, -1);
```

## Configuration

The `remember-every-input` GSettings key controls whether every candidate selection triggers learning:

```xml
<key name="remember-every-input" type="b">
    <default>true</default>
    <summary>Remember every input</summary>
</key>
```

When enabled, all candidate types (NBEST_MATCH, LONGER, NORMAL, etc.) trigger `rememberUserInput()`, which saves to both libpinyin's built-in user dictionary and the custom SQLite database.

## Debug Logging

All components write to `/tmp/user-phrase-debug.log` using a file-based logger. This is necessary because ibus-daemon with the `-d` flag redirects the engine's stdout/stderr to `/dev/null`.

Log entries include timestamps and cover:
- Database open/create operations
- Phrase learning with syllable details
- Phrase matching with fragment and result counts
- Candidate promotion with from/to positions
- Syllable extraction details
- `rememberUserInput` calls with pinyin and phrase

Example log output:
```
[14:23:01] rememberUserInput: pinyin='xierongchuan' phrase='谢榕川'
[14:23:01] rememberUserInput: parsed=12 chars
[14:23:01]   extractSyllable[0]: 0-3 'xie'
[14:23:01]   extractSyllable[1]: 3-7 'rong'
[14:23:01]   extractSyllable[2]: 7-12 'chuan'
[14:23:01] extractSyllables: raw='xierongchuan' count=3
[14:23:01] learning '谢榕川' pinyin='xierongchuan' syllables=3
[14:23:01] UserPhraseDB: learned '谢榕川' pinyin='xie'rong'chuan' id=1 syllables=3
[14:23:05] processCandidates input='xrc' fragments=3
[14:23:05] UserPhraseDB: matchPhrases fragments=3 results=2
[14:23:05]   promote '形容词' freq=20 from=0 to=0
[14:23:05]   promote '谢榕川' freq=6 from=5 to=1
```

## Key Bugs Discovered and Fixed

### 1. Wrong API for Syllable Extraction
- **Bug**: `pinyin_get_pinyin_key_rest` returns only 1 key for continuous full pinyin (`xierongchuan`), not 3.
- **Fix**: Use `pinyin_get_right_pinyin_offset` which correctly walks syllable boundaries for all input forms.

### 2. Instance State Invalidation
- **Bug**: `pinyin_guess_candidates()` invalidates pinyin key positions, but `processCandidates()` runs after it.
- **Fix**: Cache fragments in `m_pinyin_fragments` during `updatePinyin()`, before `pinyin_guess_candidates` is called.

### 3. Dirty Instance in `rememberUserInput`
- **Bug**: `pinyin_choose_candidate()` mutates the instance before `rememberUserInput()` is called, leaving it in a consumed state.
- **Fix**: Call `pinyin_reset(instance)` followed by `pinyin_parse_more_full_pinyins()` at the start of `rememberUserInput()`.

### 4. Extract Before Mutation
- **Bug**: `extractSyllablesFromInstance()` was called after `pinyin_remember_user_input()`, which may corrupt the instance state.
- **Fix**: Move extraction before `pinyin_remember_user_input()`.

### 5. LONGER Candidates Not Learned
- **Bug**: Only NBEST_MATCH and end-of-input NORMAL candidates called `rememberUserInput()`. LONGER and SORT_WITHOUT_SENTENCE paths were missing learning calls.
- **Fix**: Added `rememberUserInput()` calls to all candidate selection paths.

### 6. SQLite PRAGMA Not Persistent
- **Bug**: `PRAGMA foreign_keys = ON` was only set during `createDatabase()`, not on subsequent opens. Foreign key cascade deletes silently failed.
- **Fix**: Move PRAGMA to `openDatabase()` so it runs on every connection.

### 7. INSERT OR IGNORE + last_insert_rowid Race
- **Bug**: If `INSERT OR IGNORE` silently skips (duplicate), `sqlite3_last_insert_rowid()` returns the ID of a previous insert, causing syllable rows to be attached to the wrong phrase.
- **Fix**: Check `sqlite3_changes() == 0` after the insert; if no row was inserted, skip the syllable insertion.

### 8. Frequency Competition Bug
- **Bug**: When a phrase was already at its target position (e.g. from libpinyin's NBEST), `count` was not incremented, so lower-frequency phrases would insert before it.
- **Fix**: Always increment `count` in the found/promote branch, even when no move is performed.
