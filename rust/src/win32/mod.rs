use crate::{game, Result};

mod graphics;
mod util;
mod window;

pub fn run() -> Result<()> {
    let (internal_width, intenal_height) = (1280, 720);
    let (mut window_width, mut window_height) = (1280, 720);

    let window = window::create_handle(window_width, window_height)?;
    let device_context = window::create_device_context(window);

    let mut graphics_buffer = graphics::Buffer::new(internal_width, intenal_height);

    loop {
        match window::process_messages() {
            window::Action::Quit => break,
            window::Action::Resize(new_width, new_height) => {
                window_width = new_width;
                window_height = new_height;
            }
            window::Action::None => {}
        }

        game::update_and_render(&mut graphics_buffer);

        graphics_buffer.display(device_context, window_width, window_height);
    }
    Ok(())
}
