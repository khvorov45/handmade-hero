use crate::game;
use crate::win32::bindings::Windows::Win32::Gdi;

pub struct Buffer {
    info: Gdi::BITMAPINFO,
    pixels: Vec<u32>,
    width: u32,
    height: u32,
}

impl Buffer {
    pub fn new(width: u32, height: u32) -> Self {
        let bmi_header = Gdi::BITMAPINFOHEADER {
            biBitCount: 32,
            biCompression: Gdi::BI_RGB as u32,
            biHeight: -(height as i32), //* Negative makes the window top-down
            biSize: std::mem::size_of::<Gdi::BITMAPINFOHEADER>() as u32,
            biWidth: width as i32,
            biPlanes: 1,
            biXPelsPerMeter: 0,
            biYPelsPerMeter: 0,
            biClrUsed: 0,
            biClrImportant: 0,
            biSizeImage: 0,
        };

        let bmi_colors = Gdi::RGBQUAD {
            rgbBlue: 0,
            rgbRed: 0,
            rgbGreen: 0,
            rgbReserved: 0,
        };

        let info = Gdi::BITMAPINFO {
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
    pub fn display(&self, device_context: Gdi::HDC) {
        use std::ffi::c_void;

        unsafe {
            Gdi::StretchDIBits(
                device_context,
                0, // XDest
                0, // YDest
                self.width as i32,
                self.height as i32,
                0, // XSrc
                0, // YSrc
                self.width as i32,
                self.height as i32,
                self.pixels.as_ptr() as *mut c_void,
                &self.info,
                Gdi::DIB_USAGE::DIB_RGB_COLORS,
                Gdi::ROP_CODE::SRCCOPY,
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
