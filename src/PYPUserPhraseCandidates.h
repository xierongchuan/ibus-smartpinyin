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

#ifndef __PY_LIB_PINYIN_USER_PHRASE_CANDIDATES_H_
#define __PY_LIB_PINYIN_USER_PHRASE_CANDIDATES_H_

#include "PYPEnhancedCandidates.h"
#include "PYUserPhraseDatabase.h"
#include <vector>
#include <string>

namespace PY {

class PhoneticEditor;

class UserPhraseCandidates : public EnhancedCandidates<PhoneticEditor> {
public:
    UserPhraseCandidates (PhoneticEditor *editor);

public:
    gboolean processCandidates (std::vector<EnhancedCandidate> & candidates);

    int selectCandidate (EnhancedCandidate & enhanced);
    gboolean removeCandidate (EnhancedCandidate & enhanced);

private:
    std::vector<UserPhrase> m_matched_phrases;
};

};

#endif
