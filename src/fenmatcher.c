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

#include "fenmatcher.h"

#include "apply.h"
#include "defs.h"
#include "material.h"
#include "mymalloc.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Character on an encoded board representing an empty square. */
#define EMPTY_SQUARE '_'
/* Pattern meta characters. */
#define NON_EMPTY_SQUARE '!'
#define ANY_SQUARE_STATE '?'
#define ZERO_OR_MORE_OF_ANYTHING '*'
#define ANY_WHITE_PIECE 'A'
#define ANY_BLACK_PIECE 'a'
#define NOT_A_PAWN 'm'

/* Symbols for closures. */
#define CCL_START '['
#define CCL_END ']'
#define NCCL '^'

/**
 * Based on original pattern matching code by Rob Pike.
 * Taken from:
 *     http://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html
 * and ideas from Kernighan and Plauger's "Software Tools".
 */

/* A single rank of a FEN-based patterns to match.
 * Ranks are chained as a linear list via next_rank and
 * alternatives for the same rank via alternative_rank.
 * The optional_label (if any) is stored with the final rank of
 * the list.
 */
typedef struct FENPatternMatch {
  char *rank;
  const char *optional_label;
  struct FENPatternMatch *alternative_rank;
  struct FENPatternMatch *next_rank;
  MaterialCriteria *constraint;
} FENPatternMatch;

static FENPatternMatch *pattern_tree = NULL;

static bool matchhere(const char *regexp, const char *text);
static bool matchstar(const char *regexp, const char *text);
static bool matchccl(const char *regexp, const char *text);
static bool matchnccl(const char *regexp, const char *text);
static bool matchone(char regchar, char textchar);
static void convert_rank_to_text(const Board *board, Rank rank, char *text);
static const char *reverse_fen_pattern(const char *pattern);
static void pattern_tree_insert(const StateInfo *globals, char **ranks,
                                const char *label,
                                MaterialCriteria *constraint);
static void insert_pattern(const StateInfo *globals, FENPatternMatch *node,
                           FENPatternMatch *next);
static const char *pattern_match_rank(const StateInfo *globals,
                                      const Board *board,
                                      FENPatternMatch *pattern,
                                      int patternIndex,
                                      char ranks[BOARDSIZE + 1][BOARDSIZE + 1]);

/*
 * Add a FENPattern to be matched. If add_reverse is true then
 * additionally add a second pattern that has the colours reversed.
 * If label is non-NULL then associate it with fen_pattern for possible
 * output in a tag when the pattern is matched.
 */
