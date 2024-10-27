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

#include "grammar.h"

#include "apply.h"
#include "defs.h"
#include "eco.h"
#include "hashing.h"
#include "lex.h"
#include "material.h"
#include "moves.h"
#include "mymalloc.h"
#include "output.h"
#include "taglist.h"
#include "tokens.h"
#include "typedef.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TokenType current_symbol = NO_TOKEN;

/* Keep track of which RAV level we are at.
 * This is used to check whether a TERMINATING_RESULT is the final one
 * and whether NULL_MOVEs are allowed.
 */
static unsigned RAV_level = 0;

/* How often to report processing rate. */
static unsigned PROGRESS_RATE = 1000;

static void parse_opt_game_list(StateInfo *globals, GameHeader *game_header,
                                SourceFileType file_type);
static bool parse_game(StateInfo *globals, GameHeader *game_header,
                       Move **returned_move_list, unsigned long *start_line,
                       unsigned long *end_line);
bool parse_opt_tag_list(StateInfo *globals, GameHeader *game_header);
bool parse_tag(StateInfo *globals, GameHeader *game_header);
static Move *parse_move_list(StateInfo *globals, GameHeader *game_header);
static Move *parse_move_and_variants(StateInfo *globals,
                                     GameHeader *game_header);
static Move *parse_move(StateInfo *globals, GameHeader *game_header);
static Move *parse_move_unit(StateInfo *globals, GameHeader *game_header);
static CommentList *parse_opt_comment_list(StateInfo *globals,
                                           GameHeader *game_header);
bool parse_opt_move_number(StateInfo *globals, GameHeader *game_header);
static void parse_opt_NAG_list(StateInfo *globals, GameHeader *game_header,
                               Move *move_details);
static Variation *parse_opt_variant_list(StateInfo *globals,
                                         GameHeader *game_header);
static Variation *parse_variant(StateInfo *globals, GameHeader *game_header);
static char *parse_result(StateInfo *globals, GameHeader *game_header);

static void setup_for_new_game(void);
static CommentList *append_comment(CommentList *item, CommentList *list);
static void check_result(char **Tags, const char *terminating_result);
static bool check_for_comments(const StateInfo *globals, const Game *game);
static bool chess960_setup(Board *board);
static void deal_with_ECO_line(const StateInfo *globals,
                               GameHeader *game_header, Move *move_list);
static void deal_with_game(StateInfo *globals, GameHeader *game_header,
                           Move *move_list, unsigned long start_line,
                           unsigned long end_line);
static bool finished_processing(const StateInfo *globals);
static void free_tags(GameHeader *game_header);
static CommentList *merge_comment_lists(CommentList *prefix,
                                        CommentList *suffix);
static void output_game(const StateInfo *globals, GameHeader *game_header,
                        Game *game, FILE *outputfile);
static void split_variants(const StateInfo *globals, GameHeader *game_header,
                           Game *game, FILE *outputfile, unsigned depth);

/* Initialise the game header structure to contain
 * space for the default number of tags.
 * The space will have to be increased if new tags are
 * identified in the program source.
 */
GameHeader new_game_header() {
  unsigned i;
  GameHeader game_header = {NULL, 0, NULL};

  game_header.header_tags_length = ORIGINAL_NUMBER_OF_TAGS;
  game_header.Tags = (char **)malloc_or_die(game_header.header_tags_length *
                                            sizeof(*game_header.Tags));

  for (i = 0; i < game_header.header_tags_length; i++) {
    game_header.Tags[i] = (char *)NULL;
  }

  return game_header;
}

void increase_game_header_tags_length(const StateInfo *globals,
                                      GameHeader *game_header,
                                      unsigned new_length) {
  unsigned i;

  if (new_length <= game_header->header_tags_length) {
    fprintf(globals->logfile, "Internal error: inappropriate length %d ",
            new_length);
    fprintf(globals->logfile, " passed to increase_game_header_tags().\n");
    exit(1);
  }
  game_header->Tags = (char **)realloc_or_die(
      (void *)game_header->Tags, new_length * sizeof(*game_header->Tags));
  for (i = game_header->header_tags_length; i < new_length; i++) {
    game_header->Tags[i] = NULL;
  }
  game_header->header_tags_length = new_length;
}

/* Try to open the given file. Error and exit on failure. */
FILE *must_open_file(const StateInfo *globals, const char *filename,
                     const char *mode) {
  FILE *fp;

  fp = fopen(filename, mode);
  if (fp == NULL) {
    fprintf(globals->logfile, "Unable to open the file: \"%s\"\n", filename);
    exit(1);
  }
  return fp;
}

/* Print out on outfp the current details and
 * terminate with a newline.
 */
void report_details(GameHeader *game_header, FILE *outfp) {
  if (game_header->Tags[WHITE_TAG] != NULL) {
    fprintf(outfp, "%s - ", game_header->Tags[WHITE_TAG]);
  }
  if (game_header->Tags[BLACK_TAG] != NULL) {
    fprintf(outfp, "%s ", game_header->Tags[BLACK_TAG]);
  }

  if (game_header->Tags[EVENT_TAG] != NULL) {
    fprintf(outfp, "%s ", game_header->Tags[EVENT_TAG]);
  }
  if (game_header->Tags[SITE_TAG] != NULL) {
    fprintf(outfp, "%s ", game_header->Tags[SITE_TAG]);
  }

  if (game_header->Tags[DATE_TAG] != NULL) {
    fprintf(outfp, "%s ", game_header->Tags[DATE_TAG]);
  }
  putc('\n', outfp);
  fflush(outfp);
}

