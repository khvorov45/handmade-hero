use crate::game;

pub trait AcceptKeyboardKey {
    fn accept_keyboard_key(&mut self, key: u32);
}

impl AcceptKeyboardKey for game::input::Controller {
    fn accept_keyboard_key(&mut self, key: u32) {
        use winapi::um::winuser::{VK_DOWN, VK_LEFT, VK_RIGHT, VK_UP};
        match key as i32 {
            VK_DOWN => self.buttons.down.ended_down = !self.buttons.down.ended_down,
            VK_LEFT => self.buttons.left.ended_down = !self.buttons.left.ended_down,
            VK_RIGHT => self.buttons.right.ended_down = !self.buttons.right.ended_down,
            VK_UP => self.buttons.up.ended_down = !self.buttons.up.ended_down,
            _ => {}
        }
    }
}