void add_fen_pattern(const StateInfo *globals, const char *fen_pattern,
                     bool add_reverse, const char *label) {
  /* Check the pattern has reasonable syntax. */
  /* Count the number of rank dividers. */
  int dividers = 0;
  /* Count the number of symbols in each rank - must be
   * at least one.
   */
  int rankSymbols = 0;
  bool ok = true;
  const char *p = fen_pattern;
  const char *rank_start = fen_pattern;
  bool in_closure = false;
  char **ranks = (char **)malloc_or_die(BOARDSIZE * sizeof(*ranks));
  while (*p != '\0' && *p != ' ' && *p != MATERIAL_CONSTRAINT && ok) {
    if (*p == '/') {
      /* End of this rank. */
      if (rankSymbols == 0) {
        /* Nothing on the previous rank. */
        ok = false;
      } else {
        int num_chars = p - rank_start;
        ranks[dividers] = (char *)malloc_or_die(num_chars + 1);
        strncpy(ranks[dividers], rank_start, num_chars);
        ranks[dividers][num_chars] = '\0';
        dividers++;

        rank_start = p + 1;
      }
      rankSymbols = 0;
    } else if (*p == CCL_START) {
      if (!in_closure) {
        in_closure = true;
      } else {
        ok = false;
        fprintf(globals->logfile, "Nested closures not allowed: %s\n",
                fen_pattern);
      }
    } else if (*p == CCL_END) {
      if (in_closure) {
        in_closure = false;
      } else {
        ok = false;
        fprintf(globals->logfile, "Missing %c to match %c: %s\n", CCL_START,
                CCL_END, fen_pattern);
      }
    } else if (*p == NCCL) {
      if (!in_closure) {
        ok = false;
        fprintf(globals->logfile, "%c not allowed outside %c...%c: %s\n", NCCL,
                CCL_START, CCL_END, fen_pattern);
      }
    } else {
      rankSymbols++;
    }
    p++;
  }
  if (dividers != BOARDSIZE - 1) {
    ok = false;
  } else if (rankSymbols == 0) {
    ok = false;
  } else if (ok) {
    /* Store the final regexp of the pattern. */
    int num_chars = p - rank_start;
    ranks[dividers] = (char *)malloc_or_die(num_chars + 1);
    strncpy(ranks[dividers], rank_start, num_chars);
    ranks[dividers][num_chars] = '\0';
  }
  if (ok) {
    MaterialCriteria *constraint;
    if (*p == MATERIAL_CONSTRAINT) {
      p++;
      /* Deal with a constraint on the material that must also match. */
      constraint = process_material_description(globals, p, add_reverse, true);
    } else {
      constraint = NULL;
    }
    pattern_tree_insert(globals, ranks,
                        label != NULL ? copy_string(label) : copy_string(""),
                        constraint);

    /* Do the same again if a reversed version is required. */
    if (add_reverse) {
      char *pattern = copy_string(fen_pattern);
      /* Terminate at the end of the board position
         as we are not interested in the castling rights
         or who is to move.
       */
      pattern[p - fen_pattern] = '\0';
      const char *reversed = reverse_fen_pattern(pattern);
      if (label != NULL) {
        /* Add a suffix to make it clear that this is
         * a match of the inverted form.
         */
        char *rlabel = (char *)malloc_or_die(strlen(label) + 1 + 1);
        strcpy(rlabel, label);
        strcat(rlabel, "I");
        add_fen_pattern(globals, reversed, false, rlabel);
      } else {
        add_fen_pattern(globals, reversed, false, "");
      }
    }
  } else {
    fprintf(globals->logfile, "FEN Pattern: %s badly formed.\n", fen_pattern);
  }
}

/* Invert the colour sense of the given FENPattern.
 * Return the inverted form.
 */
static const char *reverse_fen_pattern(const char *pattern) {
  /* Completely switch the rows and invert the case of each piece letter. */
  char **rows = (char **)malloc_or_die(8 * sizeof(*rows));
  char *start = copy_string(pattern);
  char *end = start;
  /* Isolate each row in its new order. */
  int row;
  for (row = BOARDSIZE - 1; row >= 0 && *start != '\0'; row--) {
    /* Find the end of the next row. */
    while (*end != '/' && *end != '\0') {
      end++;
    }
    rows[row] = (char *)malloc_or_die((end - start + 1) * sizeof(**rows));
    strncpy(rows[row], start, end - start);
    rows[row][end - start] = '\0';
    start = end;
    if (*start != '\0') {
      start++;
    }
    end++;
  }
  char *reversed = (char *)malloc_or_die(strlen(pattern) + 1);
  /* Copy across the rows, flipping the colours. */
  char *nextchar = reversed;
  for (row = 0; row < 8; row++) {
    const char *text = rows[row];
    while (*text != '\0') {
      if (isalpha((int)*text)) {
        if (islower((int)*text)) {
          *nextchar = toupper(*text);
        } else {
          *nextchar = tolower(*text);
        }
      } else {
        *nextchar = *text;
      }
      text++;
      nextchar++;
    }
    if (row != BOARDSIZE - 1) {
      *nextchar = '/';
      nextchar++;
    }
  }
  *nextchar = '\0';
  return reversed;
}

/*
 * Insert the ranks of a single pattern into the current pattern tree
 * to consolidate similar patterns.
 */
static void pattern_tree_insert(const StateInfo *globals, char **ranks,
                                const char *label,
                                MaterialCriteria *constraint) {
  FENPatternMatch *match = (FENPatternMatch *)malloc_or_die(sizeof(*match));
  /* Create a linked list for the ranks.
   * Place the label in the final link.
   */
  FENPatternMatch *next = match;
  for (int i = 0; i < BOARDSIZE; i++) {
    next->rank = ranks[i];
    next->alternative_rank = NULL;
    if (i != BOARDSIZE - 1) {
      next->next_rank = (FENPatternMatch *)malloc_or_die(sizeof(*match));
      next->optional_label = NULL;
      next->constraint = NULL;
      next = next->next_rank;
    } else {
      next->next_rank = NULL;
      next->optional_label = label;
      next->constraint = constraint;
    }
  }
  if (pattern_tree == NULL) {
    pattern_tree = match;
  } else {
    /* Find the place to insert this list in the existing tree. */
    insert_pattern(globals, pattern_tree, match);
  }
}

