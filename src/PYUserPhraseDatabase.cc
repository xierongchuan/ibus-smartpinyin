/* vim:set et ts=4 sts=4:
 *
 * ibus-smartpinyin - Smart Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2024 Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PYUserPhraseDatabase.h"
#include <time.h>
#include <stdio.h>
#include <stdarg.h>

namespace PY {

static void
debug_log (const char *fmt, ...)
{
    FILE *f = fopen ("/tmp/user-phrase-debug.log", "a");
    if (!f) return;
    time_t now = time (NULL);
    struct tm *t = localtime (&now);
    fprintf (f, "[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
    va_list ap;
    va_start (ap, fmt);
    vfprintf (f, fmt, ap);
    va_end (ap);
    fprintf (f, "\n");
    fclose (f);
}

std::unique_ptr<UserPhraseDatabase> UserPhraseDatabase::m_instance;

void
UserPhraseDatabase::init ()
{
    if (m_instance.get () == NULL) {
        m_instance.reset (new UserPhraseDatabase ());
        m_instance->openDatabase ();
    }
}

UserPhraseDatabase::UserPhraseDatabase ()
    : m_sqlite (NULL)
{
}

UserPhraseDatabase::~UserPhraseDatabase ()
{
    if (m_sqlite) {
        sqlite3_close (m_sqlite);
        m_sqlite = NULL;
    }
}

gboolean
UserPhraseDatabase::openDatabase ()
{
    gchar *path = g_build_filename (g_get_user_cache_dir (),
                                    "ibus", "smartpinyin",
                                    "user-phrases.db", NULL);

    char *dirname = g_path_get_dirname (path);
    g_mkdir_with_parents (dirname, 0700);
    g_free (dirname);

    gboolean need_create = !g_file_test (path, G_FILE_TEST_IS_REGULAR);

    if (sqlite3_open_v2 (path, &m_sqlite,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                         NULL) != SQLITE_OK) {
        g_warning ("can't open user phrase database: %s", path);
        g_free (path);
        m_sqlite = NULL;
        return FALSE;
    }

    debug_log ("UserPhraseDB: opened %s (need_create=%d)", path, need_create);
    g_free (path);

    /* always enable foreign keys on every open, not just on create */
    executeSQL ("PRAGMA foreign_keys = ON;");

    if (need_create) {
        return createDatabase ();
    }

    return TRUE;
}

gboolean
UserPhraseDatabase::createDatabase ()
{
    const char *schema =
        "CREATE TABLE IF NOT EXISTS user_phrases ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  phrase TEXT NOT NULL,"
        "  full_pinyin TEXT NOT NULL,"
        "  syllable_count INTEGER,"
        "  freq INTEGER DEFAULT 1,"
        "  last_used INTEGER,"
        "  UNIQUE(phrase, full_pinyin)"
        ");"
        "CREATE TABLE IF NOT EXISTS syllables ("
        "  phrase_id INTEGER NOT NULL,"
        "  position INTEGER NOT NULL,"
        "  syllable TEXT NOT NULL,"
        "  PRIMARY KEY (phrase_id, position),"
        "  FOREIGN KEY (phrase_id) REFERENCES user_phrases(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_syllables_prefix ON syllables(syllable, position);"
        "CREATE INDEX IF NOT EXISTS idx_phrases_freq ON user_phrases(freq DESC);";

    return executeSQL (schema);
}

