use crate::{game, win32::bindings::Windows::Win32::WindowsAndMessaging};

pub trait AcceptKeyboardKey {
    fn accept_keyboard_key(&mut self, key: u32);
}

impl AcceptKeyboardKey for game::input::Controller {
    fn accept_keyboard_key(&mut self, key: u32) {
        match key {
            WindowsAndMessaging::VK_DOWN => {
                self.buttons.down.ended_down = !self.buttons.down.ended_down
            }
            WindowsAndMessaging::VK_LEFT => {
                self.buttons.left.ended_down = !self.buttons.left.ended_down
            }
            WindowsAndMessaging::VK_RIGHT => {
                self.buttons.right.ended_down = !self.buttons.right.ended_down
            }
            WindowsAndMessaging::VK_UP => self.buttons.up.ended_down = !self.buttons.up.ended_down,
            _ => {}
        }
    }
}
