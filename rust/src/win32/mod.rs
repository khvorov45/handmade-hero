use crate::{game, Result};

mod graphics;
mod util;
mod window;

pub fn run() -> Result<()> {
    let window = window::create_handle()?;
    let device_context = window::create_device_context(window);

    let mut graphics_buffer = graphics::Buffer::new(1280, 720);

    loop {
        match window::process_messages() {
            window::Action::Quit => break,
            window::Action::None => {}
        }

        game::update_and_render(&mut graphics_buffer);

        // TODO: don't calculate dimensions every frame
        let dimensions = window::get_dimensions(window)?;
        graphics_buffer.display(device_context, dimensions.width, dimensions.height);
    }
    Ok(())
}