/* Check that terminating_result is consistent with
 * Tags[RESULT_TAG].
 * If the latter is missing, fill it in from terminating_result.
 * If Tags[RESULT_TAG] is the short form "1/2" then replace it
 * with the long form.
 */
static void check_result(char **Tags, const char *terminating_result) {
  char *result_tag = Tags[RESULT_TAG];

  if (result_tag != NULL && strcmp(result_tag, "1/2") == 0) {
    /* Inappropriate short form. */
    (void)free(result_tag);
    result_tag = Tags[RESULT_TAG] = copy_string("1/2-1/2");
  }

  if (terminating_result != NULL) {
    if ((result_tag == NULL) || (*result_tag == '\0') ||
        (strcmp(result_tag, "?") == 0)) {
      /* Use a copy of terminating result. */
      result_tag = copy_string(terminating_result);
      Tags[RESULT_TAG] = result_tag;
    } else {
      /* Consistency checks done later. */
    }
  }
}

/* Return true if there is at least one comment in the move list,
 * or any of its variations.
 */
static bool comments_in_move_list(const Move *move_list) {
  bool comment_found = false;
  const Move *move = move_list;
  while (!comment_found && move != NULL) {
    if (move->comment_list != NULL) {
      comment_found = true;
    } else {
      if (move->Variants != NULL) {
        const Variation *v = move->Variants;
        while (!comment_found && v != NULL) {
          comment_found = comments_in_move_list(v->moves);
          v = v->next;
        }
        if (!comment_found) {
          move = move->next;
        }
      } else {
        move = move->next;
      }
    }
  }
  return comment_found;
}

/* Check whether only games with comments are to be retained. */
static bool check_for_comments(const StateInfo *globals, const Game *game) {
  if (globals->keep_only_commented_games) {
    if (game->prefix_comment != NULL) {
      return true;
    } else {
      return comments_in_move_list(game->moves);
    }
  } else {
    return true;
  }
}

/* Select which file to write to based upon the game state.
 * This will depend upon:
 *        Whether the number of games per file is limited.
 *        Whether ECO_level > DONT_DIVIDE.
 */
/* A length for filenames. */
#define FILENAME_LENGTH (250)

static FILE *select_output_file(const StateInfo *globals, StateInfo *GameState,
                                const char *eco) {
  if (GameState->games_per_file > 0) {
    if (GameState->games_per_file == 1 ||
        (GameState->num_games_matched % GameState->games_per_file) == 1) {
      /* Time to open the next one. */
      char filename[FILENAME_LENGTH];

      if (GameState->outputfile != NULL) {
        if (globals->json_format && GameState->num_games_matched != 1) {
          /* Terminate the output of the previous file. */
          fputs("\n]\n", globals->outputfile);
        }
        (void)fclose(GameState->outputfile);
      }
      sprintf(filename, "%u%s", GameState->next_file_number,
              output_file_suffix(GameState->output_format));
      GameState->outputfile = must_open_file(globals, filename, "w");
      GameState->next_file_number++;
      if (globals->json_format) {
        fputs("[\n", globals->outputfile);
      }
    }
  } else {
    if (GameState->ECO_level > DONT_DIVIDE) {
      /* Open a file of the appropriate name. */
      if (GameState->outputfile != NULL) {
        /* @@@ In practice, this might need refinement.
         * Repeated opening and closing may prove inefficient.
         */
        (void)fclose(GameState->outputfile);
        GameState->outputfile =
            open_eco_output_file(globals, GameState->ECO_level, eco);
      }
    } else if (globals->json_format && GameState->num_games_matched == 1) {
      fputs("[\n", globals->outputfile);
    } else {
    }
  }
  return GameState->outputfile;
}

/*
 * Conditions for finishing processing, other than all the input
 * having been processed.
 */
static bool finished_processing(const StateInfo *globals) {
  return (globals->matching_game_numbers != NULL &&
          globals->next_game_number_to_output == NULL) ||
         (globals->maximum_matches > 0 &&
          globals->num_games_matched == globals->maximum_matches) ||
         globals->num_games_processed >= globals->game_limit;
}

/*
 * Is the given game number within the range to be matched?
 */
static bool in_game_number_range(unsigned long number, game_number *range) {
  return range != NULL && range->min <= number && number <= range->max;
}

static void parse_opt_game_list(StateInfo *globals, GameHeader *game_header,
                                SourceFileType file_type) {
  Move *move_list = NULL;
  unsigned long start_line, end_line;

  while (parse_game(globals, game_header, &move_list, &start_line, &end_line) &&
         !finished_processing(globals)) {
    if (file_type == NORMALFILE) {
      deal_with_game(globals, game_header, move_list, start_line, end_line);
    } else if (file_type == CHECKFILE) {
      deal_with_game(globals, game_header, move_list, start_line, end_line);
    } else if (file_type == ECOFILE) {
      if (move_list != NULL) {
        deal_with_ECO_line(globals, game_header, move_list);
      } else {
        fprintf(globals->logfile, "ECO line with zero moves.\n");
        report_details(game_header, globals->logfile);
      }
    } else {
      /* Unknown type. */
      free_tags(game_header);
      free_move_list(game_header, move_list);
    }
    move_list = NULL;
    setup_for_new_game();
  }
  if (move_list != NULL) {
    free_move_list(game_header, move_list);
  }
}

/* Parse a game and return a pointer to any valid list of moves
 * in returned_move_list.
 */
