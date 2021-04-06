use std::num::Wrapping;

pub mod graphics;
pub mod input;

#[derive(Debug, Default)]
pub struct State {
    green_offset: Wrapping<i8>,
    blue_offset: Wrapping<i8>,
}

pub fn update_and_render<G: graphics::Buffer>(
    state: &mut State,
    buffer: &mut G,
    input: &input::Controller,
) {
    let speed = 10;
    state.green_offset +=
        Wrapping(speed * (input.buttons.down.ended_down as i8 - input.buttons.up.ended_down as i8));
    state.blue_offset += Wrapping(
        speed * (input.buttons.right.ended_down as i8 - input.buttons.left.ended_down as i8),
    );
    graphics::render_weird_gradient(buffer, state);
}
