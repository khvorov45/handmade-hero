pub trait Buffer {
    fn get_pixels(&self) -> &[u32];
    fn get_mut_pixels(&mut self) -> &mut [u32];
    fn get_height(&self) -> u32;
    fn get_width(&self) -> u32;
}

pub fn render_weird_gradient<B: Buffer>(buffer: &mut B) {
    let height = buffer.get_height();
    let width = buffer.get_width();
    let memory = buffer.get_mut_pixels();

    let mut pixel = 0;
    let mut y = 0;
    while y < height {
        let mut x = 0;
        while x < width {
            let blue = x as u8;
            let green = y as u8;
            memory[pixel] = ((green as u32) << 8) | blue as u32;
            x += 1;
            pixel += 1;
        }
        y += 1;
    }
}
