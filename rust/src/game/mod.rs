use crate::Result;
use std::num::Wrapping;

pub mod graphics;
pub mod input;
pub mod sound;

#[derive(Debug)]
pub struct State {
    green_offset: Wrapping<u8>,
    blue_offset: Wrapping<u8>,
    tone_hz: u32,
    volume: u32,
    t_sine: f32,
}

impl Default for State {
    fn default() -> Self {
        Self {
            green_offset: Wrapping(0),
            blue_offset: Wrapping(0),
            tone_hz: 256,
            volume: 2000,
            t_sine: 0f32,
        }
    }
}

pub fn update_and_render<G: graphics::Buffer, S: sound::Buffer>(
    state: &mut State,
    graphics_buffer: &mut G,
    sound_buffer: &mut S,
    input: &input::Controller,
) -> Result<()> {
    let speed: i8 = 10;
    state.green_offset += Wrapping(
        (speed * (input.buttons.down.ended_down as i8 - input.buttons.up.ended_down as i8)) as u8,
    );
    state.blue_offset += Wrapping(
        (speed * (input.buttons.right.ended_down as i8 - input.buttons.left.ended_down as i8))
            as u8,
    );
    graphics::render_weird_gradient(graphics_buffer, state);
    sound::play_sinewave(sound_buffer, state)?;
    Ok(())
}
