use crate::game;
use winapi::shared::windef::HDC;
use winapi::um::wingdi::{StretchDIBits, BITMAPINFO};

pub struct Buffer {
    info: BITMAPINFO,
    pixels: Vec<u32>,
    width: u32,
    height: u32,
}

impl Buffer {
    pub fn new(width: u32, height: u32) -> Self {
        use winapi::um::wingdi::{BITMAPINFOHEADER, BI_RGB, RGBQUAD};

        let bmi_header = BITMAPINFOHEADER {
            biBitCount: 32,
            biCompression: BI_RGB,
            biHeight: -(height as i32), //* Negative makes the window top-down
            biSize: std::mem::size_of::<BITMAPINFOHEADER>() as u32,
            biWidth: width as i32,
            biPlanes: 1,
            biXPelsPerMeter: 0,
            biYPelsPerMeter: 0,
            biClrUsed: 0,
            biClrImportant: 0,
            biSizeImage: 0,
        };

        let bmi_colors = RGBQUAD {
            rgbBlue: 0,
            rgbRed: 0,
            rgbGreen: 0,
            rgbReserved: 0,
        };

        let info = BITMAPINFO {
            bmiHeader: bmi_header,
            bmiColors: [bmi_colors],
        };

        Self {
            info,
            pixels: vec![0; (width * height) as usize],
            width,
            height,
        }
    }
    pub fn display(&self, device_context: HDC, window_width: u32, window_height: u32) {
        use winapi::ctypes::c_void;
        use winapi::um::wingdi::{DIB_RGB_COLORS, SRCCOPY};
        unsafe {
            StretchDIBits(
                device_context,
                0, // XDest
                0, // YDest
                window_width as i32,
                window_height as i32,
                0, // XSrc
                0, // YSrc
                self.width as i32,
                self.height as i32,
                self.pixels.as_ptr() as *mut c_void,
                &self.info,
                DIB_RGB_COLORS,
                SRCCOPY,
            );
        }
    }
}

impl game::graphics::Buffer for Buffer {
    fn get_pixels(&self) -> &[u32] {
        &self.pixels
    }
    fn get_mut_pixels(&mut self) -> &mut [u32] {
        &mut self.pixels
    }
    fn get_height(&self) -> u32 {
        self.height
    }
    fn get_width(&self) -> u32 {
        self.width
    }
}
