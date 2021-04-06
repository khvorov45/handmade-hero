use crate::game;
use std::num::Wrapping;

pub trait Buffer {
    fn get_pixels(&self) -> &[u32];
    fn get_mut_pixels(&mut self) -> &mut [u32];
    fn get_height(&self) -> u32;
    fn get_width(&self) -> u32;
}

pub fn render_weird_gradient<B: Buffer>(buffer: &mut B, state: &mut game::State) {
    let height = buffer.get_height();
    let width = buffer.get_width();
    let memory = buffer.get_mut_pixels();

    let mut pixel = 0;
    let mut y = 0;
    while y < height {
        let mut x = 0;
        while x < width {
            let blue = Wrapping(x as u8) + state.blue_offset;
            let green = Wrapping(y as u8) + state.green_offset;
            memory[pixel] = ((green.0 as u32) << 8) | blue.0 as u32;
            x += 1;
            pixel += 1;
        }
        y += 1;
    }
}