/* Starting at node, try to insert next into the tree.
 * Return true on success, false on failure.
 */
static void insert_pattern(const StateInfo *globals, FENPatternMatch *node,
                           FENPatternMatch *next) {
  bool inserted = false;
  while (!inserted && strcmp(node->rank, next->rank) == 0) {
    if (node->next_rank != NULL) {
      /* Same pattern. Move to the next rank of both. */
      node = node->next_rank;
      next = next->next_rank;
    } else {
      /* Patterns are duplicates. */
      fprintf(globals->logfile, "Warning: duplicate FEN patterns detected.\n");
      inserted = true;
    }
  }
  if (!inserted) {
    /* Insert as an alternative. */
    if (node->alternative_rank != NULL) {
      insert_pattern(globals, node->alternative_rank, next);
    } else {
      node->alternative_rank = next;
    }
  }
}

/*
 * Try to match the board against one of the FEN patterns.
 * Return NULL if no match, otherwise a possible label for the
 * match to be added to the game's tags. An empty string is
 * used for no label.
 */
const char *pattern_match_board(const StateInfo *globals, const Board *board) {
  const char *match_label = NULL;
  if (pattern_tree != NULL) {
    /* Don't convert any ranks of the board until they
     * are required.
     */
    char ranks[BOARDSIZE + 1][BOARDSIZE + 1];
    for (int i = 0; i < BOARDSIZE; i++) {
      ranks[i][0] = '\0';
    }
    match_label = pattern_match_rank(globals, board, pattern_tree, 0, ranks);
  }
  return match_label;
}

/* Match ranks[patternIndex ...] against board.
 * return the corresponding match label if a match is found.
 * Return NULL if no match is found.
 */
static const char *
pattern_match_rank(const StateInfo *globals, const Board *board,
                   FENPatternMatch *pattern, int patternIndex,
                   char ranks[BOARDSIZE + 1][BOARDSIZE + 1]) {
  const char *match_label = NULL;
  if (ranks[patternIndex][0] == '\0') {
    /* Convert the required rank.
     * Convert the others when/if needed.
     */
    convert_rank_to_text(board, LASTRANK - patternIndex, ranks[patternIndex]);
  }
  while (match_label == NULL && pattern != NULL) {
    if (matchhere(pattern->rank, ranks[patternIndex])) {
      if (patternIndex == BOARDSIZE - 1) {
        /* The board matches the pattern. */
        if (pattern->constraint != NULL) {
          if (constraint_material_match(globals, pattern->constraint, board)) {
            match_label = pattern->optional_label;
          }
        } else {
          match_label = pattern->optional_label;
        }
      } else {
        /* Try next rank.*/
        match_label = pattern_match_rank(globals, board, pattern->next_rank,
                                         patternIndex + 1, ranks);
      }
    }

    if (match_label == NULL) {
      pattern = pattern->alternative_rank;
    }
  }
  return match_label;
}

/**
 * matchhere: search for regexp at beginning of text
 */
static bool matchhere(const char *regexp, const char *text) {
  if (regexp[0] == '\0' && text[0] == '\0') {
    return true;
  }
  if (regexp[0] == ZERO_OR_MORE_OF_ANYTHING) {
    return matchstar(regexp + 1, text);
  }
  if (*text != '\0') {
    switch (*regexp) {
    case ANY_SQUARE_STATE:
      return matchhere(regexp + 1, text + 1);
      break;
    case NON_EMPTY_SQUARE:
    case ANY_WHITE_PIECE:
    case ANY_BLACK_PIECE:
    case NOT_A_PAWN:
      if (matchone(*regexp, *text)) {
        return matchhere(regexp + 1, text + 1);
      }
      break;
    case CCL_START:
      /* Closure */
      if (regexp[1] == NCCL) {
        return matchnccl(regexp + 2, text);
      } else {
        return matchccl(regexp + 1, text);
      }
      break;
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8': {
      /* The number of empty squares required. */
      int empty = regexp[0] - '0';
      bool matches = true;
      /* The number matched. */
      int match_count = 0;
      while (matches && match_count < empty) {
        if (text[match_count] == EMPTY_SQUARE) {
          match_count++;
        } else {
          matches = false;
        }
      }
      if (matches) {
        return matchhere(regexp + 1, text + match_count);
      }
    } break;
    default:
      if (*regexp == *text) {
        return matchhere(regexp + 1, text + 1);
      }
      break;
    }
  }
  /* No match. */
  return false;
}

