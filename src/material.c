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

#include "material.h"

#include "apply.h"
#include "defs.h"
#include "grammar.h"
#include "lex.h"
#include "lines.h"
#include "mymalloc.h"
#include "taglist.h"
#include "typedef.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Code to handle specifications describing the state of the board
 * in terms of numbers of pieces and material balance between opponents.
 *
 * Games are then matched against these specifications.
 */

/* Keep a list of endings to be found. */
static MaterialCriteria *endings_to_match = NULL;

/* What kind of piece is the character, c, likely to represent?
 * NB: This is NOT the same as is_piece() in decode.c
 */
/* Define pseudo-letter for minor pieces, used later. */
#define MINOR_PIECE 'L'

static Piece is_English_piece(char c) {
  Piece piece = EMPTY;

  switch (c) {
  case 'K':
  case 'k':
    piece = KING;
    break;
  case 'Q':
  case 'q':
    piece = QUEEN;
    break;
  case 'R':
  case 'r':
    piece = ROOK;
    break;
  case 'N':
  case 'n':
    piece = KNIGHT;
    break;
  case 'B':
  case 'b':
    piece = BISHOP;
    break;
  case 'P':
  case 'p':
    piece = PAWN;
    break;
  }
  return piece;
}

/* Initialise the count of required pieces prior to reading
 * in the data.
 */
static MaterialCriteria *new_ending_details(bool both_colours) {
  MaterialCriteria *details =
      (MaterialCriteria *)malloc_or_die(sizeof(MaterialCriteria));
  int c;
  Piece piece;

  details->both_colours = both_colours;
  for (piece = PAWN; piece <= KING; piece++) {
    for (c = 0; c < 2; c++) {
      details->num_pieces[c][piece] = 0;
      details->occurs[c][piece] = EXACTLY;
    }
  }
  /* Fill out some miscellaneous colour based information. */
  for (c = 0; c < 2; c++) {
    /* Only the KING is a requirement for each side. */
    details->num_pieces[c][KING] = 1;
    details->match_depth[c] = 0;
    /* How many general minor pieces to match. */
    details->num_minor_pieces[c] = 0;
    details->minor_occurs[c] = EXACTLY;
  }
  /* Assume that the match must always have a depth of at least two for
   * two half-move stability.
   */
  details->move_depth = 2;
  details->next = NULL;
  return details;
}

static const char *extract_combination(const StateInfo *globals, const char *p,
                                       Occurs *p_occurs, int *p_number,
                                       const char *line) {
  bool Ok = true;
  Occurs occurs = EXACTLY;
  int number = 1;

  if (isdigit((int)*p)) {
    /* Only single digits are allowed. */
    number = *p - '0';
    p++;
    if (isdigit((int)*p)) {
      fprintf(globals->logfile, "Number > 9 is too big in %s.\n", line);
      while (isdigit((int)*p)) {
        p++;
      }
      Ok = false;
    }
  }
  if (Ok) {
    /* Look for trailing annotations. */
    switch (*p) {
    case '*':
      number = 0;
      occurs = NUM_OR_MORE;
      p++;
      break;
    case '+':
      occurs = NUM_OR_MORE;
      p++;
      break;
    case '-':
      occurs = NUM_OR_LESS;
      p++;
      break;
    case '?':
      number = 1;
      occurs = NUM_OR_LESS;
      p++;
      break;
    case '=':
    case '#':
    case '<':
    case '>':
      switch (*p) {
      case '=':
        p++;
        occurs = SAME_AS_OPPONENT;
        break;
      case '#':
        p++;
        occurs = NOT_SAME_AS_OPPONENT;
        break;
      case '<':
        p++;
        if (*p == '=') {
          occurs = LESS_EQ_THAN_OPPONENT;
          p++;
        } else {
          occurs = LESS_THAN_OPPONENT;
        }
        break;
      case '>':
        p++;
        if (*p == '=') {
          occurs = MORE_EQ_THAN_OPPONENT;
          p++;
        } else {
          occurs = MORE_THAN_OPPONENT;
        }
        break;
      }
      break;
    }
  }

  if (Ok) {
    *p_occurs = occurs;
    *p_number = number;
    return p;
  } else {
    return NULL;
  }
}

