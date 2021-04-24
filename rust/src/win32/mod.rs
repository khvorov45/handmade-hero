use crate::{game, Result};

mod graphics;
mod input;
mod sound;
mod util;
mod window;

use input::AcceptKeyboardKey;

pub fn run() -> Result<()> {
    let (internal_width, intenal_height) = (1280, 720);
    let (window_width, window_height) = (internal_width, intenal_height);

    let window = window::create_handle(window_width, window_height)?;
    let device_context = window::create_device_context(window);

    let mut graphics_buffer = graphics::Buffer::new(internal_width, intenal_height);
    let mut sound_buffer = sound::Buffer::new(window)?;
    let mut new_input = game::input::Controller::default();
    let mut game_state = game::State::default();

    sound_buffer.play()?;

    loop {
        match window::process_messages() {
            window::Action::Quit => break,
            window::Action::Resize(_, _) => {}
            window::Action::Keyboard(key) => new_input.accept_keyboard_key(key),
            window::Action::None => {}
        }

        game::update_and_render(
            &mut game_state,
            &mut graphics_buffer,
            &mut sound_buffer,
            &new_input,
        )?;

        graphics_buffer.display(device_context);
        sound_buffer.play_new_samples()?;
    }
    Ok(())
}
