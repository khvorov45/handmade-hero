use crate::{
    win32::bindings::Windows::Win32::{
        Debug::GetLastError, Gdi, SystemServices, WindowsAndMessaging,
    },
    Result,
};
use anyhow::{bail, Context};

pub fn create_handle(width: u32, height: u32) -> Result<WindowsAndMessaging::HWND> {
    use crate::win32::bindings::Windows::Win32::MenusAndResources;
    use crate::win32::util::ToWide;
    use std::ptr::null_mut;
    use WindowsAndMessaging::WNDCLASS_STYLES;

    let get_module_handle_result =
        unsafe { SystemServices::GetModuleHandleW(SystemServices::PWSTR::NULL) };
    let instance = SystemServices::HINSTANCE(get_module_handle_result);
    if get_module_handle_result == 0 {
        windows::HRESULT::from_win32(unsafe { GetLastError() })
            .ok()
            .context("failed to get module handle")?;
    }

    let mut class_name = "HandmadeRustClassName".to_wide();

    let wnd_class = WindowsAndMessaging::WNDCLASSEXW {
        cbSize: std::mem::size_of::<WindowsAndMessaging::WNDCLASSEXW>() as u32,
        style: WNDCLASS_STYLES::CS_OWNDC
            | WNDCLASS_STYLES::CS_HREDRAW
            | WNDCLASS_STYLES::CS_VREDRAW,
        lpfnWndProc: Some(window_proc),
        cbClsExtra: 0,
        cbWndExtra: 0,
        hInstance: instance,
        hIcon: MenusAndResources::HICON::NULL,
        hCursor: MenusAndResources::HCURSOR::NULL,
        hbrBackground: Gdi::HBRUSH::NULL,
        lpszMenuName: SystemServices::PWSTR::NULL,
        lpszClassName: SystemServices::PWSTR(class_name.as_mut_ptr()),
        hIconSm: MenusAndResources::HICON::NULL,
    };

    if unsafe { WindowsAndMessaging::RegisterClassExW(&wnd_class) == 0 } {
        windows::HRESULT::from_win32(unsafe { GetLastError() })
            .ok()
            .context("window class registration failed")?
    };
    let window = unsafe {
        WindowsAndMessaging::CreateWindowExW(
            WindowsAndMessaging::WINDOW_EX_STYLE(0),
            SystemServices::PWSTR(class_name.as_mut_ptr()),
            "Handmade Rust",
            WindowsAndMessaging::WINDOW_STYLE(
                WindowsAndMessaging::WINDOW_STYLE::WS_OVERLAPPEDWINDOW.0
                    | WindowsAndMessaging::WINDOW_STYLE::WS_VISIBLE.0,
            ),
            WindowsAndMessaging::CW_USEDEFAULT,
            WindowsAndMessaging::CW_USEDEFAULT,
            width as i32,
            height as i32,
            WindowsAndMessaging::HWND::NULL,
            MenusAndResources::HMENU::NULL,
            instance,
            null_mut(),
        )
    };

    if window.is_null() {
        bail!("Window Creation Failed!");
    }

    Ok(window)
}

pub fn create_device_context(handle: WindowsAndMessaging::HWND) -> Gdi::HDC {
    unsafe { Gdi::GetDC(handle) }
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub struct Dimensions {
    pub width: u32,
    pub height: u32,
}

/// Do not process any messages here, process in `process_messages` instead.
/// Some messages don't enter the queue and thus are not picked up by `process_messages`
/// (e.g. WM_SIZE), hence the need to post them to the queue.
extern "system" fn window_proc(
    hwnd: WindowsAndMessaging::HWND,
    msg: u32,
    wparam: WindowsAndMessaging::WPARAM,
    lparam: WindowsAndMessaging::LPARAM,
) -> SystemServices::LRESULT {
    static mut LAST_SIZE_LPARAM: WindowsAndMessaging::LPARAM = WindowsAndMessaging::LPARAM(0);
    unsafe {
        match msg {
            WindowsAndMessaging::WM_CLOSE | WindowsAndMessaging::WM_DESTROY => {
                WindowsAndMessaging::PostQuitMessage(0);
            }
            //* Resize handling.
            //* Note that exit size is sent after the last size, so need to save sizes
            WindowsAndMessaging::WM_SIZE => LAST_SIZE_LPARAM = lparam,
            WindowsAndMessaging::WM_EXITSIZEMOVE => {
                WindowsAndMessaging::PostMessageW(
                    hwnd,
                    WindowsAndMessaging::WM_SIZE,
                    WindowsAndMessaging::WPARAM(0),
                    LAST_SIZE_LPARAM,
                );
            }
            _ => return WindowsAndMessaging::DefWindowProcW(hwnd, msg, wparam, lparam),
        }
    }
    SystemServices::LRESULT(0)
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum Action {
    Quit,
    Resize(u32, u32),
    Keyboard(u32),
    None,
}

pub fn process_messages() -> Action {
    let mut msg = std::mem::MaybeUninit::<WindowsAndMessaging::MSG>::uninit();
    unsafe {
        while WindowsAndMessaging::PeekMessageW(
            msg.as_mut_ptr(),
            WindowsAndMessaging::HWND::NULL,
            0,
            0,
            WindowsAndMessaging::PeekMessage_wRemoveMsg::PM_REMOVE,
        )
        .as_bool()
        {
            let msg_value = msg.assume_init();
            match msg_value.message {
                WindowsAndMessaging::WM_QUIT => return Action::Quit,
                WindowsAndMessaging::WM_SIZE => {
                    let new_size = msg_value.lParam.0 as u32;
                    return Action::Resize((new_size & 0xFFFF) as u32, (new_size >> 16) as u32);
                }
                WindowsAndMessaging::WM_SYSKEYDOWN
                | WindowsAndMessaging::WM_SYSKEYUP
                | WindowsAndMessaging::WM_KEYDOWN
                | WindowsAndMessaging::WM_KEYUP => {
                    let code = msg_value.wParam.0;
                    let was_down = (msg_value.lParam.0 & (1 << 30)) != 0;
                    let is_down = (msg_value.lParam.0 & (1 << 31)) == 0;
                    //* These are key repeats
                    if was_down == is_down {
                        break;
                    } else {
                        return Action::Keyboard(code as u32);
                    }
                }
                _ => {
                    WindowsAndMessaging::TranslateMessage(msg.as_ptr());
                    WindowsAndMessaging::DispatchMessageW(msg.as_ptr());
                }
            }
        }
    };
    Action::None
}
