use crate::Result;

mod util;
mod window;

pub fn run() -> Result<()> {
    window::create()?;
    loop {
        if window::process_messages() == window::ShouldQuit::Yes {
            break;
        };
    }
    Ok(())
}
