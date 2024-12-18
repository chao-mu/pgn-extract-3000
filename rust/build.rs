extern crate bindgen;
extern crate cmake;

use cmake::Config;
use std::{env, path::PathBuf};

fn main() {
    // Run cmake to build nng
    let dst = Config::new("..")
        .generator("Ninja")
        .define("CMAKE_BUILD_TYPE", "Release")
        .build();

    // Check output of `cargo build --verbose`, should see something like:
    // -L native=/path/runng/target/debug/build/runng-sys-abc1234/out
    // That contains output from cmake
    println!("cargo:rustc-link-search=native={}", dst.display());
    // Tell rustc to use nng static library
    println!("cargo:rustc-link-lib=static=pgne");

    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        // This is needed if use `#include <nng.h>` instead of `#include "path/nng.h"`
        //.clang_arg("-Inng/src/")
        .generate()
        .expect("Unable to generate bindings");
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings");
}
