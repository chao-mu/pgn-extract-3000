#![warn(clippy::all, clippy::pedantic)]

use clap::{arg, Parser};
use std::{path::PathBuf, process::exit};

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
    let args = Args::parse();

    let _ = CString::new(args.input.as_os_str().as_encoded_bytes()).unwrap();
}
