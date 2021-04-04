pub mod graphics;

pub fn update_and_render<G: graphics::Buffer>(buffer: &mut G) {
    graphics::render_weird_gradient(buffer);
}
