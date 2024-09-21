#ifndef MATERIAL_H
#define MATERIAL_H

#include "typedef.h"

#include <stdbool.h>

/* Define a type to represent classes of occurrance. */
typedef enum {
  EXACTLY,
  NUM_OR_MORE,
  NUM_OR_LESS,
  SAME_AS_OPPONENT,
  NOT_SAME_AS_OPPONENT,
  LESS_THAN_OPPONENT,
  MORE_THAN_OPPONENT,
  LESS_EQ_THAN_OPPONENT,
  MORE_EQ_THAN_OPPONENT
} Occurs;

/* Define a structure to hold details on the occurrances of
 * each of the pieces.
 */
typedef struct MaterialCriteria {
  /* Whether the pieces are to be tried against
   * both colours.
   */
  bool both_colours;
  /* The number of each set of pieces. */
  int num_pieces[2][NUM_PIECE_VALUES];
  Occurs occurs[2][NUM_PIECE_VALUES];
  /* Numbers of general minor pieces. */
  int num_minor_pieces[2];
  Occurs minor_occurs[2];
  /* How long a given relationship must last to be recognised.
   * This value is in half moves.
   */
  unsigned move_depth;
  /* How long a match relationship has been matched.
   * This is always reset to zero on failure and incremented on
   * success. A full match is only returned when match_depth == move_depth.
   */
  unsigned match_depth[2];
  struct MaterialCriteria *next;
} MaterialCriteria;

/*
typedef struct MaterialCriterias {
  Material *curr;
  Material *next;
} MaterialCriterias;
*/

/* Character to separate a pattern from material constraints.
 * NB: This is used to add a material constraint to a FEN pattern.
 */
#define MATERIAL_CONSTRAINT ':'

bool build_endings(const char *infile, bool both_colours);
bool check_for_material_match(Game *game);
bool constraint_material_match(MaterialCriteria *details_to_find,
                               const Board *board);
bool insufficient_material(const Board *board);
MaterialCriteria *process_material_description(const char *line,
                                               bool both_colours,
                                               bool pattern_constraint);

#endif // MATERIAL_H
