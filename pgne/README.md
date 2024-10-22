# pgn-extract-3000

A Portable Game Notation (PGN) Manipulator for Chess Games.

This is a fork of pgn-extract based on version 24-10 by [David J. Barnes](https://www.cs.kent.ac.uk/~djb/) ( [@kentdjb](https://twitter.com/kentdjb))  [![RSS feed image](feed.png)](pgn-extract.xml). Please use the original, it's probably better.

## Introduction

The _pgn-extract-300_ program, is a _command-line_ program for searching,
manipulating and formatting chess games recorded in the Portable Game Notation (PGN) or
something close. It is capable of handling files containing millions of games.
It also recognises Chess960 encodings.

Unlike the original, this fork is only guarenteed to compile and run under Linux,
but pull requests are welcome.

This program is made available under the terms of the
[GNU General Public License (Version 3).](https://www.cs.kent.ac.uk/~djb/pgn-extract/COPYING)

## Overview

The program is designed to make it easy to extract and format selected games from a
PGN format data file based on a wide variety of criteria.
The criteria include:

- textual move sequences;

- the position reached after a sequence of moves;

- information in the tag fields;

- fuzzy board position;

- and material balance in the ending.


Over the on-going 20+ year course of its development, it has also added
lots of features for controlling what is output (e.g., different
algebraic formats, EPD, no move numbers, restricting game length, etc.)

The program includes a semantic analyser which will
report errors in game scores and it is also able to detect duplicate
games found in its input files.

The range of input move formats accepted is fairly wide.
The output is normally in English Standard
Algebraic Notation (SAN) but this can be varied to long-algebraic or UCI,
for instance.

Extracted games may be written out either including or excluding
comments, NAGs, variations, move numbers, tags and/or results.
Games may be given ECO classifications
derived from the accompanying file eco.pgn, or a customised version
provided by the user.

The program is designed to be relatively memory-friendly, so it
does not retain a game's moves in memory once it has been processed.
This also makes it suitable for bulk processing very large collections of games
\- it can efficiently process files containing several millions of games.

Use the _--help_ argument to the program to
get the full lists of arguments.


## More information

The original [usage documentation](https://www.cs.kent.ac.uk/~djb/pgn-extract/help.html).

The original [eco.pgn](https://www.cs.kent.ac.uk/~djb/pgn-extract/eco.pgn) file for ECO classification.

## Blog post about data mining with pgn-extract

In October 2018 David wrote a [blog post](http://blogs.kent.ac.uk/djb/2018/10/14/data-mining-with-pgn-extract/)
about using pgn-extract to mine a PGN database.
As an example it looks at the effect of having a bishop pair versus a knight pair.

## Answers on Chess StackExchange using pgn-extract

David is active on [Chess StackExchange](https://chess.stackexchange.com/) as
[kentdjb](https://chess.stackexchange.com/users/12951/kentdjb)
and aims to respond to pgn-extract related questions, although email to him is my preferred way to raise
potential issues with the program.

From time to time, he provideus answers to questions that involve
the use of pgn-extract for analysis tasks:

- [Three-fold repetition](https://chess.stackexchange.com/questions/23938/is-there-a-record-for-threefold-repetition-for-when-the-claimed-positions-are-th/24276#24276).


- [Quadrupled pawns](https://chess.stackexchange.com/questions/24166/what-professional-game-has-the-quickest-sequence-starting-from-move-one-to-qua/24274#24274).


- [Frequency of occurrence of castling](https://chess.stackexchange.com/questions/24245/how-often-does-castling-occur-in-grandmaster-games/24247#24247).


* * *

© 1994-2024 David J. Barnes

[David's home page](https://www.cs.kent.ac.uk/~djb/) at the University of Kent.

David's email: [d.j.barnes @ kent.ac.uk](mailto:d.j.barnes @ kent.ac.uk)

David's Twitter: [@kentdjb](https://x.com/kentdjb)