/* Extract a single piece set of information from line.
 * Return where we have got to as the result.
 * colour == WHITE means we are looking at the first set of
 * pieces, so some of the notation is illegal (i.e. the relative ops).
 *
 * The basic syntax for a piece description is:
 *        piece [number] [occurs]
 * For instance:
 *        P2+ Pawn occurs at least twice or more.
 *        R= Rook occurs same number of times as opponent. (colour == BLACK)
 *        P1>= Exactly one pawn more than the opponent. (colour == BLACK)
 */
static const char *extract_piece_information(const StateInfo *globals,
                                             const char *line,
                                             MaterialCriteria *details,
                                             Colour colour) {
  const char *p = line;
  bool Ok = true;

  while (Ok && (*p != '\0') && !isspace((int)*p) && *p != MATERIAL_CONSTRAINT) {
    Piece piece = is_English_piece(*p);
    /* By default a piece should occur exactly once. */
    Occurs occurs = EXACTLY;
    int number = 1;

    if (piece != EMPTY) {
      /* Skip over the piece. */
      p++;
      p = extract_combination(globals, p, &occurs, &number, line);
      if (p != NULL) {
        if ((piece == KING) && (number != 1)) {
          fprintf(globals->logfile, "A king must occur exactly once.\n");
          number = 1;
        } else if ((piece == PAWN) && (number > 8)) {
          fprintf(globals->logfile, "No more than 8 pawns are allowed.\n");
          number = 8;
        }
        details->num_pieces[colour][piece] = number;
        details->occurs[colour][piece] = occurs;
      } else {
        Ok = false;
      }
    } else if (isalpha((int)*p) && (toupper((int)*p) == MINOR_PIECE)) {
      p++;
      p = extract_combination(globals, p, &occurs, &number, line);
      if (p != NULL) {
        details->num_minor_pieces[colour] = number;
        details->minor_occurs[colour] = occurs;
      } else {
        Ok = false;
      }
    } else {
      fprintf(globals->logfile, "Unknown symbol at %s\n", p);
      Ok = false;
    }
  }
  if (Ok) {
    /* Make a sanity check on the use of minor pieces. */
    if ((details->num_minor_pieces[colour] > 0) ||
        (details->minor_occurs[colour] != EXACTLY)) {
      /* Warn about use of BISHOP and KNIGHT letters. */
      if ((details->num_pieces[colour][BISHOP] > 0) ||
          (details->occurs[colour][BISHOP] != EXACTLY) ||
          (details->num_pieces[colour][KNIGHT] > 0) ||
          (details->occurs[colour][KNIGHT] != EXACTLY)) {
        fprintf(globals->logfile,
                "Warning: the mixture of minor pieces in %s is not guaranteed "
                "to work.\n",
                line);
        fprintf(globals->logfile, "In a single set it is advisable to stick "
                                  "to either L or B and/or N.\n");
      }
    }
    return p;
  } else {
    return NULL;
  }
}

/* Extract the piece specification from line and fill out
 * details with the pattern information.
 */
