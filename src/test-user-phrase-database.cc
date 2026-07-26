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

/*
 * Regression test for the user phrase database, which implements the
 * feature this fork exists for: an input typed as the first letters of
 * each pinyin syllable (z'h'r'm'g'h'g) has to be remembered and offered
 * again as a candidate (中华人民共和国).
 *
 * The test drives UserPhraseDatabase directly - no IBus session, no
 * libpinyin data files - so it can run unattended in `make check`.
 * XDG_CACHE_HOME is redirected to a throw-away directory, so the
 * developer's real ~/.cache/ibus/smartpinyin/user-phrases.db is never
 * touched.
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "PYDebugLog.h"
#include "PYUserPhraseDatabase.h"

using namespace PY;

static int n_checks = 0;
static int n_failures = 0;

#define CHECK(expr)                                                     \
    do {                                                                \
        n_checks++;                                                     \
        if (expr) {                                                     \
            g_print ("ok %d - %s\n", n_checks, #expr);                  \
        } else {                                                        \
            n_failures++;                                               \
            g_print ("not ok %d - %s\n", n_checks, #expr);              \
            g_printerr ("  FAILED at %s:%d\n", __FILE__, __LINE__);     \
        }                                                               \
    } while (0)

static std::vector<std::string>
syllables (const gchar *space_separated)
{
    std::vector<std::string> result;
    gchar **parts = g_strsplit (space_separated, " ", -1);
    for (gchar **p = parts; *p != NULL; p++)
        result.push_back (*p);
    g_strfreev (parts);
    return result;
}

/* index of a phrase in a match result, or -1 */
static int
index_of (const std::vector<UserPhrase> &results, const gchar *phrase)
{
    for (size_t i = 0; i < results.size (); i++) {
        if (results[i].phrase == phrase)
            return (int)i;
    }
    return -1;
}

/* out-of-range accessors, so that a regression that returns too few
   matches is reported as a failed check instead of crashing the run */
static std::string
phrase_at (const std::vector<UserPhrase> &results, size_t i)
{
    return i < results.size () ? results[i].phrase : std::string ("<missing>");
}

static std::string
pinyin_at (const std::vector<UserPhrase> &results, size_t i)
{
    return i < results.size () ? results[i].full_pinyin : std::string ("<missing>");
}

static int
freq_at (const std::vector<UserPhrase> &results, size_t i)
{
    return i < results.size () ? results[i].freq : -1;
}

static void
test_learn_and_match_by_initials (UserPhraseDatabase &db)
{
    g_print ("# learning a phrase and recalling it by its initials\n");

    CHECK (db.learnPhrase ("中华人民共和国",
                           syllables ("zhong hua ren min gong he guo")));

    std::vector<UserPhrase> results;

    /* the whole point of the fork: z'h'r'm'g'h'g must find the phrase */
    CHECK (db.matchPhrases (syllables ("z h r m g h g"), results));
    CHECK (results.size () == 1);
    CHECK (phrase_at (results, 0) == "中华人民共和国");
    CHECK (pinyin_at (results, 0) == "zhong'hua'ren'min'gong'he'guo");
    CHECK (freq_at (results, 0) == 1);

    /* the full spelling and any partial prefix must work too */
    CHECK (db.matchPhrases (syllables ("zhong hua ren min gong he guo"), results));
    CHECK (results.size () == 1);
    CHECK (db.matchPhrases (syllables ("zh hu r mi g he g"), results));
    CHECK (results.size () == 1);

    /* a different syllable count is a different phrase */
    CHECK (!db.matchPhrases (syllables ("z h r"), results));
    CHECK (results.empty ());

    /* a mismatching initial must not match */
    CHECK (!db.matchPhrases (syllables ("z h r m g h x"), results));

    /* prefix matching is anchored at the start of each syllable */
    CHECK (!db.matchPhrases (syllables ("hong ua en in ong e uo"), results));

    /* re-learning the same phrase bumps its frequency instead of
       inserting a duplicate */
    CHECK (db.learnPhrase ("中华人民共和国",
                           syllables ("zhong hua ren min gong he guo")));
    CHECK (db.matchPhrases (syllables ("z h r m g h g"), results));
    CHECK (results.size () == 1);
    CHECK (freq_at (results, 0) == 2);
}

static void
test_ranking_and_limit (UserPhraseDatabase &db)
{
    g_print ("# ranking by frequency and honouring the result limit\n");

    CHECK (db.learnPhrase ("你好", syllables ("ni hao")));
    CHECK (db.learnPhrase ("内涵", syllables ("nei han")));

    std::vector<UserPhrase> results;
    CHECK (db.matchPhrases (syllables ("n h"), results));
    CHECK (results.size () == 2);
    CHECK (index_of (results, "你好") >= 0);
    CHECK (index_of (results, "内涵") >= 0);

    /* a more frequently used phrase has to be offered first */
    CHECK (db.incrementFrequency ("内涵", "nei'han"));
    CHECK (db.matchPhrases (syllables ("n h"), results));
    CHECK (results.size () == 2);
    CHECK (phrase_at (results, 0) == "内涵");
    CHECK (freq_at (results, 0) == 2);
    CHECK (phrase_at (results, 1) == "你好");

    /* the limit caps the number of candidates handed to the editor */
    CHECK (db.matchPhrases (syllables ("n h"), results, 1));
    CHECK (results.size () == 1);
    CHECK (phrase_at (results, 0) == "内涵");

    /* incrementing an unknown phrase must not create one */
    CHECK (!db.incrementFrequency ("不存在", "bu'cun'zai"));
    CHECK (!db.matchPhrases (syllables ("b c z"), results));
}

