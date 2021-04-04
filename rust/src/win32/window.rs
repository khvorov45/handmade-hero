use crate::Result;
use anyhow::bail;
use winapi::shared::{
    minwindef::{LPARAM, LRESULT, UINT, WPARAM},
    windef::HWND,
};

use crate::win32::util::ToWide;

pub fn create() -> Result<HWND> {
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

#[derive(Debug, PartialEq)]
pub enum ShouldQuit {
    Yes,
    No,
}

pub fn process_messages() -> ShouldQuit {
    use std::ptr::null_mut;
    use winapi::um::winuser::{
        DispatchMessageW, PeekMessageW, TranslateMessage, MSG, PM_REMOVE, WM_QUIT,
    };
    let mut msg = std::mem::MaybeUninit::<MSG>::uninit();
    unsafe {
        while PeekMessageW(msg.as_mut_ptr(), null_mut(), 0, 0, PM_REMOVE) != 0 {
            if msg.assume_init().message == WM_QUIT {
                return ShouldQuit::Yes;
            }
            TranslateMessage(msg.as_ptr());
            DispatchMessageW(msg.as_ptr());
        }
    };
    ShouldQuit::No
}
