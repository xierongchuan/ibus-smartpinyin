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

#include "PYPUserPhraseCandidates.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "PYPPhoneticEditor.h"
#include "PYConfig.h"

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

using namespace PY;

UserPhraseCandidates::UserPhraseCandidates (PhoneticEditor *editor)
{
    m_editor = editor;
}

gboolean
UserPhraseCandidates::processCandidates (std::vector<EnhancedCandidate> & candidates)
{
    m_matched_phrases.clear ();

    std::string input = m_editor->m_text.c_str ();
    if (input.empty ())
        return FALSE;

    /* use cached fragments from updatePinyin(), since
       pinyin_guess_candidates may invalidate key rest positions. */
    const std::vector<std::string> &fragments = m_editor->m_pinyin_fragments;
    debug_log ("processCandidates input='%s' fragments=%zu",
               input.c_str (), fragments.size ());

    if (fragments.size () < 2)
        return FALSE;

    if (!UserPhraseDatabase::instance ().matchPhrases (fragments, m_matched_phrases))
        return FALSE;

    /* Promote or insert matched phrases based on frequency.
       - freq >= 3: target position 0 (top of list)
       - freq < 3:  after the first NBEST_MATCH candidate
       If the phrase already exists in the candidate list (from libpinyin's
       built-in dictionary), MOVE it to the target position instead of
       skipping it as a duplicate. */
    size_t after_first_nbest = 0;
    if (!candidates.empty () &&
        candidates[0].m_candidate_type == CANDIDATE_NBEST_MATCH)
        after_first_nbest = 1;

    int count = 0;
    for (size_t i = 0; i < m_matched_phrases.size (); i++) {
        const UserPhrase &up = m_matched_phrases[i];
        size_t target = (up.freq >= 3) ? 0 : after_first_nbest;
        target += count;

        /* check if phrase already exists in candidate list */
        bool found = false;
        for (size_t j = 0; j < candidates.size (); j++) {
            if (candidates[j].m_display_string == up.phrase) {
                found = true;
                if (j > target) {
                    /* move the existing candidate up to target position */
                    EnhancedCandidate existing = candidates[j];
                    candidates.erase (candidates.begin () + j);
                    candidates.insert (candidates.begin () + target, existing);
                }
                /* always count: even if already at target, reserve
                   this slot so lower-freq phrases insert after it. */
                count++;
                debug_log ("  promote '%s' freq=%d from=%zu to=%zu",
                           up.phrase.c_str (), up.freq, j, target);
                break;
            }
        }
        if (found)
            continue;

        /* phrase not in candidate list — insert new entry */
        EnhancedCandidate enhanced;
        enhanced.m_candidate_type = CANDIDATE_USER_PHRASE;
        enhanced.m_candidate_id = (guint)i;
        enhanced.m_display_string = up.phrase;

        candidates.insert (candidates.begin () + target, enhanced);
        count++;
    }

    return count > 0;
}

int
UserPhraseCandidates::selectCandidate (EnhancedCandidate & enhanced)
{
    assert (CANDIDATE_USER_PHRASE == enhanced.m_candidate_type);

    if (enhanced.m_candidate_id < m_matched_phrases.size ()) {
        const UserPhrase &up = m_matched_phrases[enhanced.m_candidate_id];
        UserPhraseDatabase::instance ().incrementFrequency (
            up.phrase.c_str (), up.full_pinyin.c_str ());
    }

    return SELECT_CANDIDATE_DIRECT_COMMIT;
}

gboolean
UserPhraseCandidates::removeCandidate (EnhancedCandidate & enhanced)
{
    assert (CANDIDATE_USER_PHRASE == enhanced.m_candidate_type);

    if (enhanced.m_candidate_id < m_matched_phrases.size ()) {
        const UserPhrase &up = m_matched_phrases[enhanced.m_candidate_id];
        return UserPhraseDatabase::instance ().removePhrase (
            up.phrase.c_str (), up.full_pinyin.c_str ());
    }

    return FALSE;
}