static void
test_remove (UserPhraseDatabase &db)
{
    g_print ("# removing a phrase\n");

    std::vector<UserPhrase> results;

    CHECK (db.removePhrase ("你好", "ni'hao"));
    CHECK (db.matchPhrases (syllables ("n h"), results));
    CHECK (results.size () == 1);
    CHECK (phrase_at (results, 0) == "内涵");

    /* removing twice is a no-op, not a crash */
    CHECK (!db.removePhrase ("你好", "ni'hao"));

    /* the row really is gone: re-learning starts the counter over */
    CHECK (db.learnPhrase ("你好", syllables ("ni hao")));
    CHECK (db.matchPhrases (syllables ("ni hao"), results));
    CHECK (results.size () == 1);
    CHECK (freq_at (results, 0) == 1);
}

static void
test_bad_input (UserPhraseDatabase &db)
{
    g_print ("# rejecting malformed input\n");

    std::vector<UserPhrase> results;
    std::vector<std::string> empty;

    CHECK (!db.learnPhrase (NULL, syllables ("ni hao")));
    CHECK (!db.learnPhrase ("空", empty));
    CHECK (!db.matchPhrases (empty, results));
    CHECK (results.empty ());
}

static void
test_singleton (void)
{
    g_print ("# the shared instance opens the database under XDG_CACHE_HOME\n");

    UserPhraseDatabase::init ();
    UserPhraseDatabase &db = UserPhraseDatabase::instance ();

    CHECK (db.learnPhrase ("单例", syllables ("dan li")));

    std::vector<UserPhrase> results;
    CHECK (db.matchPhrases (syllables ("d l"), results));
    CHECK (index_of (results, "单例") >= 0);

    gchar *path = g_build_filename (g_get_user_cache_dir (), "ibus",
                                    "smartpinyin", "user-phrases.db", NULL);
    CHECK (g_file_test (path, G_FILE_TEST_IS_REGULAR));
    g_free (path);
}

static void
test_debug_log_is_opt_in (const gchar *tmpdir)
{
    g_print ("# debug logging stays off unless it is asked for\n");

    gchar *path = g_build_filename (tmpdir, "debug.log", NULL);

    g_unsetenv ("IBUS_SMARTPINYIN_DEBUG_LOG");
    debug_log ("this line must never be written");
    CHECK (!g_file_test (path, G_FILE_TEST_EXISTS));

    g_setenv ("IBUS_SMARTPINYIN_DEBUG_LOG", path, TRUE);
    debug_log ("hello %s", "world");
    CHECK (g_file_test (path, G_FILE_TEST_IS_REGULAR));

    gchar *contents = NULL;
    if (g_file_get_contents (path, &contents, NULL, NULL)) {
        CHECK (strstr (contents, "hello world") != NULL);
        g_free (contents);
    } else {
        CHECK (FALSE /* the log file could not be read back */);
    }

    GStatBuf st;
    CHECK (g_stat (path, &st) == 0);
    CHECK ((st.st_mode & 0077) == 0);

    g_unsetenv ("IBUS_SMARTPINYIN_DEBUG_LOG");
    g_free (path);
}

int
main (int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* redirect the database away from the real user cache before glib
       has any chance to resolve and memoise the cache directory. */
    GError *error = NULL;
    gchar *tmpdir = g_dir_make_tmp ("ibus-smartpinyin-test-XXXXXX", &error);
    if (tmpdir == NULL) {
        g_printerr ("cannot create a temporary directory: %s\n",
                    error ? error->message : "unknown error");
        g_clear_error (&error);
        return 77; /* automake: skip */
    }
    g_setenv ("XDG_CACHE_HOME", tmpdir, TRUE);
    g_unsetenv ("IBUS_SMARTPINYIN_DEBUG_LOG");

    {
        UserPhraseDatabase db;
        CHECK (db.openDatabase ());

        test_learn_and_match_by_initials (db);
        test_ranking_and_limit (db);
        test_remove (db);
        test_bad_input (db);
    }

    test_singleton ();
    test_debug_log_is_opt_in (tmpdir);

    g_print ("1..%d\n", n_checks);
    if (n_failures != 0)
        g_printerr ("%d of %d checks failed\n", n_failures, n_checks);

    /* best effort clean up; the temporary directory is disposable */
    gchar *db_path = g_build_filename (tmpdir, "ibus", "smartpinyin",
                                       "user-phrases.db", NULL);
    g_unlink (db_path);
    g_free (db_path);

    g_free (tmpdir);

    return n_failures == 0 ? 0 : 1;
}