static bool decompose_line(const StateInfo *globals, const char *line,
                           MaterialCriteria *details) {
  const char *p = line;
  bool Ok = true;

  /* Skip initial space. */
  while (isspace((int)*p)) {
    p++;
  }

  /* Look for a move depth. */
  if (isdigit((int)*p)) {
    unsigned depth;

    depth = *p - '0';
    p++;
    while (isdigit((int)*p)) {
      depth = (depth * 10) + (*p - '0');
      p++;
    }
    while (isspace((int)*p)) {
      p++;
    }
    details->move_depth = depth;
  }

  /* Extract two pairs of piece information.
   * NB: If the first set of pieces consists of a lone king then that must
   * be included explicitly. If the second set consists of a lone
   * king then that can be omitted.
   */
  p = extract_piece_information(globals, p, details, WHITE);
  if (p != NULL) {
    while ((*p != '\0') && (isspace((int)*p) || (*p == MATERIAL_CONSTRAINT))) {
      p++;
    }
    if (*p != '\0') {
      p = extract_piece_information(globals, p, details, BLACK);
    } else {
      /* No explicit requirements for the other colour. */
      Piece piece;

      for (piece = PAWN; piece <= KING; piece++) {
        details->num_pieces[BLACK][piece] = 0;
        details->occurs[BLACK][piece] = EXACTLY;
      }
      details->num_pieces[BLACK][KING] = 1;
      details->occurs[BLACK][KING] = EXACTLY;
    }
  }
  if (p != NULL) {
    /* Allow trailing text as a comment. */
  } else {
    Ok = false;
  }
  return Ok;
}

/* A new game to be looked for. Indicate that we have not
 * started matching any yet.
 */
static void reset_match_depths(MaterialCriteria *endings) {
  for (; endings != NULL; endings = endings->next) {
    endings->match_depth[WHITE] = 0;
    endings->match_depth[BLACK] = 0;
  }
}

/* Try to find a match for the given number of piece details. */
static bool piece_match(const StateInfo *globals, int num_available,
                        int num_to_find, int num_opponents, Occurs occurs) {
  bool match = false;

  switch (occurs) {
  case EXACTLY:
    match = num_available == num_to_find;
    break;
  case NUM_OR_MORE:
    match = num_available >= num_to_find;
    break;
  case NUM_OR_LESS:
    match = num_available <= num_to_find;
    break;
  case SAME_AS_OPPONENT:
    match = num_available == num_opponents;
    break;
  case NOT_SAME_AS_OPPONENT:
    match = num_available != num_opponents;
    break;
  case LESS_THAN_OPPONENT:
    match = (num_available + num_to_find) <= num_opponents;
    break;
  case MORE_THAN_OPPONENT:
    match = (num_available - num_to_find) >= num_opponents;
    break;
  case LESS_EQ_THAN_OPPONENT:
    /* This means exactly num_to_find less than the
     * opponent.
     */
    match = (num_available + num_to_find) == num_opponents;
    break;
  case MORE_EQ_THAN_OPPONENT:
    /* This means exactly num_to_find greater than the
     * opponent.
     */
    match = (num_available - num_to_find) == num_opponents;
    break;
  default:
    fprintf(globals->logfile, "Inconsistent state %d in piece_match.\n",
            occurs);
    match = false;
  }
  return match;
}

/* Try to find a match against one player's pieces in the piece_set_colour
 * set of details_to_find.
 */