gboolean
UserPhraseDatabase::executeSQL (const char *sql)
{
    if (m_sqlite == NULL)
        return FALSE;

    gchar *errmsg = NULL;
    if (sqlite3_exec (m_sqlite, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
        g_warning ("SQL error: %s: %s", errmsg, sql);
        sqlite3_free (errmsg);
        return FALSE;
    }
    return TRUE;
}

gboolean
UserPhraseDatabase::learnPhrase (const gchar *phrase,
                                 const std::vector<std::string> &syllables)
{
    if (m_sqlite == NULL || phrase == NULL || syllables.empty ())
        return FALSE;

    /* build full_pinyin from syllables: "xie'rong'chuan" */
    std::string full_pinyin;
    for (size_t i = 0; i < syllables.size (); i++) {
        if (i > 0)
            full_pinyin += "'";
        full_pinyin += syllables[i];
    }

    time_t now = time (NULL);

    /* try to update existing phrase first */
    sqlite3_stmt *stmt = NULL;
    const char *update_sql =
        "UPDATE user_phrases SET freq = freq + 1, last_used = ? "
        "WHERE phrase = ? AND full_pinyin = ?;";

    if (sqlite3_prepare_v2 (m_sqlite, update_sql, -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    sqlite3_bind_int64 (stmt, 1, (sqlite3_int64)now);
    sqlite3_bind_text (stmt, 2, phrase, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 3, full_pinyin.c_str (), -1, SQLITE_TRANSIENT);

    sqlite3_step (stmt);
    int changes = sqlite3_changes (m_sqlite);
    sqlite3_finalize (stmt);

    if (changes > 0)
        return TRUE;

    /* insert new phrase */
    executeSQL ("BEGIN TRANSACTION;");

    const char *insert_sql =
        "INSERT OR IGNORE INTO user_phrases (phrase, full_pinyin, syllable_count, freq, last_used) "
        "VALUES (?, ?, ?, 1, ?);";

    if (sqlite3_prepare_v2 (m_sqlite, insert_sql, -1, &stmt, NULL) != SQLITE_OK) {
        executeSQL ("ROLLBACK;");
        return FALSE;
    }

    sqlite3_bind_text (stmt, 1, phrase, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, full_pinyin.c_str (), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 3, (int)syllables.size ());
    sqlite3_bind_int64 (stmt, 4, (sqlite3_int64)now);

    if (sqlite3_step (stmt) != SQLITE_DONE) {
        sqlite3_finalize (stmt);
        executeSQL ("ROLLBACK;");
        return FALSE;
    }
    sqlite3_finalize (stmt);

    /* verify the insert actually happened (INSERT OR IGNORE may skip) */
    if (sqlite3_changes (m_sqlite) == 0) {
        debug_log ("UserPhraseDB: phrase '%s' already exists, skipping", phrase);
        executeSQL ("COMMIT;");
        return TRUE;
    }

    sqlite3_int64 phrase_id = sqlite3_last_insert_rowid (m_sqlite);

    /* insert syllables */
    const char *syllable_sql =
        "INSERT INTO syllables (phrase_id, position, syllable) VALUES (?, ?, ?);";

    for (size_t i = 0; i < syllables.size (); i++) {
        if (sqlite3_prepare_v2 (m_sqlite, syllable_sql, -1, &stmt, NULL) != SQLITE_OK) {
            executeSQL ("ROLLBACK;");
            return FALSE;
        }

        sqlite3_bind_int64 (stmt, 1, phrase_id);
        sqlite3_bind_int (stmt, 2, (int)i);
        sqlite3_bind_text (stmt, 3, syllables[i].c_str (), -1, SQLITE_TRANSIENT);

        if (sqlite3_step (stmt) != SQLITE_DONE) {
            sqlite3_finalize (stmt);
            executeSQL ("ROLLBACK;");
            return FALSE;
        }
        sqlite3_finalize (stmt);
    }

    executeSQL ("COMMIT;");
    debug_log ("UserPhraseDB: learned '%s' pinyin='%s' id=%lld syllables=%zu",
               phrase, full_pinyin.c_str (), (long long)phrase_id, syllables.size ());
    return TRUE;
}

gboolean
UserPhraseDatabase::matchPhrases (const std::vector<std::string> &fragments,
                                  std::vector<UserPhrase> &results,
                                  int limit)
{
    results.clear ();

    if (m_sqlite == NULL || fragments.empty ())
        return FALSE;

    int n = (int)fragments.size ();

    /*
     * Build dynamic SQL:
     * SELECT p.phrase, p.full_pinyin, p.freq
     * FROM user_phrases p
     * JOIN syllables s0 ON s0.phrase_id = p.id AND s0.position = 0
     * JOIN syllables s1 ON s1.phrase_id = p.id AND s1.position = 1
     * ...
     * WHERE p.syllable_count = N
     *   AND s0.syllable LIKE 'x%'
     *   AND s1.syllable LIKE 'r%'
     *   ...
     * ORDER BY p.freq DESC, p.last_used DESC
     * LIMIT ?;
     */
    std::string sql = "SELECT p.phrase, p.full_pinyin, p.freq FROM user_phrases p";

    for (int i = 0; i < n; i++) {
        char join[128];
        g_snprintf (join, sizeof(join),
                    " JOIN syllables s%d ON s%d.phrase_id = p.id AND s%d.position = %d",
                    i, i, i, i);
        sql += join;
    }

    sql += " WHERE p.syllable_count = ?";

    for (int i = 0; i < n; i++) {
        char where[64];
        g_snprintf (where, sizeof(where), " AND s%d.syllable LIKE ?", i);
        sql += where;
    }

    sql += " ORDER BY p.freq DESC, p.last_used DESC LIMIT ?;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2 (m_sqlite, sql.c_str (), -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }

    /* bind parameters */
    int param = 1;
    sqlite3_bind_int (stmt, param++, n);

    for (int i = 0; i < n; i++) {
        std::string pattern = fragments[i] + "%";
        sqlite3_bind_text (stmt, param++, pattern.c_str (), -1, SQLITE_TRANSIENT);
    }

    sqlite3_bind_int (stmt, param, limit);

    /* fetch results */
    while (sqlite3_step (stmt) == SQLITE_ROW) {
        UserPhrase up;
        const char *phrase = (const char *)sqlite3_column_text (stmt, 0);
        const char *pinyin = (const char *)sqlite3_column_text (stmt, 1);
        int freq = sqlite3_column_int (stmt, 2);

        if (phrase) up.phrase = phrase;
        if (pinyin) up.full_pinyin = pinyin;
        up.freq = freq;

        results.push_back (up);
    }

    sqlite3_finalize (stmt);
    debug_log ("UserPhraseDB: matchPhrases fragments=%zu results=%zu",
               fragments.size (), results.size ());
    return !results.empty ();
}

gboolean
UserPhraseDatabase::removePhrase (const gchar *phrase, const gchar *full_pinyin)
{
    if (m_sqlite == NULL)
        return FALSE;

    executeSQL ("PRAGMA foreign_keys = ON;");

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "DELETE FROM user_phrases WHERE phrase = ? AND full_pinyin = ?;";

    if (sqlite3_prepare_v2 (m_sqlite, sql, -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    sqlite3_bind_text (stmt, 1, phrase, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, full_pinyin, -1, SQLITE_TRANSIENT);

    sqlite3_step (stmt);
    int changes = sqlite3_changes (m_sqlite);
    sqlite3_finalize (stmt);

    return changes > 0;
}

gboolean
UserPhraseDatabase::incrementFrequency (const gchar *phrase, const gchar *full_pinyin)
{
    if (m_sqlite == NULL)
        return FALSE;

    time_t now = time (NULL);

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "UPDATE user_phrases SET freq = freq + 1, last_used = ? "
        "WHERE phrase = ? AND full_pinyin = ?;";

    if (sqlite3_prepare_v2 (m_sqlite, sql, -1, &stmt, NULL) != SQLITE_OK)
        return FALSE;

    sqlite3_bind_int64 (stmt, 1, (sqlite3_int64)now);
    sqlite3_bind_text (stmt, 2, phrase, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 3, full_pinyin, -1, SQLITE_TRANSIENT);

    sqlite3_step (stmt);
    int changes = sqlite3_changes (m_sqlite);
    sqlite3_finalize (stmt);

    return changes > 0;
}

};
