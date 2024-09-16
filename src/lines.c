/*
 *  This file is part of pgn-extract: a Portable Game Notation (PGN) extractor.
 *  Copyright (C) 1994-2024 David J. Barnes
 *
 *  pgn-extract is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  pgn-extract is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with pgn-extract. If not, see <http://www.gnu.org/licenses/>.
 *
 *  David J. Barnes may be contacted as d.j.barnes@kent.ac.uk
 *  https://www.cs.kent.ac.uk/people/staff/djb/
 */

#include "lines.h"

#include "mymalloc.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Define a character that may be used to comment a line in, e.g.
 * the variations files.
 * Use the PGN escape mechanism character, for consistency.
 */
#define COMMENT_CHAR '%'

/* Return true if line contains a non-space character, but
 * is not a comment line.
 */
bool non_blank_line(const char *line) {
  bool blank = true;

  if (line != NULL) {
    if (comment_line(line)) {
      /* Comment lines count as blanks. */
    } else {
      while (blank && (*line != '\0')) {
        if (!isspace((int)*line)) {
          blank = false;
        } else {
          line++;
        }
      }
    }
  }
  return !blank;
}

bool blank_line(const char *line) { return !non_blank_line(line); }

/* Should the given line be regarded as a comment line? */
bool comment_line(const char *line) { return *line == COMMENT_CHAR; }