static bool piece_set_match(const StateInfo *globals,
                            const MaterialCriteria *details_to_find,
                            int num_pieces[2][NUM_PIECE_VALUES],
                            Colour game_colour, Colour piece_set_colour) {
  bool match = true;
  Piece piece;
  /* Determine whether we failed on a match for minor pieces or not. */
  bool minor_failure = false;

  /* No need to check KING. */
  for (piece = PAWN; (piece < KING) && match; piece++) {
    int num_available = num_pieces[game_colour][piece];
    int num_opponents = num_pieces[OPPOSITE_COLOUR(game_colour)][piece];
    int num_to_find = details_to_find->num_pieces[piece_set_colour][piece];
    Occurs occurs = details_to_find->occurs[piece_set_colour][piece];

    match =
        piece_match(globals, num_available, num_to_find, num_opponents, occurs);
    if (!match) {
      if ((piece == KNIGHT) || (piece == BISHOP)) {
        minor_failure = true;
        /* Carry on trying to match. */
        match = true;
      } else {
        minor_failure = false;
      }
    }
  }

  if (match) {
    /* Ensure that the minor pieces match if there is a minor pieces
     * requirement.
     */
    int num_to_find = details_to_find->num_minor_pieces[piece_set_colour];
    Occurs occurs = details_to_find->minor_occurs[piece_set_colour];

    if ((num_to_find > 0) || (occurs != EXACTLY)) {
      int num_available =
          num_pieces[game_colour][BISHOP] + num_pieces[game_colour][KNIGHT];
      int num_opponents = num_pieces[OPPOSITE_COLOUR(game_colour)][BISHOP] +
                          num_pieces[OPPOSITE_COLOUR(game_colour)][KNIGHT];

      match = piece_match(globals, num_available, num_to_find, num_opponents,
                          occurs);
    } else if (minor_failure) {
      /* We actually failed with proper matching of individual minor
       * pieces, and no minor match fixup is possible.
       */
      match = false;
    } else {
      /* Match stands. */
    }
  }
  return match;
}

/* Look for a material match between current_details and
 * details_to_find. Only return true if we have both a match
 * and match_depth >= move_depth in details_to_find.
 * NB: If the game ends before the required depth is reached then a
 * potential match would be missed. This could be considered
 * as a bug.
 */
static bool material_match(const StateInfo *globals,
                           MaterialCriteria *details_to_find,
                           int num_pieces[2][NUM_PIECE_VALUES],
                           Colour game_colour) {
  bool match = true;
  Colour piece_set_colour = WHITE;

  match = piece_set_match(globals, details_to_find, num_pieces, game_colour,
                          piece_set_colour);
  if (match) {
    game_colour = OPPOSITE_COLOUR(game_colour);
    piece_set_colour = OPPOSITE_COLOUR(piece_set_colour);
    match = piece_set_match(globals, details_to_find, num_pieces, game_colour,
                            piece_set_colour);
    /* Reset colour to its original value. */
    game_colour = OPPOSITE_COLOUR(game_colour);
  }

  if (match) {
    if (details_to_find->match_depth[game_colour] <
        details_to_find->move_depth) {
      /* Not a full match yet. */
      match = false;
      details_to_find->match_depth[game_colour]++;
    } else {
      /* A stable match. */
    }
  } else {
    /* Reset the match counter. */
    details_to_find->match_depth[game_colour] = 0;
  }
  return match;
}

/* Extract the numbers of each type of piece from the given board. */
static void extract_pieces_from_board(int num_pieces[2][NUM_PIECE_VALUES],
                                      const Board *board) {
  /* Set up num_pieces from the board. */
  for (int c = 0; c < 2; c++) {
    for (int p = 0; p < NUM_PIECE_VALUES; p++) {
      num_pieces[c][p] = 0;
    }
  }
  for (char rank = FIRSTRANK; rank <= LASTRANK; rank++) {
    for (char col = FIRSTCOL; col <= LASTCOL; col++) {
      int r = RankConvert(rank);
      int c = ColConvert(col);

      Piece coloured_piece = board->board[r][c];
      if (coloured_piece != EMPTY) {
        int p = EXTRACT_PIECE(coloured_piece);
        num_pieces[EXTRACT_COLOUR(coloured_piece)][p]++;
      }
    }
  }
}

/* Check to see whether the given moves lead to a position
 * that matches the given 'ending' position.
 * In other words, a position with the required balance
 * of pieces.
 */
