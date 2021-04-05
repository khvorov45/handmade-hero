use crate::Result;
use anyhow::bail;
use winapi::shared::{
    minwindef::{LPARAM, LRESULT, UINT, WPARAM},
    windef::{HDC, HWND},
};
use winapi::um::winuser::GetDC;

use crate::win32::util::ToWide;

pub fn create_handle(width: u32, height: u32) -> Result<HWND> {
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
            width as i32,                            // Int nWidth
            height as i32,                           // Int nHeight
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

/// Do not process any messages here, process in `process_messages` instead.
/// Some messages don't enter the queue and thus are not picked up by `process_messages`
/// (e.g. WM_SIZE), hence the need to post them to the queue.
extern "system" fn window_proc(hwnd: HWND, msg: UINT, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    use winapi::um::winuser::{
        DefWindowProcW, PostMessageW, PostQuitMessage, WM_CLOSE, WM_DESTROY, WM_EXITSIZEMOVE,
        WM_SIZE,
    };
    static mut LAST_SIZE_LPARAM: LPARAM = 0; // Stupid BS
    unsafe {
        match msg {
            WM_CLOSE | WM_DESTROY => {
                PostQuitMessage(0);
            }
            //* Resize handling.
            //* Note that exit size is sent after the last size, so need to save sizes
            WM_SIZE => LAST_SIZE_LPARAM = lparam,
            WM_EXITSIZEMOVE => {
                PostMessageW(hwnd, WM_SIZE, 0, LAST_SIZE_LPARAM);
            }
            _ => return DefWindowProcW(hwnd, msg, wparam, lparam),
        }
    }
    0
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum Action {
    Quit,
    Resize(u32, u32),
    None,
}

pub fn process_messages() -> Action {
    use std::ptr::null_mut;
    use winapi::shared::minwindef::{HIWORD, LOWORD};
    use winapi::um::winuser::{
        DispatchMessageW, PeekMessageW, TranslateMessage, MSG, PM_REMOVE, WM_QUIT, WM_SIZE,
    };
    let mut msg = std::mem::MaybeUninit::<MSG>::uninit();
    unsafe {
        while PeekMessageW(msg.as_mut_ptr(), null_mut(), 0, 0, PM_REMOVE) != 0 {
            let msg_value = msg.assume_init();
            match msg_value.message {
                WM_QUIT => return Action::Quit,
                WM_SIZE => {
                    let new_size = msg_value.lParam as u32;
                    return Action::Resize(LOWORD(new_size) as u32, HIWORD(new_size) as u32);
                }
                _ => {
                    TranslateMessage(msg.as_ptr());
                    DispatchMessageW(msg.as_ptr());
                }
            }
        }
    };
    Action::None
}