static bool
parse_game(StateInfo *globals, GameHeader *game_header,
           Move **returned_move_list, unsigned long *start_line,
           unsigned long *end_line) { /* bool something_found = false; */
  CommentList *prefix_comment;
  Move *move_list = NULL;
  char *result;
  /* There shouldn't be a hanging comment before the result,
   * but there sometimes is.
   */
  CommentList *hanging_comment;

  /* Assume that we won't return anything. */
  *returned_move_list = NULL;
  /* Skip over any junk between games. */
  current_symbol = skip_to_next_game(globals, game_header, current_symbol);
  prefix_comment = parse_opt_comment_list(globals, game_header);
  if (prefix_comment != NULL) {
    /* Free this here, as it is hard to
     * know whether it belongs to the game or the file.
     * It is better to put game comments after the tags.
     */
    /* something_found = true; */
    free_comment_list(game_header, prefix_comment);
    prefix_comment = NULL;
  }
  *start_line = get_line_number();
  if (parse_opt_tag_list(globals, game_header)) {
    /* something_found = true; */
  }

  /* Some games have an initial NAG as a print-board indication.
   * This is not legal PGN.
   * Silently delete it/them.
   */
  while (current_symbol == NAG) {
    current_symbol = next_token(globals, game_header);
  }

  /* @@@ Beware of comments and/or tags without moves. */
  move_list = parse_move_list(globals, game_header);

  /* @@@ Look for a comment with no move text before the result. */
  hanging_comment = parse_opt_comment_list(globals, game_header);
  /* Append this to the final move, if there is one. */

  /* Look for a result, even if there were no moves. */
  result = parse_result(globals, game_header);
  *end_line = get_line_number();
  if (move_list != NULL) {
    /* Find the last move. */
    Move *last_move = move_list;

    while (last_move->next != NULL) {
      last_move = last_move->next;
    }
    if (hanging_comment != NULL) {
      last_move->comment_list =
          append_comment(hanging_comment, last_move->comment_list);
    }
    if (result != NULL) {
      /* Append it to the last move. */
      last_move->terminating_result = result;
      check_result(game_header->Tags, result);
      *returned_move_list = move_list;
    } else {
      fprintf(globals->logfile, "Missing result.\n");
      report_details(game_header, globals->logfile);
    }
    /* something_found = true; */
  } else {
    /* @@@ Nothing to attach the comment to. */
    (void)free((void *)hanging_comment);
    hanging_comment = NULL;
    /*
     * Workaround for games with zero moves.
     * Check the result for consistency with the tags, but then
     * there is no move to attach it to.
     * When outputting a game, the missing result in this case
     * will have to be supplied from the tags.
     */
    check_result(game_header->Tags, result);
    if (result != NULL) {
      (void)free((void *)result);
    }
    *returned_move_list = NULL;
  }
  return current_symbol != EOF_TOKEN;
}

bool parse_opt_tag_list(StateInfo *globals, GameHeader *game_header) {
  bool something_found = false;
  CommentList *prefix_comment;

  while (parse_tag(globals, game_header)) {
    something_found = true;
  }
  prefix_comment = parse_opt_comment_list(globals, game_header);
  if (prefix_comment != NULL) {
    game_header->prefix_comment = prefix_comment;
    something_found = true;
  }
  return something_found;
}

/* Return true if it looks like board contains an initial
 * Chess 960 setup position. false otherwise.
 * Assessment requires that:
 *     + The move number be 1.
 *     + All castling rights are intact.
 *     + The two home ranks are full.
 *     + Identical pieces are opposite each other on the back rank.
 *     + At least one piece is out of its standard position.
 */
static bool chess960_setup(Board *board) {
  if (board->move_number == 1 && board->WKingRank == '1' &&
      board->BKingRank == '8' && board->WKingCol == board->BKingCol &&
      (board->WKingCastle != '\0' && board->WQueenCastle != '\0' &&
       board->BKingCastle != '\0' && board->BQueenCastle != '\0')) {
    /* Check for a full set of pawns. */
    bool probable = true;
    int white_r = RankConvert('2');
    int black_r = RankConvert('7');
    Piece white_pawn = MAKE_COLOURED_PIECE(WHITE, PAWN);
    Piece black_pawn = MAKE_COLOURED_PIECE(BLACK, PAWN);
    for (int c = ColConvert('a'); c <= ColConvert('h') && probable; c++) {
      probable = board->board[white_r][c] == white_pawn &&
                 board->board[black_r][c] == black_pawn;
      /* Make sure the back rank is full and identical pieces
       * are opposite each other.
       */
      if (probable) {
        probable = board->board[white_r - 1][c] != EMPTY &&
                   EXTRACT_PIECE(board->board[white_r - 1][c]) ==
                       EXTRACT_PIECE(board->board[black_r + 1][c]);
      }
    }
    if (probable) {
      /* Check for at least one piece type being out of position. */
      /* Only need to check one colour because we already know the
       * pieces are identically paired.
       */
      white_r = RankConvert('1');
      probable = board->board[white_r][ColConvert('a')] != W(ROOK) ||
                 board->board[white_r][ColConvert('b')] != W(KNIGHT) ||
                 board->board[white_r][ColConvert('c')] != W(BISHOP) ||
                 board->board[white_r][ColConvert('d')] != W(QUEEN) ||
                 board->board[white_r][ColConvert('e')] != W(KING) ||
                 board->board[white_r][ColConvert('f')] != W(BISHOP) ||
                 board->board[white_r][ColConvert('g')] != W(KNIGHT) ||
                 board->board[white_r][ColConvert('h')] != W(ROOK);
    }
    return probable;
  } else {
    return false;
  }
}

