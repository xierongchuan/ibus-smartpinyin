/* vim:set et ts=4 sts=4:
 *
 * ibus-smartpinyin - Smart Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2026 黑鱼 <xierongchuan@tutamail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __PY_DEBUG_LOG_H_
#define __PY_DEBUG_LOG_H_

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

namespace PY {

/**
 * Tracing for the user phrase learning code.
 *
 * Switched off by default: the traces contain everything the user types,
 * so they are only written when IBUS_SMARTPINYIN_DEBUG_LOG is set to the
 * path of a log file, e.g.
 *
 *     IBUS_SMARTPINYIN_DEBUG_LOG=$HOME/smartpinyin.log ibus-daemon -drx
 *
 * The file is created with 0600 permissions so that a log left in a
 * shared directory is not readable by other users.
 */
static inline void
debug_log (const char *fmt, ...)
{
    const char *path = getenv ("IBUS_SMARTPINYIN_DEBUG_LOG");
    if (path == NULL || path[0] == '\0')
        return;

    int fd = open (path, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW, 0600);
    if (fd < 0)
        return;

    FILE *f = fdopen (fd, "a");
    if (!f) {
        close (fd);
        return;
    }

    time_t now = time (NULL);
    struct tm tm_buf;
    struct tm *t = localtime_r (&now, &tm_buf);
    if (t)
        fprintf (f, "[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);

    va_list ap;
    va_start (ap, fmt);
    vfprintf (f, fmt, ap);
    va_end (ap);

    fprintf (f, "\n");
    fclose (f);
}

};

#endif
