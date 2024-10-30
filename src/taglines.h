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

#ifndef TAGLINES_H
#define TAGLINES_H

#include "typedef.h"

#include <stdbool.h>

void read_tag_file(StateInfo *globals, GameHeader *game_header,
                   const char *TagFile, bool positive_match);
void read_tag_roster_file(StateInfo *globals, GameHeader *game_header,
                          const char *RosterFile);
bool process_tag_line(StateInfo *globals, GameHeader *game_header,
                      const char *TagFile, char *line, bool positive_match);
bool process_roster_line(const StateInfo *globals, GameHeader *game_header,
                         char *line);

#endif // TAGLINES_H