bool parse_tag(StateInfo *globals, GameHeader *game_header) {
  bool TagFound = true;

  if (current_symbol == TAG) {
    TagName tag_index = yylval.tag_index;

    current_symbol = next_token(globals, game_header);
    if (current_symbol == STRING) {
      char *tag_string = yylval.token_string;

      if (tag_index < game_header->header_tags_length) {
        game_header->Tags[tag_index] = tag_string;
      } else {
        print_error_context(globals, globals->logfile);
        fprintf(globals->logfile,
                "Internal error: Illegal tag index %d for %s\n", tag_index,
                tag_string);
        exit(1);
      }
      current_symbol = next_token(globals, game_header);
    } else {
      print_error_context(globals, globals->logfile);
      fprintf(globals->logfile, "Missing tag string.\n");
    }
    if (current_symbol == TAG_END) {
      current_symbol = next_token(globals, game_header);
    } else {
      print_error_context(globals, globals->logfile);
      fprintf(globals->logfile, "Missing ]\n");
    }
  } else if (current_symbol == STRING) {
    print_error_context(globals, globals->logfile);
    fprintf(globals->logfile, "Missing tag for %s.\n", yylval.token_string);
    (void)free((void *)yylval.token_string);
    current_symbol = next_token(globals, game_header);
    if (current_symbol == TAG_END) {
      current_symbol = next_token(globals, game_header);
    } else {
      /* No point reporting the error. */
    }
  } else {
    TagFound = false;
  }
  return TagFound;
}

static Move *parse_move_list(StateInfo *globals, GameHeader *game_header) {
  Move *head = NULL, *tail = NULL;

  head = parse_move_and_variants(globals, game_header);
  if (head != NULL) {
    Move *next_move;

    tail = head;
    while ((next_move = parse_move_and_variants(globals, game_header)) !=
           NULL) {
      tail->next = next_move;
      tail = next_move;
    }
  }
  return head;
}

static Move *parse_move_and_variants(StateInfo *globals,
                                     GameHeader *game_header) {
  Move *move_details;

  move_details = parse_move(globals, game_header);
  if (move_details != NULL) {
    CommentList *comment;

    move_details->Variants = parse_opt_variant_list(globals, game_header);
    comment = parse_opt_comment_list(globals, game_header);
    if (comment != NULL) {
      move_details->comment_list =
          append_comment(comment, move_details->comment_list);
    }
  }
  return move_details;
}

static Move *parse_move(StateInfo *globals, GameHeader *game_header) {
  Move *move_details = NULL;

  if (parse_opt_move_number(globals, game_header)) {
  }
  /* @@@ Watch out for finding just the number. */
  move_details = parse_move_unit(globals, game_header);
  if (move_details != NULL) {
    parse_opt_NAG_list(globals, game_header, move_details);
    /* Any trailing comments will have been picked up
     * and attached to the NAGs.
     */
  }
  return move_details;
}

static Move *parse_move_unit(StateInfo *globals, GameHeader *game_header) {
  Move *move_details = NULL;

  if (current_symbol == MOVE) {
    move_details = yylval.move_details;

    if (move_details->class == NULL_MOVE && RAV_level == 0) {
      if (!globals->allow_null_moves) {
        print_error_context(globals, globals->logfile);
        fprintf(globals->logfile,
                "Null moves (--) only allowed in variations.\n");
      }
    }

    current_symbol = next_token(globals, game_header);
    if (current_symbol == CHECK_SYMBOL) {
      strcat((char *)move_details->move, "+");
      current_symbol = next_token(globals, game_header);
      /* Sometimes + is followed by #, so cover this case. */
      if (current_symbol == CHECK_SYMBOL) {
        current_symbol = next_token(globals, game_header);
      }
    }
    move_details->comment_list = parse_opt_comment_list(globals, game_header);
  }
  return move_details;
}

static CommentList *parse_opt_comment_list(StateInfo *globals,
                                           GameHeader *game_header) {
  CommentList *head = NULL, *tail = NULL;

  while (current_symbol == COMMENT) {
    if (head == NULL) {
      head = tail = yylval.comment;
    } else {
      tail->next = yylval.comment;
      tail = tail->next;
    }
    current_symbol = next_token(globals, game_header);
  }
  return head;
}

bool parse_opt_move_number(StateInfo *globals, GameHeader *game_header) {
  bool something_found = false;

  if (current_symbol == MOVE_NUMBER) {
    current_symbol = next_token(globals, game_header);
    something_found = true;
  }
  return something_found;
}

/**
 * Parse 0 or more NAGs, optionally followed by 0 or more comments.
 * @param move_details
 */
static void parse_opt_NAG_list(StateInfo *globals, GameHeader *game_header,
                               Move *move_details) {
  while (current_symbol == NAG) {
    Nag *details = (Nag *)malloc_or_die(sizeof(*details));
    details->text = NULL;
    details->comments = NULL;
    details->next = NULL;
    do {
      details->text = save_string_list_item(details->text, yylval.token_string);
      current_symbol = next_token(globals, game_header);
    } while (current_symbol == NAG);
    details->comments = parse_opt_comment_list(globals, game_header);
    if (move_details->NAGs == NULL) {
      move_details->NAGs = details;
    } else {
      Nag *nextNAG = move_details->NAGs;
      while (nextNAG->next != NULL) {
        nextNAG = nextNAG->next;
      }
      nextNAG->next = details;
    }
  }
}

static Variation *parse_opt_variant_list(StateInfo *globals,
                                         GameHeader *game_header) {
  Variation *head = NULL, *tail = NULL, *variation;

  while ((variation = parse_variant(globals, game_header)) != NULL) {
    if (head == NULL) {
      head = tail = variation;
    } else {
      tail->next = variation;
      tail = variation;
    }
  }
  return head;
}

