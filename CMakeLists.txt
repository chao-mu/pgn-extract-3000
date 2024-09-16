cmake_minimum_required(VERSION 3.21)

project(pgn-extract)

set(TARGET_NAME ${PROJECT_NAME})

add_executable(
  pgn-extract
  src/main.c
  src/apply.h
  src/taglines.h
  src/defs.h
  src/map.h
  src/eco.h
  src/taglist.h
  src/fenmatcher.c
  src/end.h
  src/bool.h
  src/hashing.h
  src/fenmatcher.h
  src/mymalloc.c
  src/moves.h
  src/taglist.c
  src/end.c
  src/hashing.c
  src/lines.c
  src/zobrist.h
  src/lex.h
  src/grammar.c
  src/tokens.h
  src/decode.c
  src/apply.c
  src/zobrist.c
  src/mymalloc.h
  src/grammar.h
  src/moves.c
  src/argsfile.c
  src/output.h
  src/output.c
  src/taglines.c
  src/lines.h
  src/eco.c
  src/map.c
  src/decode.h
  src/lex.c
  src/typedef.h
  src/argsfile.h)

target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror
                                              -Wno-unused-parameter)
target_compile_features(${TARGET_NAME} PRIVATE c_std_17)
target_link_libraries(${TARGET_NAME} m)