/**
 * matchstar: leftmost longest search on a single rank.
 */
static bool matchstar(const char *regexp, const char *text) {
  const char *t;

  /* Find the end of this rank. */
  for (t = text; *t != '\0'; t++) {
    ;
  }
  /* Try from the longest match to the shortest until success. */
  do {
    /* * matches zero or more */
    if (matchhere(regexp, t)) {
      return true;
    }
  } while (t-- > text);
  return false;
}

/*
 * Return true if regchar matches textchar, false otherwise.
 */
static bool matchone(char regchar, char textchar) {
  if (regchar == textchar) {
    return true;
  } else {
    switch (regchar) {
    case NON_EMPTY_SQUARE:
      return textchar != EMPTY_SQUARE;
    case ANY_WHITE_PIECE:
      /* Match any white piece. */
      switch (textchar) {
      case 'K':
      case 'Q':
      case 'R':
      case 'N':
      case 'B':
      case 'P':
        return true;
      default:
        return false;
      }
    case ANY_BLACK_PIECE:
      /* Match any black piece. */
      switch (textchar) {
      case 'k':
      case 'q':
      case 'r':
      case 'n':
      case 'b':
      case 'p':
        return true;
      default:
        return false;
      }
    case ANY_SQUARE_STATE:
      return true;
    case NOT_A_PAWN:
      switch (textchar) {
      case 'P':
      case 'p':
        return false;
      default:
        return true;
      }
    default:
      return false;
    }
  }
}

/*
 * Match any of the character closure.
 */
static bool matchccl(const char *regexp, const char *text) {
  while (*regexp != CCL_END && !matchone(*regexp, *text) && *regexp != '\0') {
    regexp++;
  }
  if (matchone(*regexp, *text)) {
    do {
      regexp++;
    } while (*regexp != CCL_END && *regexp != '\0');
    return matchhere(regexp + 1, text + 1);
  } else {
    return false;
  }
}

/*
 * Match any of the characters not in the closure.
 */
static bool matchnccl(const char *regexp, const char *text) {
  while (*regexp != CCL_END && !matchone(*regexp, *text) && *regexp != '\0') {
    regexp++;
  }
  if (*regexp == CCL_END) {
    return matchhere(regexp + 1, text + 1);
  } else {
    return false;
  }
}

#if 0
/* Build a basic EPD string from the given board. */
static char *
convert_board_to_text(const Board *board)
{
    Rank rank;
    int ix = 0;
    /* Allow space for a full board and '/' separators in between. */
    char *text = (char *) malloc_or_die(8 * 8 + 8);
    for (rank = LASTRANK; rank >= FIRSTRANK; rank--) {
        const Piece *rankP = board->board[RankConvert(rank)];
        Col col;
        for (col = FIRSTCOL; col <= LASTCOL; col++) {
            int coloured_piece = rankP[ColConvert(col)];
            if (coloured_piece != EMPTY) {
                text[ix] = coloured_piece_to_SAN_letter(coloured_piece);
            }
            else {
                text[ix] = EMPTY_SQUARE;
            }
            ix++;
        }
        if (rank != FIRSTRANK) {
            text[ix] = '/';
            ix++;
        }
    }
    text[ix] = '\0';
    return text;
}
#endif

/* Build a basic EPD string from rank of the given board. */
static void convert_rank_to_text(const Board *board, Rank rank, char *text) {
  const Piece *rankP = board->board[RankConvert(rank)];
  int ix = 0;
  Col col;
  for (col = FIRSTCOL; col <= LASTCOL; col++) {
    int coloured_piece = rankP[ColConvert(col)];
    if (coloured_piece != EMPTY) {
      text[ix] = coloured_piece_to_SAN_letter(coloured_piece);
    } else {
      text[ix] = EMPTY_SQUARE;
    }
    ix++;
  }
  text[ix] = '\0';
}
