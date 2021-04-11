use crate::Result;
use std::num::Wrapping;

pub mod graphics;
pub mod input;
pub mod sound;

#[derive(Debug)]
pub struct State {
    speed: i8,
    green_offset: Wrapping<u8>,
    blue_offset: Wrapping<u8>,
    tone_hz: u32,
    volume: u32,
    t_sine: f32,
}

impl Default for State {
    fn default() -> Self {
        Self {
            speed: 10,
            green_offset: Wrapping(0),
            blue_offset: Wrapping(0),
            tone_hz: 256,
            volume: 2000,
            t_sine: 0f32,
        }
    }
}

impl State {
    pub fn simulate(&mut self, input: &input::Controller) {
        let vertical = input.buttons.down.ended_down as i8 - input.buttons.up.ended_down as i8;
        let horizontal = input.buttons.right.ended_down as i8 - input.buttons.left.ended_down as i8;
        self.green_offset += Wrapping((self.speed * vertical) as u8);
        self.blue_offset += Wrapping((self.speed * horizontal) as u8);
        self.tone_hz = (256 + horizontal as i32 * 100) as u32;
        self.volume = (2000 + vertical as i32 * 1000) as u32;
    }
}

pub fn update_and_render<G: graphics::Buffer, S: sound::Buffer>(
    state: &mut State,
    graphics_buffer: &mut G,
    sound_buffer: &mut S,
    input: &input::Controller,
) -> Result<()> {
    state.simulate(input);
    graphics::render_weird_gradient(graphics_buffer, state);
    sound::play_sinewave(sound_buffer, state)?;
    Ok(())
}
