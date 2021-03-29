#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <stdint.h>
#include <math.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265358979323846264338327950

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef int32 bool32;

typedef float real32;
typedef double real64;

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
    return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// @NOTE XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// @NOTE DirectSoundCreate
#define DIRECT_SOUND_CREATE(name) DWORD WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

global_variable bool GobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

internal void
Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if (!DSoundLibrary)
    {
        return;
    }
    direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

    LPDIRECTSOUND DirectSound;
    if (!(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))))
    {
        return;
    }
    if (!SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
    {
        return;
    }

    // Primary buffer
    DSBUFFERDESC BufferDescriptionPrimary = {};
    BufferDescriptionPrimary.dwSize = sizeof(BufferDescriptionPrimary);
    BufferDescriptionPrimary.dwFlags = DSBCAPS_PRIMARYBUFFER;
    LPDIRECTSOUNDBUFFER PrimaryBuffer;
    if (!SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescriptionPrimary, &PrimaryBuffer, 0)))
    {
        return;
    }

    WAVEFORMATEX WaveFormat = {};
    WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
    WaveFormat.nChannels = 2;
    WaveFormat.wBitsPerSample = 16;
    WaveFormat.nSamplesPerSec = SamplesPerSecond;
    WaveFormat.nBlockAlign = WaveFormat.nChannels * WaveFormat.wBitsPerSample / 8;
    WaveFormat.nAvgBytesPerSec = WaveFormat.nBlockAlign * SamplesPerSecond;
    WaveFormat.cbSize = 0;

    if (!SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
    {
        return;
    };

    // Secondary buffer
    DSBUFFERDESC BufferDescriptionSecondary = {};
    BufferDescriptionSecondary.dwSize = sizeof(BufferDescriptionSecondary);
    BufferDescriptionSecondary.dwFlags = 0;
    BufferDescriptionSecondary.dwBufferBytes = BufferSize;
    BufferDescriptionSecondary.lpwfxFormat = &WaveFormat;
    if (!SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescriptionSecondary, &GlobalSecondaryBuffer, 0)))
    {
        return;
    }
}

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
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

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

        bool AltIsDown = (LParam & (1 << 29)) != 0;
        if (AltIsDown && VKCode == VK_F4)
        {
            GobalRunning = false;
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

struct win32_sound_output
{
    int SamplesPerSecond;
    int BytesPerSample;
    int ToneHz;
    int ToneVolume;
    uint32 RunningSampleIndex;
    int WavePeriod;
    int32 SecondaryBufferSize;
    real32 tSine;
    int LatencySampleCount;
};

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;

    HRESULT LockResult = GlobalSecondaryBuffer->Lock(ByteToLock,
                                                     BytesToWrite,
                                                     &Region1, &Region1Size,
                                                     &Region2, &Region2Size,
                                                     0);
    if (SUCCEEDED(LockResult))
    {
        int16 *SampleOut = (int16 *)Region1;
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
        {
            real32 SineValue = sinf(SoundOutput->tSine);
            int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;
            SoundOutput->tSine += 2.0f * Pi32 * 1.0f / (real32)SoundOutput->WavePeriod;
            ++SoundOutput->RunningSampleIndex;
        }
        SampleOut = (int16 *)Region2;
        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
        {

            real32 SineValue = sinf(SoundOutput->tSine);
            int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;
            SoundOutput->tSine += 2.0f * Pi32 * 1.0f / (real32)SoundOutput->WavePeriod;
            ++SoundOutput->RunningSampleIndex;
        }

        HRESULT UnlockResult = GlobalSecondaryBuffer->Unlock(Region1, Region1Size,
                                                             Region2, Region2Size);
    }
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

    WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
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

    // @NOTE just one DC here because of CS_OWNDC above
    HDC DeviceContext = GetDC(Window);

    GobalRunning = true;

    int XOffset = 0;
    int YOffset = 0;

    // @NOTE: Sample is both left/right (32 bits)
    win32_sound_output SoundOutput = {};

    SoundOutput.SamplesPerSecond = 48000;
    SoundOutput.BytesPerSample = sizeof(int16) * 2;
    SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
    SoundOutput.ToneHz = 256;
    SoundOutput.ToneVolume = 500;
    SoundOutput.RunningSampleIndex = 0;
    SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
    SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;

    Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
    Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample);
    GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

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

                int16 Deadzone = 13;

                int16 OffsetXBy;
                if (StickX < 0)
                {
                    OffsetXBy = ((StickX * -1) >> Deadzone) * -1;
                }
                else
                {
                    OffsetXBy = StickX >> Deadzone;
                }
                int16 OffsetYBy;
                if (StickY < 0)
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
                if (AButton)
                {
                    SoundOutput.ToneHz = 512;
                    SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
                }
                if (XButton)
                {
                    SoundOutput.ToneHz = 256;
                    SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
                }
                SoundOutput.ToneHz = 512 + (int)(256.0f * ((real32)StickY / 30000.0f));
                SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
            }
            else
            {
            }
        }

        RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);

        // @NOTE sound
        DWORD PlayCursor;
        DWORD WriteCursor;
        HRESULT GetCurrentPositionResult = GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
        if (SUCCEEDED(GetCurrentPositionResult))
        {
            DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;

            DWORD TargetCursor =
                (PlayCursor +
                 (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) %
                SoundOutput.SecondaryBufferSize;
            DWORD BytesToWrite;

            if (ByteToLock > TargetCursor)
            {
                BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock) + TargetCursor;
            }
            else
            {
                BytesToWrite = TargetCursor - ByteToLock;
            }

            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
        }

        win32_window_dimension Dim = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(DeviceContext,
                                   Dim.Width, Dim.Height,
                                   GlobalBackBuffer);
    }

    return (0);
}