static Variation *parse_variant(StateInfo *globals, GameHeader *game_header) {
  Variation *variation = NULL;

  if (current_symbol == RAV_START) {
    CommentList *prefix_comment;
    CommentList *suffix_comment;
    char *result = NULL;
    Move *moves;

    RAV_level++;
    variation = (Variation *)malloc_or_die(sizeof(Variation));

    current_symbol = next_token(globals, game_header);
    prefix_comment = parse_opt_comment_list(globals, game_header);
    moves = parse_move_list(globals, game_header);
    if (moves == NULL) {
      print_error_context(globals, globals->logfile);
      fprintf(globals->logfile, "Missing move list in variation.\n");
    } else if (globals->lichess_comment_fix && prefix_comment != NULL) {
      /* lichess study deletes the prefix comment, so
       * move it after the first move of the variation.
       */
      moves->comment_list =
          merge_comment_lists(prefix_comment, moves->comment_list);
      prefix_comment = NULL;
    }
    result = parse_result(globals, game_header);
    if ((result != NULL) && (moves != NULL)) {
      /* Find the last move, to which to append the terminating
       * result.
       */
      Move *last_move = moves;
      CommentList *trailing_comment;

      while (last_move->next != NULL) {
        last_move = last_move->next;
      }
      last_move->terminating_result = result;
      /* Accept a comment after the result, but it will
       * be printed out preceding the result.
       */
      trailing_comment = parse_opt_comment_list(globals, game_header);
      if (trailing_comment != NULL) {
        last_move->comment_list =
            append_comment(trailing_comment, last_move->comment_list);
      }
    } else {
      /* Ok. */
    }
    if (current_symbol == RAV_END) {
      RAV_level--;
      current_symbol = next_token(globals, game_header);
    } else {
      fprintf(globals->logfile, "Missing ')' to close variation.\n");
      print_error_context(globals, globals->logfile);
    }
    suffix_comment = parse_opt_comment_list(globals, game_header);
    variation->prefix_comment = prefix_comment;
    variation->suffix_comment = suffix_comment;
    variation->moves = moves;
    variation->next = NULL;
  }
  return variation;
}

static char *parse_result(StateInfo *globals, GameHeader *game_header) {
  char *result = NULL;

  if (current_symbol == TERMINATING_RESULT) {
    result = yylval.token_string;
    if (RAV_level == 0) {
      /* In the interests of skipping any intervening material
       * between games, set the lookahead to a dummy token.
       */
      current_symbol = NO_TOKEN;
    } else {
      current_symbol = next_token(globals, game_header);
    }
  }
  return result;
}

static void setup_for_new_game(void) {
  restart_lex_for_new_game();
  RAV_level = 0;
}

/* Discard any data held in the game_header->Tags structure. */
static void free_tags(GameHeader *game_header) {
  unsigned tag;

  for (tag = 0; tag < game_header->header_tags_length; tag++) {
    if (game_header->Tags[tag] != NULL) {
      free(game_header->Tags[tag]);
      game_header->Tags[tag] = NULL;
    }
  }
}

/* Discard data from a gathered game. */
void free_string_list(StringList *list) {
  StringList *next;

  while (list != NULL) {
    next = list;
    list = list->next;
    if (next->str != NULL) {
      (void)free((void *)next->str);
    }
    (void)free((void *)next);
  }
}

void free_comment_list(GameHeader *game_header, CommentList *comment_list) {
  while (comment_list != NULL) {
    CommentList *this_comment = comment_list;

    if (comment_list->comment != NULL) {
      free_string_list(comment_list->comment);
    }
    comment_list = comment_list->next;
    (void)free((void *)this_comment);
  }
}

static void free_variation(GameHeader *game_header, Variation *variation) {
  Variation *next;

  while (variation != NULL) {
    next = variation;
    variation = variation->next;
    if (next->prefix_comment != NULL) {
      free_comment_list(game_header, next->prefix_comment);
    }
    if (next->suffix_comment != NULL) {
      free_comment_list(game_header, next->suffix_comment);
    }
    if (next->moves != NULL) {
      (void)free_move_list(game_header, next->moves);
    }
    (void)free((void *)next);
  }
}

static void free_NAG_list(GameHeader *game_header, Nag *nag_list) {
  while (nag_list != NULL) {
    Nag *nextNAG = nag_list->next;
    free_string_list(nag_list->text);
    free_comment_list(game_header, nag_list->comments);
    (void)free((void *)nag_list);
    nag_list = nextNAG;
  }
}

void free_move_list(GameHeader *game_header, Move *move_list) {
  Move *nextMove;

  while (move_list != NULL) {
    nextMove = move_list;
    move_list = move_list->next;

    free_NAG_list(game_header, nextMove->NAGs);
    free_comment_list(game_header, nextMove->comment_list);
    free_variation(game_header, nextMove->Variants);

    if (nextMove->epd != NULL) {
      (void)free((void *)nextMove->epd);
    }
    if (nextMove->fen_suffix != NULL) {
      (void)free((void *)nextMove->fen_suffix);
      nextMove->fen_suffix = NULL;
    }
    if (nextMove->terminating_result != NULL) {
      (void)free((void *)nextMove->terminating_result);
    }

    (void)free((void *)nextMove);
  }
}

/* Add str onto the tail of list and
 * return the head of the resulting list.
 */
StringList *save_string_list_item(StringList *list, const char *str) {
  if (str != NULL && *str != '\0') {
    StringList *new_item;

    new_item = (StringList *)malloc_or_die(sizeof(*new_item));
    new_item->str = str;
    new_item->next = NULL;
    if (list == NULL) {
      list = new_item;
    } else {
      StringList *tail = list;

      while (tail->next != NULL) {
        tail = tail->next;
      }
      tail->next = new_item;
    }
  }
#if 1
  /* This is almost certainly correct to avoid losing
   * two bytes with malloc'd empty strings.
   * No problems with valgrind but just being
   * cautious.
   */
  else if (str != NULL) {
    (void)free((void *)str);
  }
#endif
  return list;
}

