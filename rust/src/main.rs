use handmade_rust::{win32, Result};

#[cfg(windows)]
fn main() -> Result<()> {
    win32::run()?;
    Ok(())
}

#[cfg(not(windows))]
fn main() {
    println!("Only works on windows!");
}