static bool look_for_material_match(const StateInfo *globals,
                                    GameHeader *game_header,
                                    Game *game_details) {
  bool game_ok = true;
  bool match_comment_added = false;
  Move *next_move = game_details->moves;
  Move *move_for_comment = NULL;
  Colour colour = WHITE;
  /* The initial game position has the full set of piece details. */
  int num_pieces[2][NUM_PIECE_VALUES] = {
      /* Dummies for OFF and EMPTY at the start. */
      /*     P  N  B  R  Q  K */
      {0, 0, 8, 2, 2, 2, 1, 1},
      {0, 0, 8, 2, 2, 2, 1, 1}};
  Board *board =
      new_game_board(globals, game_header, game_details->tags[FEN_TAG]);

  if (game_details->tags[FEN_TAG] != NULL) {
    extract_pieces_from_board(num_pieces, board);
    colour = board->to_move;
  }
  /* Ensure that all previous match indications are cleared. */
  reset_match_depths(endings_to_match);

  /* Keep going while the game is ok, and we have some more
   * moves and we haven't exceeded the search depth without finding
   * a match.
   */
  bool matches = false;
  bool end_of_game = false;
  bool white_matches = false, black_matches = false;
  while (game_ok && !matches && !end_of_game) {
    for (MaterialCriteria *details_to_find = endings_to_match;
         !matches && (details_to_find != NULL);
         details_to_find = details_to_find->next) {
      /* Try before applying each move.
       * Note, that we wish to try both ways around because we might
       * have WT,BT WF,BT ... If we don't try BLACK on WHITE success
       * then we might miss a match because a full match takes several
       * separate individual match steps.
       */
      white_matches =
          material_match(globals, details_to_find, num_pieces, WHITE);
      if (details_to_find->both_colours) {
        black_matches =
            material_match(globals, details_to_find, num_pieces, BLACK);
      } else {
        black_matches = false;
      }
      if (white_matches || black_matches) {
        matches = true;
        /* See whether a matching comment is required. */
        if (globals->add_position_match_comments && !match_comment_added) {
          CommentList *match_comment = create_match_comment(globals, board);
          if (move_for_comment != NULL) {
            append_comments_to_move(game_header, move_for_comment,
                                    match_comment);
          } else {
            if (game_details->prefix_comment == NULL) {
              game_details->prefix_comment = match_comment;
            } else {
              CommentList *comm = game_details->prefix_comment;
              while (comm->next != NULL) {
                comm = comm->next;
              }
              comm->next = match_comment;
            }
          }
        }
      }
    }
    if (matches) {
      /* Nothing required. */
    } else if (next_move == NULL) {
      end_of_game = true;
    } else if (*(next_move->move) != '\0') {
      /* Try the next position. */
      if (apply_move(globals, game_header, next_move, board)) {
        /* Remove any captured pieces. */
        if (next_move->captured_piece != EMPTY) {
          num_pieces[OPPOSITE_COLOUR(colour)][next_move->captured_piece]--;
        }
        if (next_move->promoted_piece != EMPTY) {
          num_pieces[colour][next_move->promoted_piece]++;
          /* Remove the promoting pawn. */
          num_pieces[colour][PAWN]--;
        }

        move_for_comment = next_move;
        colour = OPPOSITE_COLOUR(colour);
        next_move = next_move->next;
      } else {
        game_ok = false;
      }
    } else {
      /* An empty move. */
      fprintf(globals->logfile,
              "Internal error: Empty move in look_for_material_match.\n");
      game_ok = false;
    }
  }
  (void)free((void *)board);
  if (game_ok && matches) {
    if (globals->add_match_tag) {
      game_details->tags[MATERIAL_MATCH_TAG] =
          copy_string(white_matches ? "White" : "Black");
    }
    return true;
  } else {
    return false;
  }
}

/* Check to see whether the given moves lead to a position
 * that matches one of the required 'material match' positions.
 * In other words, a position with the required balance
 * of pieces.
 */
bool check_for_material_match(const StateInfo *globals, GameHeader *game_header,
                              Game *game) {
  /* Match if there are no endings to match. */
  if (endings_to_match != NULL) {
    return look_for_material_match(globals, game_header, game);
  } else {
    return true;
  }
}

/* Does the board's material match the constraints of details_to_find?
 * Return true if it does, false otherwise.
 */
