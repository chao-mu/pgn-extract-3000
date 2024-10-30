#![warn(clippy::all, clippy::pedantic)]

use std::{path::PathBuf, ptr::null_mut};

use clap::{arg, Parser};

pub mod bindings;

use bindings::StateInfo;

#[derive(Parser, Debug)]
struct Args {
    #[arg(long, default_value_t = 0)]
    minply: u32,
    #[arg(long)]
    nobadresults: bool,
    #[arg(long)]
    fixresulttags: bool,
    input: PathBuf,
}

fn main() {
    let _ = Args::parse();

    let default_position_match_comment =
        std::ffi::CString::new(bindings::DEFAULT_POSITION_MATCH_COMMENT).unwrap();

    let default_eco_file = std::ffi::CString::new(bindings::DEFAULT_ECO_FILE).unwrap();

    let globals = StateInfo {
        skipping_current_game: false,                           /*  */
        check_only: false,                                      /*  (-r) */
        verbosity: 2,                                           /*  level (-s and --quiet) */
        keep_NAGs: true,                                        /*  (-N) */
        keep_comments: true,                                    /*  (-C) */
        keep_variations: true,                                  /*  (-V) */
        tag_output_format: bindings::TagOutputForm_ALL_TAGS,    /*  (-7, --notags) */
        match_permutations: true,                               /*  (-v) */
        positional_variations: false,                           /*  (-x) */
        use_soundex: false,                                     /*  (-S) */
        suppress_duplicates: false,                             /*  (-D) */
        suppress_originals: false,                              /*  (-U) */
        fuzzy_match_duplicates: false,                          /*  (--fuzzy) */
        fuzzy_match_depth: 0,                                   /*  (--fuzzy) */
        check_tags: false,                                      /*  */
        add_ECO: false,                                         /*  (-e) */
        parsing_ECO_file: false,                                /*  (-e) */
        ECO_level: bindings::EcoDivision_DONT_DIVIDE,           /*  (-E) */
        output_format: bindings::OutputFormat_SAN,              /*  (-W) */
        max_line_length: bindings::MAX_LINE_LENGTH,             /*  (-w) */
        use_virtual_hash_table: false,                          /*  (-Z) */
        check_move_bounds: false,                               /*  (-b) */
        match_only_checkmate: false,                            /*  (-M) */
        match_only_stalemate: false,                            /*  (--stalemate) */
        match_only_insufficient_material: false,                /*  (--insufficient) */
        keep_move_numbers: true,                                /*  (--nomovenumbers) */
        keep_results: true,                                     /*  (--noresults) */
        keep_checks: true,                                      /*  (--nochecks) */
        output_evaluation: false,                               /*  (--evaluation) */
        keep_broken_games: false,                               /*  (--keepbroken) */
        suppress_redundant_ep_info: false,                      /*  (--nofauxep) */
        json_format: false,                                     /*  (--json) */
        tsv_format: false,                                      /*  (--tsv) */
        tag_match_anywhere: false,                              /*  (--tagsubstr) */
        match_underpromotion: false,                            /*  (--underpromotion) */
        suppress_matched: false,                                /*  (--suppressmatched) */
        depth_of_positional_search: 0,                          /*  */
        num_games_processed: 0,                                 /*  */
        num_games_matched: 0,                                   /*  */
        num_non_matching_games: 0,                              /*  */
        games_per_file: 0,                                      /*  (-#) */
        next_file_number: 1,                                    /*  */
        lower_move_bound: 0,                                    /*  */
        upper_move_bound: 10000,                                /*  */
        output_ply_limit: -1,                                   /*  (--plylimit) */
        quiescence_threshold: 0,                                /*  (--stable) */
        first_game_number: 1,                                   /*  */
        game_limit: 0,                                          /*  */
        maximum_matches: 0,                                     /*  */
        drop_ply_number: 0,                                     /*  (--dropply) */
        startply: 1,                                            /*  (--startply) */
        check_for_repetition: 0,                                /*  (--repetition) */
        check_for_N_move_rule: 0,                               /*  (--fifty, --seventyfive) */
        output_FEN_string: false,                               /*  */
        add_FEN_comments: false,                                /*  (--fencomments) */
        add_hashcode_comments: false,                           /*  (--hashcomments) */
        add_position_match_comments: false,                     /*  (--markmatches) */
        output_plycount: false,                                 /*  (--plycount) */
        output_total_plycount: false,                           /*  (--totalplycount) */
        add_hashcode_tag: false,                                /*  (--addhashcode) */
        fix_result_tags: false,                                 /*  (--fixresulttags) */
        fix_tag_strings: false,                                 /*  (--fixtagstrings) */
        add_fen_castling: false,                                /*  (--addfencastling) */
        separate_comment_lines: false,                          /*  (--commentlines) */
        split_variants: false,                                  /*  (--separatevariants) */
        reject_inconsistent_results: false,                     /*  (--nobadresults) */
        allow_null_moves: false,                                /*  (--allownullmoves) */
        allow_nested_comments: false,                           /*  (--nestedcomments) */
        add_match_tag: false,                                   /*  (--addmatchtag) */
        add_matchlabel_tag: false,                              /*  (--addlabeltag) */
        only_output_wanted_tags: false,                         /*  (--xroster) */
        delete_same_setup: false,                               /*  (--deletesamesetup) */
        lichess_comment_fix: false,                             /*  (--lichesscommentfix) */
        keep_only_commented_games: false,                       /*  (--only_commented_games) */
        split_depth_limit: 0,                                   /*  */
        current_file_type: bindings::SourceFileType_NORMALFILE, /*  */
        setup_status: bindings::SetupOutputStatus_SETUP_TAG_OK, /*  */
        whose_move: bindings::WhoseMove_EITHER_TO_MOVE,         /*  */
        position_match_comment: default_position_match_comment.as_ptr(),
        FEN_comment_pattern: null_mut(),        /*  (-Fpattern) */
        drop_comment_pattern: null_mut(),       /*  (--dropbefore) */
        line_number_marker: null_mut(),         /*  (--linenumbers) */
        current_input_file: null_mut(),         /*  */
        eco_file: default_eco_file.as_ptr(),    /*  (-e) */
        outputfile: null_mut(),                 /*  (-o, -a). Default is stdout */
        output_filename: null_mut(),            /*  (-o, -a) */
        logfile: null_mut(),                    /*  (-l). Default is stderr */
        duplicate_file: null_mut(),             /*  (-d) */
        non_matching_file: null_mut(),          /*  (-n) */
        matching_game_numbers: null_mut(),      /*  */
        next_game_number_to_output: null_mut(), /*  */
        skip_game_numbers: null_mut(),          /*  */
        next_game_number_to_skip: null_mut(),   /*  */
    };
}
