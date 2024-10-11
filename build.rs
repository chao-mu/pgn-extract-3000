use cmake::build;

fn main() {
	let dst = build("libpgne");

	println!("cargo:rustc-link-search=native={}", dst.display());
	println!("cargo:rustc-link-lib=static=pgne");
}