bool constraint_material_match(const StateInfo *globals,
                               MaterialCriteria *details_to_find,
                               const Board *board) {
  /* Only a single match position is required. */
  details_to_find->move_depth = 0;
  details_to_find->match_depth[0] = 0;
  details_to_find->match_depth[1] = 0;

  int num_pieces[2][NUM_PIECE_VALUES];
  extract_pieces_from_board(num_pieces, board);
  bool white_matches =
      material_match(globals, details_to_find, num_pieces, WHITE);
  bool black_matches;

  if (details_to_find->both_colours) {
    black_matches = material_match(globals, details_to_find, num_pieces, BLACK);
  } else {
    black_matches = false;
  }
  return white_matches || black_matches;
}

/* Decompose the text of line to extract two sets of
 * piece configurations.
 * If both_colours is true then matches will be tried
 * for both colours in each configuration.
 * Otherwise, the first set of pieces are assumed to
 * be white and the second to be black.
 * If pattern_constraint is true then the description
 * is a constraint of a FEN pattern and should not be
 * retained as a separate material match.
 */
MaterialCriteria *process_material_description(const StateInfo *globals,
                                               const char *line,
                                               bool both_colours,
                                               bool pattern_constraint) {
  MaterialCriteria *details = NULL;

  if (non_blank_line(line)) {
    details = new_ending_details(both_colours);

    if (decompose_line(globals, line, details)) {
      if (!pattern_constraint) {
        /* Add it on to the list. */
        details->next = endings_to_match;
        endings_to_match = details;
      }
    } else {
      (void)free((void *)details);
      details = NULL;
    }
  }
  return details;
}

/* Read a file containing material matches. */
bool build_endings(const StateInfo *globals, GameHeader *game_header,
                   const char *infile, bool both_colours) {
  FILE *fp = fopen(infile, "r");
  bool Ok = true;

  if (fp == NULL) {
    fprintf(globals->logfile, "Cannot open %s for reading.\n", infile);
    exit(1);
  } else {
    char *line;
    while ((line = read_line(globals, game_header, fp)) != NULL) {
      if (process_material_description(globals, line, both_colours, false) ==
          NULL) {
        Ok = false;
      }
      (void)free(line);
    }
    (void)fclose(fp);
  }
  return Ok;
}

/* Return true if there is insufficient material on the board to force a win. */
bool insufficient_material(const Board *board) {
  int num_pieces[2][NUM_PIECE_VALUES] = {
      /* Dummies for OFF and EMPTY at the start. */
      /*     P  N  B  R  Q  K */
      {0, 0, 8, 2, 2, 2, 1, 1},
      {0, 0, 8, 2, 2, 2, 1, 1}};

  extract_pieces_from_board(num_pieces, board);

  if (num_pieces[0][PAWN] != 0 || num_pieces[1][PAWN] != 0) {
    return false;
  }
  if (num_pieces[0][ROOK] != 0 || num_pieces[1][ROOK] != 0) {
    return false;
  }
  if (num_pieces[0][QUEEN] != 0 || num_pieces[1][QUEEN] != 0) {
    return false;
  }
  if (num_pieces[0][BISHOP] > 1 || num_pieces[1][BISHOP] > 1) {
    return false;
  }
  if ((num_pieces[0][BISHOP] != 0 && num_pieces[0][KNIGHT] != 0) ||
      (num_pieces[1][BISHOP] != 0 && num_pieces[1][KNIGHT] != 0)) {
    return false;
  }
  if ((num_pieces[0][KNIGHT] >= 2 &&
       (num_pieces[1][BISHOP] != 0 || num_pieces[1][KNIGHT] != 0)) ||
      (num_pieces[1][KNIGHT] >= 2 &&
       (num_pieces[0][BISHOP] != 0 || num_pieces[0][KNIGHT] != 0))) {
    return false;
  }
  return true;
}
