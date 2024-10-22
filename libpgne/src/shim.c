#include "grammar.h"
#include "lex.h"
#include "map.h"
#include "taglist.h"
#include "typedef.h"

/**
 * The following is a shim to expose functions to Rust
 * during the migration to rust
 */
void legacy_init_all_globals() {
  /* Prepare the Game_Header. */
  init_game_header();
  /* Prepare the tag lists for -t/-T matching. */
  init_tag_lists();
  /* Prepare the hash tables for transposition detection. */
  init_hashtab();
  /* Initialise the lexical analyser's tables. */
  init_lex_tables();
}

int legacy_yyparse(StateInfo *globals) {
  return yyparse(globals, globals->current_file_type);
}

void legacy_add_filename_to_source_list(const StateInfo *globals,
                                        const char *filename) {
  add_filename_to_source_list(globals, filename, NORMALFILE);
}