/* Append any comments in Comment onto the end of
 * any associated with move.
 */
void append_comments_to_move(GameHeader *game_header, Move *move,
                             CommentList *comment) {
  if (comment != NULL) {
    move->comment_list = append_comment(comment, move->comment_list);
  }
}

/* Add item to the end of list.
 * If list is empty, return item.
 */
static CommentList *append_comment(CommentList *item, CommentList *list) {
  if (list == NULL) {
    return item;
  } else {
    CommentList *tail = list;

    while (tail->next != NULL) {
      tail = tail->next;
    }
    tail->next = item;
    return list;
  }
}

/* Add the suffix list (if any) to the end of the prefix list.
 */
static CommentList *merge_comment_lists(CommentList *prefix,
                                        CommentList *suffix) {
  if (prefix == NULL) {
    return suffix;
  } else if (suffix != NULL) {
    CommentList *tail = prefix;
    while (tail->next != NULL) {
      tail = tail->next;
    }
    tail->next = suffix;
  }
  return prefix;
}

/* Check for consistency of any FEN-related tags. */
static bool consistent_FEN_tags(const StateInfo *globals,
                                GameHeader *game_header, Game *current_game) {
  bool consistent = true;

  if ((current_game->tags[SETUP_TAG] != NULL) &&
      (strcmp(current_game->tags[SETUP_TAG], "1") == 0)) {
    /* There must be a FEN_TAG to go with it. */
    if (current_game->tags[FEN_TAG] == NULL) {
      consistent = false;
      report_details(game_header, globals->logfile);
      fprintf(globals->logfile, "Missing %s Tag to accompany %s Tag.\n",
              tag_header_string(globals, FEN_TAG),
              tag_header_string(globals, SETUP_TAG));
      print_error_context(globals, globals->logfile);
    }
  }
  if (current_game->tags[FEN_TAG] != NULL) {
    Board *board =
        new_fen_board(globals, game_header, current_game->tags[FEN_TAG]);
    if (board != NULL) {
      /* There must be a SETUP_TAG to go with it. */
      if (current_game->tags[SETUP_TAG] == NULL) {
        // This is such a common problem that it makes
        // more sense just to silently correct it.
#if 0
                report_details(globals->logfile, game_header);
                fprintf(globals->logfile,
                        "Missing %s Tag to accompany %s Tag.\n",
                        tag_header_string(SETUP_TAG),
                        tag_header_string(FEN_TAG));
                print_error_context(globals,globals->logfile);
#endif
        /* Fix the inconsistency. */
        current_game->tags[SETUP_TAG] = copy_string("1");
      }

      bool chess960 = chess960_setup(board);
      if (current_game->tags[VARIANT_TAG] == NULL) {
        /* See if there should be a Variant tag. */
        /* Look for an initial position found in Chess 960. */
        if (chess960) {
          const char *missing_value = "chess 960";
          report_details(game_header, globals->logfile);
          fprintf(globals->logfile,
                  "Missing %s Tag for non-standard setup; adding %s.\n",
                  tag_header_string(globals, VARIANT_TAG), missing_value);
          /* Fix the inconsistency. */
          current_game->tags[VARIANT_TAG] = copy_string(missing_value);
        } else if (globals->add_fen_castling) {
          /* If add_fen_castling is true and castling permissions are absent
           * then liberally assume them based on the King and Rook positions.
           */
          if (!board->WKingCastle && !board->WQueenCastle &&
              !board->BKingCastle && !board->BQueenCastle) {
            add_fen_castling(globals, game_header, current_game, board);
          }
        }
      } else if (chess960) {
        /* @@@ Should really make sure the Variant tag is appropriate. */
      }

      free_board(board);
    } else {
      consistent = false;
    }
  }
  return consistent;
}

