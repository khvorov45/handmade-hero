pub mod graphics;
pub mod input;

pub fn update_and_render<G: graphics::Buffer>(buffer: &mut G, input: &input::Controller) {
    let green_offset =
        input.buttons.up.ended_down as i8 * 50 + input.buttons.down.ended_down as i8 * (-50);
    let blue_offset =
        input.buttons.left.ended_down as i8 * 50 + input.buttons.right.ended_down as i8 * (-50);
    graphics::render_weird_gradient(buffer, blue_offset, green_offset);
}
