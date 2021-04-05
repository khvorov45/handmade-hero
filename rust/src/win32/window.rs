use crate::Result;
use anyhow::bail;
use winapi::shared::{
    minwindef::{LPARAM, LRESULT, UINT, WPARAM},
    windef::{HDC, HWND},
};
use winapi::um::winuser::GetDC;

use crate::win32::util::ToWide;

pub fn create_handle() -> Result<HWND> {
    use std::ptr::null_mut;
    use winapi::shared::windef::HBRUSH;
    use winapi::um::libloaderapi::GetModuleHandleW;
    use winapi::um::winuser::{
        CreateWindowExW, RegisterClassExW, COLOR_BACKGROUND, CS_HREDRAW, CS_OWNDC, CS_VREDRAW,
        CW_USEDEFAULT, WNDCLASSEXW, WS_OVERLAPPEDWINDOW, WS_VISIBLE,
    };

    let instance = unsafe { GetModuleHandleW(null_mut()) };

    let class_name = "HandmadeRustClassName".to_wide_null();

    let wnd_class = WNDCLASSEXW {
        cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
        style: CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
        lpfnWndProc: Some(window_proc),
        cbClsExtra: 0,
        cbWndExtra: 0,
        hInstance: instance,
        hIcon: null_mut(),
        hCursor: null_mut(),
        hbrBackground: COLOR_BACKGROUND as HBRUSH,
        lpszMenuName: null_mut(),
        lpszClassName: class_name.as_ptr(),
        hIconSm: null_mut(),
    };

    if unsafe { RegisterClassExW(&wnd_class) == 0 } {
        bail!("window class registration failed");
    };

    let window = unsafe {
        CreateWindowExW(
            0,                                       // dwExStyle
            class_name.as_ptr(),                     // lpClassName
            "Handmade Rust".to_wide_null().as_ptr(), // lpWindowName
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,        // dwStyle
            CW_USEDEFAULT,                           // Int x
            CW_USEDEFAULT,                           // Int y
            CW_USEDEFAULT,                           // Int nWidth
            CW_USEDEFAULT,                           // Int nHeight
            null_mut(),                              // hWndParent
            null_mut(),                              // hMenu
            instance,                                // hInstance
            null_mut(),                              // lpParam
        )
    };

    if window.is_null() {
        bail!("Window Creation Failed!");
    }

    Ok(window)
}

pub fn create_device_context(handle: HWND) -> HDC {
    unsafe { GetDC(handle) }
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub struct Dimensions {
    pub width: u32,
    pub height: u32,
}

extern "system" fn window_proc(hwnd: HWND, msg: UINT, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    use winapi::um::winuser::{DefWindowProcW, PostQuitMessage, WM_CLOSE, WM_DESTROY};
    unsafe {
        match msg {
            WM_CLOSE | WM_DESTROY => {
                PostQuitMessage(0);
            }
            _ => return DefWindowProcW(hwnd, msg, wparam, lparam),
        }
    }
    0
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum Action {
    Quit,
    None,
}

pub fn get_dimensions(window: HWND) -> Result<Dimensions> {
    use winapi::shared::windef::RECT;
    use winapi::um::winuser::GetClientRect;
    let mut client_rect = RECT {
        bottom: 0,
        left: 0,
        right: 0,
        top: 0,
    };
    let res = unsafe { GetClientRect(window, &mut client_rect) };
    if res == 0 {
        bail!("failed to get rectangle");
    }
    Ok(Dimensions {
        width: (client_rect.right - client_rect.left) as u32,
        height: (client_rect.bottom - client_rect.top) as u32,
    })
}

pub fn process_messages() -> Action {
    use std::ptr::null_mut;
    use winapi::um::winuser::{
        DispatchMessageW, PeekMessageW, TranslateMessage, MSG, PM_REMOVE, WM_QUIT,
    };
    let mut msg = std::mem::MaybeUninit::<MSG>::uninit();
    unsafe {
        while PeekMessageW(msg.as_mut_ptr(), null_mut(), 0, 0, PM_REMOVE) != 0 {
            if msg.assume_init().message == WM_QUIT {
                return Action::Quit;
            }
            TranslateMessage(msg.as_ptr());
            DispatchMessageW(msg.as_ptr());
        }
    };
    Action::None
}
