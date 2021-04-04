use crate::{game, Result};

mod graphics;
mod util;
mod window;

pub fn run() -> Result<()> {
    let window = window::Window::new()?;

    let mut graphics_buffer = graphics::Buffer::new(1280, 720);

    loop {
        if window::process_messages() == window::ShouldQuit::Yes {
            break;
        };

        game::graphics::render_weird_gradient(&mut graphics_buffer);

        let window_dimensions = window.get_dimensions()?;
        graphics_buffer.display(
            window.get_device_context(),
            window_dimensions.width,
            window_dimensions.height,
        );
    }
    Ok(())
}
