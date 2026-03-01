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

#ifndef __PY_USER_PHRASE_DATABASE_H_
#define __PY_USER_PHRASE_DATABASE_H_

#include <memory>
#include <string>
#include <vector>
#include <glib.h>
#include <sqlite3.h>

namespace PY {

struct UserPhrase {
    std::string phrase;
    std::string full_pinyin;
    int freq;
};

class UserPhraseDatabase {
public:
    static void init ();
    static UserPhraseDatabase & instance (void) { return *m_instance; }

public:
    UserPhraseDatabase ();
    ~UserPhraseDatabase ();

    gboolean openDatabase ();

    gboolean learnPhrase (const gchar *phrase,
                          const std::vector<std::string> &syllables);

    gboolean matchPhrases (const std::vector<std::string> &fragments,
                           std::vector<UserPhrase> &results,
                           int limit = 10);

    gboolean removePhrase (const gchar *phrase, const gchar *full_pinyin);

    gboolean incrementFrequency (const gchar *phrase, const gchar *full_pinyin);

private:
    gboolean createDatabase ();
    gboolean executeSQL (const char *sql);

private:
    sqlite3 *m_sqlite;

    static std::unique_ptr<UserPhraseDatabase> m_instance;
};

};

#endif
