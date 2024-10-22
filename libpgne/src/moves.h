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

#ifndef MOVES_H
#define MOVES_H

#include "typedef.h"

#include <stdbool.h>

void add_positional_variations_from_file(StateInfo *globals, FILE *fpin);
void add_positional_variation_from_line(StateInfo *globals, char *line);
void add_textual_variations_from_file(const StateInfo *globals, FILE *fpin);
void add_textual_variation_from_line(const StateInfo *globals, char *line);
void add_fen_positional_match(StateInfo *globals, const char *fen_string);
void add_fen_pattern_match(StateInfo *globals, const char *fen_pattern,
                           bool add_reverse, const char *label);
bool check_for_only_checkmate(const StateInfo *globals,
                              const Game *game_details);
bool check_for_only_insufficient_material(const StateInfo *globals,
                                          const Board *board);
bool check_for_only_stalemate(const StateInfo *globals, const Board *board,
                              const Move *moves);
bool check_move_bounds(const StateInfo *globals, unsigned plycount);
bool check_textual_variations(const StateInfo *globals,
                              const Game *game_details);
bool is_stalemate(const StateInfo *globals, const Board *board,
                  const Move *moves);

#endif // MOVES_H
