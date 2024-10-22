cmake_minimum_required(VERSION 3.21)

project(pgn-extract-3000)

set(LIB_NAME pgne)
set(EXEC_NAME pgn-extract)

set(SOURCE_FILES
    src/apply.h
    src/globals.h
    src/taglines.h
    src/defs.h
    src/map.h
    src/eco.h
    src/taglist.h
    src/fenmatcher.c
    src/material.h
    src/hashing.h
    src/fenmatcher.h
    src/mymalloc.c
    src/moves.h
    src/taglist.c
    src/material.c
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

add_library(${LIB_NAME} STATIC ${SOURCE_FILES})
add_executable(${EXEC_NAME} src/main.c)

target_compile_options(${EXEC_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror
                                            -Wno-unused-parameter)
target_compile_options(${LIB_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror
                                           -Wno-unused-parameter)
target_compile_features(${EXEC_NAME} PRIVATE c_std_17)
target_compile_features(${LIB_NAME} PRIVATE c_std_17)

target_link_libraries(${LIB_NAME} m)
target_link_libraries(${EXEC_NAME} ${LIB_NAME})

install(TARGETS ${LIB_NAME} ${EXEC_NAME} DESTINATION .)