static void deal_with_game(StateInfo *globals, GameHeader *game_header,
                           Move *move_list, unsigned long start_line,
                           unsigned long end_line) {
  Game current_game;
  /* We need a dummy argument for apply_move_list. */
  unsigned plycount;
  /* Whether the game matches, as long as it is not in a CHECKFILE. */
  bool game_matches = false;
  /* Whether to output the game. */
  bool output_the_game = false;

  if (globals->current_file_type != CHECKFILE) {
    /* Update the count of how many games handled. */
    globals->num_games_processed++;
  }

  /* Fill in the information currently known. */
  current_game.tags = game_header->Tags;
  current_game.tags_length = game_header->header_tags_length;
  current_game.prefix_comment = game_header->prefix_comment;
  current_game.moves = move_list;
  current_game.moves_checked = false;
  current_game.moves_ok = false;
  current_game.error_ply = 0;
  current_game.position_counts = NULL;
  current_game.start_line = start_line;
  current_game.end_line = end_line;

  /* Determine whether or not this game is wanted, on the
   * basis of the various selection criteria available.
   */

  /*
   * apply_move_list checks out the moves.
   * If it returns true as a match, it will also fill in the
   *     current_game.final_hash_value and
   *     current_game.cumulative_hash_value
   * fields of current_game so that these can be used in the
   * previous_occurrence function.
   *
   * If there are any tag criteria, it will be easy to quickly
   * eliminate most games without going through the lengthy
   * process of game matching.
   *
   * If ECO adding is done, the order of checking may cause
   * a conflict here since it won't be possible to reject a game
   * based on its ECO code unless it already has one.
   * Therefore, check for the ECO tag only after everything else has
   * been checked.
   */
  if (consistent_FEN_tags(globals, game_header, &current_game) &&
      check_tag_details_not_ECO(globals, current_game.tags,
                                current_game.tags_length, true) &&
      check_setup_tag(globals, current_game.tags) &&
      check_duplicate_setup(globals, game_header, &current_game) &&
      apply_move_list(globals, game_header, &current_game, &plycount,
                      globals->depth_of_positional_search, true) &&
      check_move_bounds(globals, plycount) &&
      check_textual_variations(globals, &current_game) &&
      check_for_material_match(globals, game_header, &current_game) &&
      check_for_only_checkmate(globals, &current_game) &&
      check_for_only_repetition(globals, current_game.position_counts) &&
      check_ECO_tag(globals, current_game.tags, true) &&
      check_for_comments(globals, &current_game)) {
    /* If there is no original filename then the game is not a
     * duplicate.
     */
    const char *original_filename =
        previous_occurance(globals, current_game, plycount);

    if ((original_filename == NULL) && globals->suppress_originals) {
      /* Don't output first occurrences. */
    } else if ((original_filename == NULL) || !globals->suppress_duplicates) {
      if (globals->current_file_type == CHECKFILE) {
        /* We are only checking, so don't count this as a matched game. */
      } else if (globals->num_games_processed >= globals->first_game_number) {
        game_matches = true;
        globals->num_games_matched++;
        if (globals->matching_game_numbers != NULL &&
            !in_game_number_range(globals->num_games_matched,
                                  globals->next_game_number_to_output)) {
          /* This is not the right matching game to be output. */
        } else if (globals->skip_game_numbers != NULL &&
                   in_game_number_range(globals->num_games_matched,
                                        globals->next_game_number_to_skip)) {
          /* Skip this matching game. */
          if (globals->num_games_matched ==
              globals->next_game_number_to_skip->max) {
            globals->next_game_number_to_skip =
                globals->next_game_number_to_skip->next;
          }
        } else if (globals->check_only) {
          /* We are only checking. */
          if (globals->verbosity > 1) {
            /* Report progress on logfile. */
            report_details(game_header, globals->logfile);
          }
        } else {
          output_the_game = true;
        }
      } else {
        /* Not wanted. */
      }
      if (output_the_game) {
        /* This game is to be kept and output. */
        FILE *outputfile =
            select_output_file(globals, globals, current_game.tags[ECO_TAG]);

        /* See if we wish to separate out duplicates. */
        if ((original_filename != NULL) && (globals->duplicate_file != NULL)) {
          static const char *last_input_file = NULL;

          outputfile = globals->duplicate_file;
          if ((last_input_file != globals->current_input_file) &&
              (globals->current_input_file != NULL)) {
            if (globals->keep_comments) {
              /* Record which file this and succeeding
               * duplicates come from.
               */
              print_str(globals, game_header, outputfile, "{ From: ");
              print_str(globals, game_header, outputfile,
                        globals->current_input_file);
              print_str(globals, game_header, outputfile, " }");
              terminate_line(globals, outputfile);
            }
            last_input_file = globals->current_input_file;
          }
          if (globals->keep_comments) {
            print_str(globals, game_header, outputfile, "{ First found in: ");
            print_str(globals, game_header, outputfile, original_filename);
            print_str(globals, game_header, outputfile, " }");
            terminate_line(globals, outputfile);
          }
        }
        if (!globals->suppress_matched) {
          /* Now output what we have. */
          output_game(globals, game_header, &current_game, outputfile);
          if (globals->verbosity > 1) {
            /* Report progress on logfile. */
            report_details(game_header, globals->logfile);
          }
        }
      }
    }
  }
  if (!game_matches && (globals->non_matching_file != NULL) &&
      globals->current_file_type != CHECKFILE) {
    /* The user wants to keep everything else. */
    if (!current_game.moves_checked) {
      /* Make sure that the move text is in a reasonable state.
       * Force checking of the whole game.
       */
      (void)apply_move_list(globals, game_header, &current_game, &plycount, 0,
                            false);
    }
    if (current_game.moves_ok || globals->keep_broken_games) {
      if (globals->json_format) {
        if (globals->num_non_matching_games == 0) {
          fputs("[\n", globals->non_matching_file);
        } else {
          fputs(",\n", globals->non_matching_file);
        }
      }
      globals->num_non_matching_games++;
      output_game(globals, game_header, &current_game,
                  globals->non_matching_file);
    }
  }
  if (game_matches && globals->matching_game_numbers != NULL &&
      in_game_number_range(globals->num_games_matched,
                           globals->next_game_number_to_output)) {
    if (globals->num_games_matched ==
        globals->next_game_number_to_output->max) {
      globals->next_game_number_to_output =
          globals->next_game_number_to_output->next;
    }
  }

  /* Game is finished with, so free everything. */
  if (game_header->prefix_comment != NULL) {
    free_comment_list(game_header, game_header->prefix_comment);
  }
  /* Ensure that the GameHeader's prefix comment is NULL for
   * the next game.
   */
  game_header->prefix_comment = NULL;

  free_tags(game_header);
  free_move_list(game_header, current_game.moves);
  if (current_game.position_counts != NULL) {
    free_position_count_list(current_game.position_counts);
    current_game.position_counts = NULL;
  }
  if (globals->verbosity != 0 &&
      (globals->num_games_processed % PROGRESS_RATE) == 0) {
    fprintf(stderr, "Games: %lu\r", globals->num_games_processed);
  }
}

/*
 * Output the given game to the output file.
 * If globals->split_variants then this will involve outputting
 * each variation separately.
 */
