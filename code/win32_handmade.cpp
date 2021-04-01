#include "handmade.cpp"

#include <math.h>

#include <windows.h>
#include <xinput.h>
#include <dsound.h>

struct win32_offscreen_buffer {
    BITMAPINFO Info;
    void* Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_window_dimension {
    int Width;
    int Height;
};

//* XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) {
    return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//* XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) {
    return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

//* DirectSoundCreate
#define DIRECT_SOUND_CREATE(name)\
    DWORD WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

global_variable bool32 GobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

internal void
Win32LoadXInput(void) {
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (!XInputLibrary) {
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    if (!XInputLibrary) {
        XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
    }
    if (XInputLibrary) {
        XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize) {
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if (!DSoundLibrary) {
        return;
    }
    direct_sound_create* DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

    LPDIRECTSOUND DirectSound;
    if (!(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))) {
        return;
    }
    if (!SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
        return;
    }

    //* Primary buffer
    DSBUFFERDESC BufferDescriptionPrimary = {};
    BufferDescriptionPrimary.dwSize = sizeof(BufferDescriptionPrimary);
    BufferDescriptionPrimary.dwFlags = DSBCAPS_PRIMARYBUFFER;
    LPDIRECTSOUNDBUFFER PrimaryBuffer;
    if (!SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescriptionPrimary, &PrimaryBuffer, 0))) {
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

    if (!SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) {
        return;
    };

    //* Secondary buffer
    DSBUFFERDESC BufferDescriptionSecondary = {};
    BufferDescriptionSecondary.dwSize = sizeof(BufferDescriptionSecondary);
    BufferDescriptionSecondary.dwFlags = 0;
    BufferDescriptionSecondary.dwBufferBytes = BufferSize;
    BufferDescriptionSecondary.lpwfxFormat = &WaveFormat;
    if (!SUCCEEDED(
        DirectSound->CreateSoundBuffer(&BufferDescriptionSecondary, &GlobalSecondaryBuffer, 0)
    )) {
        return;
    }
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window) {
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    win32_window_dimension Dimensions;
    Dimensions.Width = ClientRect.right - ClientRect.left;
    Dimensions.Height = ClientRect.bottom - ClientRect.top;
    return (Dimensions);
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer* Buffer, int Width, int Height) {
    if (Buffer->Memory) {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Width;
    //* When negative - bimap becomes top-down
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
    win32_offscreen_buffer Buffer) {
    StretchDIBits(
        DeviceContext,
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer.Width, Buffer.Height,
        Buffer.Memory, &Buffer.Info,
        DIB_RGB_COLORS, SRCCOPY
    );
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
    UINT Message,
    WPARAM WParam,
    LPARAM LParam) {
    LRESULT Result = 0;

    switch (Message) {
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
        bool32 WasDown = ((LParam & (1 << 30)) != 0);
        bool32 isDown = ((LParam & (1 << 31)) == 0);
        //* These are key repeats
        if (WasDown == isDown) {
            break;
        }
        if (VKCode == 'W') {
            if (isDown) {
                OutputDebugStringA("W DOWN");
            } else {
                OutputDebugStringA("W UP");
            }
        } else if (VKCode == 'A') {
            if (WasDown) {
                OutputDebugStringA("A WAS DOWN\n");
            } else {
                OutputDebugStringA("A WAS NOT DOWN\n");
            }
        } else if (VKCode == 'S') {
            OutputDebugStringA("S");
        } else if (VKCode == 'D') {
            OutputDebugStringA("D");
        } else if (VKCode == 'Q') {
            OutputDebugStringA("Q");
        } else if (VKCode == 'E') {
            OutputDebugStringA("E");
        } else if (VKCode == VK_UP) {
            OutputDebugStringA("UP");
        } else if (VKCode == VK_DOWN) {
            OutputDebugStringA("DOWN");
        } else if (VKCode == VK_LEFT) {
            OutputDebugStringA("LEFT");
        } else if (VKCode == VK_RIGHT) {
            OutputDebugStringA("RIGHT");
        } else if (VKCode == VK_ESCAPE) {
            OutputDebugStringA("ESCAPE");
        } else if (VKCode == VK_SPACE) {
            OutputDebugStringA("SPACE");
        }

        bool32 AltIsDown = (LParam & (1 << 29)) != 0;
        if (AltIsDown && VKCode == VK_F4) {
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

struct win32_sound_output {
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
Win32FillSoundBuffer(
    win32_sound_output* SoundOutput, DWORD ByteToLock, DWORD BytesToWrite,
    game_sound_buffer* SourceBuffer
) {
    VOID* Region1;
    DWORD Region1Size;
    VOID* Region2;
    DWORD Region2Size;

    HRESULT LockResult = GlobalSecondaryBuffer->Lock(ByteToLock,
        BytesToWrite,
        &Region1, &Region1Size,
        &Region2, &Region2Size,
        0
    );
    if (SUCCEEDED(LockResult)) {
        int16* SourceSample = SourceBuffer->Samples;
        int16* DestSample = (int16*)Region1;
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }
        DestSample = (int16*)Region2;
        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        HRESULT UnlockResult = GlobalSecondaryBuffer->Unlock(
            Region1, Region1Size,
            Region2, Region2Size
        );
    }
}

internal void Win32ClearBuffer(win32_sound_output* SoundOutput) {
    VOID* Region1;
    DWORD Region1Size;
    VOID* Region2;
    DWORD Region2Size;
    HRESULT LockResult = GlobalSecondaryBuffer->Lock(
        0,
        SoundOutput->SecondaryBufferSize,
        &Region1, &Region1Size,
        &Region2, &Region2Size,
        0
    );
    if (SUCCEEDED(LockResult)) {
        uint8* DestByte = (uint8*)Region1;
        for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex) {
            *DestByte++ = 0;
        }
        DestByte = (uint8*)Region2;
        for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex) {
            *DestByte++ = 0;
        }
        HRESULT UnlockResult = GlobalSecondaryBuffer->Unlock(
            Region1, Region1Size,
            Region2, Region2Size
        );
    }
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode) {

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
        0
    );

    //* Just one DC here because of CS_OWNDC above
    HDC DeviceContext = GetDC(Window);

    GobalRunning = true;

    int XOffset = 0;
    int YOffset = 0;

    //* Sample is both left/right (32 bits)
    win32_sound_output SoundOutput = {};

    SoundOutput.SamplesPerSecond = 48000;
    SoundOutput.BytesPerSample = sizeof(int16) * 2;
    SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 16;
    SoundOutput.ToneHz = 256;
    SoundOutput.ToneVolume = 500;
    SoundOutput.RunningSampleIndex = 0;
    SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
    SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;

    Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);

    int16* Samples = (int16*)VirtualAlloc(
        0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE
    );

    Win32ClearBuffer(&SoundOutput);
    GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

    //* Timing

    LARGE_INTEGER PerfCounterFrequencyResult;
    QueryPerformanceFrequency(&PerfCounterFrequencyResult);
    int64 PerfCounterFrequency = PerfCounterFrequencyResult.QuadPart;

    LARGE_INTEGER LastCounter;
    QueryPerformanceCounter(&LastCounter);

    uint64 LastCycleCount = __rdtsc();

    while (GobalRunning) {
        MSG Message;
        while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
            if (Message.message == WM_QUIT) {
                GobalRunning = false;
            }
            TranslateMessage(&Message);
            DispatchMessageA(&Message);
        };

        for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex) {
            XINPUT_STATE ControllerState;
            if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) {
                XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

                bool32 Up = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                bool32 Down = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                bool32 Left = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                bool32 Right = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

                bool32 Back = Pad->wButtons & XINPUT_GAMEPAD_BACK;
                bool32 Start = Pad->wButtons & XINPUT_GAMEPAD_START;

                bool32 LeftShoulder = Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                bool32 RightShoulder = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;

                bool32 AButton = Pad->wButtons & XINPUT_GAMEPAD_A;
                bool32 BButton = Pad->wButtons & XINPUT_GAMEPAD_B;
                bool32 XButton = Pad->wButtons & XINPUT_GAMEPAD_X;
                bool32 YButton = Pad->wButtons & XINPUT_GAMEPAD_Y;

                int16 StickX = Pad->sThumbLX;
                int16 StickY = Pad->sThumbLY;

                XOffset += StickX / 4096;
                YOffset -= StickY / 4096;

                XINPUT_VIBRATION Vibration = {};
                if (BButton) {
                    Vibration.wLeftMotorSpeed = 20000;
                    Vibration.wRightMotorSpeed = 20000;
                    XInputSetState(ControllerIndex, &Vibration);
                }
                if (YButton) {
                    Vibration.wLeftMotorSpeed = 0;
                    Vibration.wRightMotorSpeed = 0;
                    XInputSetState(ControllerIndex, &Vibration);
                }
                if (AButton) {
                    SoundOutput.ToneHz = 512;
                    SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
                }
                if (XButton) {
                    SoundOutput.ToneHz = 256;
                    SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
                }
                SoundOutput.ToneHz = 512 + (int)(256.0f * ((real32)StickY / 30000.0f));
                SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
            } else {
            }
        }

        //* Sound

        bool32 SoundIsValid = false;
        DWORD ByteToLock;
        DWORD TargetCursor;
        DWORD BytesToWrite;

        DWORD PlayCursor;
        DWORD WriteCursor;

        HRESULT GetCurrentPositionResult =
            GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
        if (SUCCEEDED(GetCurrentPositionResult)) {int16 Samples[48000 * 2];
            ByteToLock =
                (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
                SoundOutput.SecondaryBufferSize;

            TargetCursor =
                (PlayCursor +
                    (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) %
                SoundOutput.SecondaryBufferSize;

            if (ByteToLock > TargetCursor) {
                BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock) + TargetCursor;
            } else {
                BytesToWrite = TargetCursor - ByteToLock;
            }

            SoundIsValid = true;
        }

        //* Game

        game_offscreen_buffer GraphicsBuffer = {};
        GraphicsBuffer.Memory = GlobalBackBuffer.Memory;
        GraphicsBuffer.Width = GlobalBackBuffer.Width;
        GraphicsBuffer.Height = GlobalBackBuffer.Height;
        GraphicsBuffer.Pitch = GlobalBackBuffer.Pitch;

        game_sound_buffer SoundBuffer = {};
        SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
        SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
        SoundBuffer.Samples = Samples;

        GameUpdateAndRender(&GraphicsBuffer, XOffset, YOffset, &SoundBuffer, SoundOutput.ToneHz);

        //* Then sound again I guess

        if (SoundIsValid) {
            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
        }

        win32_window_dimension Dim = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(
            DeviceContext,
            Dim.Width, Dim.Height,
            GlobalBackBuffer
        );

        //* Timing

        LARGE_INTEGER EndCounter;
        QueryPerformanceCounter(&EndCounter);
        int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
        LastCounter = EndCounter;

        uint64 EndCycleCount = __rdtsc();
        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
        LastCycleCount = EndCycleCount;

        int64 MsPerFrame = (1000 * CounterElapsed) / PerfCounterFrequency;
        int64 FPS = 1000 / MsPerFrame;

        char OutputBuffer[256];
        wsprintfA(
            OutputBuffer,
            "Frame: %d ms | %d FPS | %d Mcycles\n",
            MsPerFrame, FPS, CyclesElapsed / 1000 / 1000
        );
        OutputDebugStringA(OutputBuffer);
    }

    return (0);
}
