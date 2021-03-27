#include <windows.h>
#include <xinput.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

// @NOTE XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return 0;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// @NOTE XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return 0;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void
Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

global_variable bool GobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;

internal void
RenderWeirdGradient(win32_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
    int Width = Buffer->Width;
    uint8 *Row = (uint8 *)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; Y++)
    {
        uint32 *Pixel = (uint32 *)Row;

        for (int X = 0; X < Buffer->Width; X++)
        {
            uint8 Blue = (X + XOffset);
            uint8 Green = (Y + YOffset);
            *Pixel++ = (Green << 8 | Blue);
        }

        Row += Buffer->Pitch;
    }
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    win32_window_dimension Dimensions;
    Dimensions.Width = ClientRect.right - ClientRect.left;
    Dimensions.Height = ClientRect.bottom - ClientRect.top;
    return (Dimensions);
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Width;
    // When negative - bimap becomes top-down
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    Buffer->BytesPerPixel = 4;

    int BitmapMemorySize = Buffer->BytesPerPixel * Width * Height;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(HDC DeviceContext,
                           int WindowWidth, int WindowHeight,
                           win32_offscreen_buffer Buffer)
{
    StretchDIBits(DeviceContext,
                  0, 0, WindowWidth, WindowHeight,
                  0, 0, Buffer.Width, Buffer.Height,
                  Buffer.Memory, &Buffer.Info,
                  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
                        UINT Message,
                        WPARAM WParam,
                        LPARAM LParam)
{
    LRESULT Result = 0;

    switch (Message)
    {
    case WM_SIZE:
    {
    }
    break;
    case WM_DESTROY:
    {
        GobalRunning = false;
    }
    break;
    case WM_CLOSE:
    {
        GobalRunning = false;
    }
    break;
    case WM_ACTIVATEAPP:
    {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
    }
    break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        uint32 VKCode = WParam;
        bool WasDown = ((LParam & (1 << 30)) != 0);
        bool isDown = ((LParam & (1 << 31)) == 0);
        // @NOTE These are key repeats
        if (WasDown == isDown)
        {
            break;
        }
        if (VKCode == 'W')
        {
            if (isDown)
            {
                OutputDebugStringA("W DOWN");
            }
            else
            {
                OutputDebugStringA("W UP");
            }
        }
        else if (VKCode == 'A')
        {
            if (WasDown)
            {
                OutputDebugStringA("A WAS DOWN\n");
            }
            else
            {
                OutputDebugStringA("A WAS NOT DOWN\n");
            }
        }
        else if (VKCode == 'S')
        {
            OutputDebugStringA("S");
        }
        else if (VKCode == 'D')
        {
            OutputDebugStringA("D");
        }
        else if (VKCode == 'Q')
        {
            OutputDebugStringA("Q");
        }
        else if (VKCode == 'E')
        {
            OutputDebugStringA("E");
        }
        else if (VKCode == VK_UP)
        {
            OutputDebugStringA("UP");
        }
        else if (VKCode == VK_DOWN)
        {
            OutputDebugStringA("DOWN");
        }
        else if (VKCode == VK_LEFT)
        {
            OutputDebugStringA("LEFT");
        }
        else if (VKCode == VK_RIGHT)
        {
            OutputDebugStringA("RIGHT");
        }
        else if (VKCode == VK_ESCAPE)
        {
            OutputDebugStringA("ESCAPE");
        }
        else if (VKCode == VK_SPACE)
        {
            OutputDebugStringA("SPACE");
        }
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);
        win32_window_dimension Dim = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(DeviceContext,
                                   Dim.Width, Dim.Height,
                                   GlobalBackBuffer);

        EndPaint(Window, &Paint);
    }
    break;
    default:
    {
        Result = DefWindowProc(Window, Message, WParam, LParam);
    }
    break;
    }

    return (Result);
}

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
    Win32LoadXInput();

    WNDCLASSA WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    RegisterClassA(&WindowClass);

    HWND Window = CreateWindowExA(
        0,
        WindowClass.lpszClassName,
        "Handmade hero",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        Instance,
        0);

    GobalRunning = true;
    int XOffset = 0;
    int YOffset = 0;
    while (GobalRunning)
    {
        MSG Message;
        while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
        {
            if (Message.message == WM_QUIT)
            {
                GobalRunning = false;
            }
            TranslateMessage(&Message);
            DispatchMessageA(&Message);
        };

        for (DWORD ControllerIndex = 0;
             ControllerIndex < XUSER_MAX_COUNT;
             ++ControllerIndex)
        {
            XINPUT_STATE ControllerState;
            if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
            {
                XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                bool Up = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                bool Down = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                bool Left = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                bool Right = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

                bool Back = Pad->wButtons & XINPUT_GAMEPAD_BACK;
                bool Start = Pad->wButtons & XINPUT_GAMEPAD_START;

                bool LeftShoulder = Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                bool RightShoulder = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;

                bool AButton = Pad->wButtons & XINPUT_GAMEPAD_A;
                bool BButton = Pad->wButtons & XINPUT_GAMEPAD_B;
                bool XButton = Pad->wButtons & XINPUT_GAMEPAD_X;
                bool YButton = Pad->wButtons & XINPUT_GAMEPAD_Y;

                int16 StickX = Pad->sThumbLX;
                int16 StickY = Pad->sThumbLY;

                bool ReverseX = StickX < 0;
                bool ReverseY = StickY < 0;

                int16 Deadzone = 12;

                int16 OffsetXBy;
                if (ReverseX)
                {
                    OffsetXBy = ((StickX * -1) >> Deadzone) * -1;
                }
                else
                {
                    OffsetXBy = StickX >> Deadzone;
                }
                int16 OffsetYBy;
                if (ReverseY)
                {
                    OffsetYBy = ((StickY * -1) >> Deadzone) * -1;
                }
                else
                {
                    OffsetYBy = StickY >> Deadzone;
                }

                XOffset += OffsetXBy;
                YOffset -= OffsetYBy;

                XINPUT_VIBRATION Vibration = {};
                if (BButton)
                {
                    Vibration.wLeftMotorSpeed = 20000;
                    Vibration.wRightMotorSpeed = 20000;
                    XInputSetState(ControllerIndex, &Vibration);
                }
                if (YButton)
                {
                    Vibration.wLeftMotorSpeed = 0;
                    Vibration.wRightMotorSpeed = 0;
                    XInputSetState(ControllerIndex, &Vibration);
                }
            }
            else
            {
            }
        }

        RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);

        HDC DeviceContext = GetDC(Window);
        win32_window_dimension Dim = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(DeviceContext,
                                   Dim.Width, Dim.Height,
                                   GlobalBackBuffer);
        ReleaseDC(Window, DeviceContext);
    }

    return (0);
}