static void output_game(const StateInfo *globals, GameHeader *game_header,
                        Game *game, FILE *outputfile) {
  if (globals->split_variants && globals->keep_variations) {
    split_variants(globals, game_header, game, outputfile, 0);
  } else {
    format_game(globals, game_header, game, outputfile);
  }
}

/*
 * Output each variation separately, to the required depth.
 * NB: This involves the removal of all variations from the game.
 * This is done recursively and depth (>=0) defines the current
 * level of recursion.
 */
static void split_variants(const StateInfo *globals, GameHeader *game_header,
                           Game *game, FILE *outputfile, unsigned depth) {
  /* Gather all the suffix comments at this level. */
  Move *move = game->moves;
  while (move != NULL) {
    Variation *variants = move->Variants;
    while (variants != NULL) {
      if (variants->suffix_comment != NULL) {
        move->comment_list =
            append_comment(variants->suffix_comment, move->comment_list);
        variants->suffix_comment = NULL;
      }
      variants = variants->next;
    }
    move = move->next;
  }

  /* Format the main line at this level. */
  format_game(globals, game_header, game, outputfile);

  if (globals->split_depth_limit == 0 || globals->split_depth_limit > depth) {
    /* Now all the variations. */
    char *result_tag = game->tags[RESULT_TAG];
    game->tags[RESULT_TAG] = copy_string("*");
    move = game->moves;
    Move *prev = NULL;
    while (move != NULL) {
      Variation *variants = move->Variants;
      while (variants != NULL) {
        Variation *next_variant = variants->next;
        Move *variant_moves = variants->moves;
        if (variant_moves != NULL) {
          /* Supply a result if it is missing. */
          Move *last_move = variant_moves;
          while (last_move->next != NULL) {
            last_move = last_move->next;
          }
          if (last_move->terminating_result == NULL) {
            last_move->terminating_result = copy_string("*");
          }
          /* Replace the main line with the variants. */
          if (prev != NULL) {
            prev->next = variant_moves;
          } else {
            game->moves = variant_moves;
          }
          /* Detach following variations. */
          variants->next = NULL;
          CommentList *prefix_comment = variants->prefix_comment;
          if (prefix_comment != NULL) {
            if (prev != NULL) {
              prev->comment_list =
                  append_comment(prefix_comment, prev->comment_list);
            } else {
              game->prefix_comment =
                  append_comment(prefix_comment, game->prefix_comment);
            }
          }
          split_variants(globals, game_header, game, outputfile, depth + 1);
          if (prefix_comment != NULL) {
            /* Remove the appended comments. */
            CommentList *list;
            if (prev != NULL) {
              list = prev->comment_list;
            } else {
              list = game->prefix_comment;
            }
            if (list == prefix_comment) {
              if (prev != NULL) {
                prev->comment_list = NULL;
              } else {
                game->prefix_comment = NULL;
              }
            } else {
              while (list->next != prefix_comment && list->next != NULL) {
                list = list->next;
              }
              list->next = NULL;
            }
          }
          variants->next = next_variant;
        }
        variants = next_variant;
      }
      if (move->Variants != NULL) {
        /* The variation can now be disposed of. */
        free_variation(game_header, move->Variants);
        move->Variants = NULL;
        /* Restore the move replaced by its variants. */
        if (prev != NULL) {
          prev->next = move;
        } else {
          game->moves = move;
        }
      }
      prev = move;
      move = move->next;
    }
    /* Put everything back as it was. */
    (void)free((void *)game->tags[RESULT_TAG]);
    game->tags[RESULT_TAG] = result_tag;
  }
}

static void deal_with_ECO_line(const StateInfo *globals,
                               GameHeader *game_header, Move *move_list) {
  Game current_game;
  /* We need to know the length of a game to store with the
   * hash information as a sanity check.
   */
  unsigned number_of_half_moves;

  /* Fill in the information currently known. */
  current_game.tags = game_header->Tags;
  current_game.tags_length = game_header->header_tags_length;
  current_game.prefix_comment = game_header->prefix_comment;
  current_game.moves = move_list;
  current_game.moves_checked = false;
  current_game.moves_ok = false;
  current_game.error_ply = 0;

  /* apply_eco_move_list checks out the moves.
   * It will also fill in the
   *                 current_game.final_hash_value and
   *                 current_game.cumulative_hash_value
   * fields of current_game.
   */
  Board *final_position = apply_eco_move_list(
      globals, game_header, &current_game, &number_of_half_moves);
  if (final_position != NULL) {
    /* Store the ECO code in the appropriate hash location. */
    save_eco_details(globals, &current_game, final_position,
                     number_of_half_moves);
  }

  /* Game is finished with, so free everything. */
  if (game_header->prefix_comment != NULL) {
    free_comment_list(game_header, game_header->prefix_comment);
  }
  /* Ensure that the GameHeader's prefix comment is NULL for
   * the next game.
   */
  game_header->prefix_comment = NULL;

  free_tags(game_header);
  free_move_list(game_header, current_game.moves);
}

/* If file_type == ECOFILE we are dealing with a file of ECO
 * input rather than a normal game file.
 */
int yyparse(StateInfo *globals, GameHeader *game_header,
            SourceFileType file_type) {
  setup_for_new_game();
  current_symbol = skip_to_next_game(globals, game_header, NO_TOKEN);
  parse_opt_game_list(globals, game_header, file_type);
  if (current_symbol == EOF_TOKEN) {
    /* Ok -- EOF. */
    return 0;
  } else if (finished_processing(globals)) {
    /* Ok -- done all we need to. */
    return 0;
  } else {
    fprintf(globals->logfile, "End of input reached before end of file.\n");
    return 1;
  }
}
