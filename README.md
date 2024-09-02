# pgn-extract

A Portable Game Notation (PGN) Manipulator for Chess Games

Version 24-10 by [David J. Barnes](https://www.cs.kent.ac.uk/~djb/) ( [@kentdjb](https://twitter.com/kentdjb))  [![RSS feed image](feed.png)](pgn-extract.xml)

## Introduction

This is the home page for the _pgn-extract_ program,
which is a _command-line_ program for searching, manipulating and formatting
chess games recorded in the Portable Game Notation (PGN) or something close. It
is capable of handling files containing millions of games. It also recognises Chess960
encodings.

A [full description of pgn-extract's functionality](https://www.cs.kent.ac.uk/people/staff/djb/pgn-extract/help.html) is available and included
with the sources.

Here you can find the C source code and Windows binaries for the current
version.
pgn-extract compiles and runs under Windows, Linux and Mac OS X.
This program is made available under the terms of the
[GNU General Public License (Version 3).](https://www.cs.kent.ac.uk/~djb/pgn-extract/COPYING)

## Getting-started video for Windows users

For Windows users who are really only interested in getting
the binary working, there is [a short introductory video](https://www.cs.kent.ac.uk/~djb/pgn-extract/intro.mp4).

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

## New in recent versions

These are the main changes in the most recent version:

- A bug fix for -n when used with tag matching.


- A bug fix for tag matching with relational operators.


- A bug fix for position matching via -z. Games with FEN tags in which black
was to move had colours swapped when matching.


- New options --commented, --insufficient and --suppressmatched.


- Added stdout as an output for -n.


- EloDiff pseudo-tag added.


- Relational operator available for non-numeric tag values.


## Available Files

You can take a copy of the full source and
documentation as either
[pgn-extract-24-10.tgz](https://www.cs.kent.ac.uk/~djb/pgn-extract/pgn-extract-24-10.tgz)
or
[pgn-extract-24-10.zip](https://www.cs.kent.ac.uk/~djb/pgn-extract/pgn-extract-24-10.zip).
Alternatively, a Windows
[64-bit binary](https://www.cs.kent.ac.uk/~djb/pgn-extract/pgn-extract.exe) is also available.

**Name****Description****Size****Date**[pgn-extract-24-10.tgz](https://www.cs.kent.ac.uk/~djb/pgn-extract/pgn-extract-24-10.tgz)

GZipped tar file
of the complete source of the latest version of the program.

Includes [usage documentation](https://www.cs.kent.ac.uk/~djb/pgn-extract/help.html),
Makefile for compilation and
[eco.pgn](https://www.cs.kent.ac.uk/~djb/pgn-extract/eco.pgn) file for ECO classification.459K bytes  12 Jun 2024[pgn-extract-24-10.zip](https://www.cs.kent.ac.uk/~djb/pgn-extract/pgn-extract-24-10.zip)Zipped file of the complete source of the latest version of the program.

Includes [usage documentation](https://www.cs.kent.ac.uk/~djb/pgn-extract/help.html), Makefile for compilation and
[eco.pgn](https://www.cs.kent.ac.uk/~djb/pgn-extract/eco.pgn) file for ECO classification.602K bytes  12 Jun 2024[pgn-extract.exe](https://www.cs.kent.ac.uk/~djb/pgn-extract/pgn-extract.exe)Windows 64-bit binary of the latest version of the program.2.3M bytes  12 Jun 2024[eco.zip](https://www.cs.kent.ac.uk/~djb/pgn-extract/eco.zip)Zipped version of [eco.pgn](https://www.cs.kent.ac.uk/~djb/pgn-extract/eco.pgn).32K bytes  [eco.pgn](https://www.cs.kent.ac.uk/~djb/pgn-extract/eco.pgn)File of openings with PGN classification.

This file is already included in the source archives.
254K bytes  [COPYING](https://www.cs.kent.ac.uk/~djb/pgn-extract/COPYING)GNU General Public License (version 3).35K bytes

## Blog post about data mining with pgn-extract

In October 2018 I wrote [blog post](http://blogs.kent.ac.uk/djb/2018/10/14/data-mining-with-pgn-extract/)
about using pgn-extract to mine a PGN database.
As an example it looks at the effect of having a bishop pair versus a knight pair.

## Answers on Chess StackExchange using pgn-extract

I am active on [Chess StackExchange](https://chess.stackexchange.com/) as
[kentdjb](https://chess.stackexchange.com/users/12951/kentdjb)
and aim to respond to pgn-extract related questions, although email to me is my preferred way to raise
potential issues with the program.

From time to time, I have provided answers to questions that involve
the use of pgn-extract for analysis tasks:

- [Three-fold repetition](https://chess.stackexchange.com/questions/23938/is-there-a-record-for-threefold-repetition-for-when-the-claimed-positions-are-th/24276#24276).


- [Quadrupled pawns](https://chess.stackexchange.com/questions/24166/what-professional-game-has-the-quickest-sequence-starting-from-move-one-to-qua/24274#24274).


- [Frequency of occurrence of castling](https://chess.stackexchange.com/questions/24245/how-often-does-castling-occur-in-grandmaster-games/24247#24247).


## Feedback

Feedback and suggestions for further features are always welcome, although I can't always
promise to undertake significant development work.

* * *

© 1994-2024 David J. Barnes

[My home page](https://www.cs.kent.ac.uk/~djb/) at the University of Kent.

[d.j.barnes @ kent.ac.uk](mailto:d.j.barnes @ kent.ac.uk)

[@kentdjb](https://x.com/kentdjb)

Last updated: 12th June 2024: version 24-10 released.

