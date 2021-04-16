#include "handmade.cpp"

#include <math.h>
#include <stdio.h>

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

struct win32_sound_output {
    int SamplesPerSecond;
    int BytesPerSample;
    uint32 RunningSampleIndex;
    int32 SecondaryBufferSize;
    real32 tSine;
    int LatencySampleCount;
};

struct win32_window_dimension {
    int Width;
    int Height;
};

struct win32_debug_time_marker {
    DWORD PlayCursor;
    DWORD WriteCursor;
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

global_variable bool32 GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCounterFrequency;

internal debug_read_file_result DEBUGPlatformReadEntireFile(char* Filename) {

    debug_read_file_result Result = {};

    HANDLE FileHandle = CreateFileA(
        Filename, GENERIC_READ, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0
    );
    if (FileHandle == INVALID_HANDLE_VALUE) {
        return Result;
    }
    LARGE_INTEGER FileSize;
    DWORD FileSizeResult = GetFileSizeEx(FileHandle, &FileSize);
    if (FileSizeResult == 0) {
        CloseHandle(FileHandle);
        return Result;
    }
    Result.Size = SafeTruncateUint64(FileSize.QuadPart);
    Result.Contents = VirtualAlloc(0, Result.Size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (Result.Contents == 0) {
        CloseHandle(FileHandle);
        return Result;
    }
    DWORD BytesRead;
    BOOL ReadFileResult = ReadFile(FileHandle, Result.Contents, Result.Size, &BytesRead, 0);
    if (ReadFileResult == 0 || Result.Size != BytesRead) {
        DEBUGPlatformFreeFileMemory(Result.Contents);
        CloseHandle(FileHandle);
        return Result;
    }
    CloseHandle(FileHandle);
    return Result;
}

internal void DEBUGPlatformFreeFileMemory(void* Memory) {
    if (!Memory) {
        return;
    }
    VirtualFree(Memory, 0, MEM_RELEASE);
}

internal bool32 DEBUGPlatformWriteEntireFile(char* Filename, uint32 MemorySize, void* Memory) {
    HANDLE FileHandle = CreateFileA(
        Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0
    );
    if (FileHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD BytesWritten;
    BOOL WriteFileResult = WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0);
    CloseHandle(FileHandle);
    if (WriteFileResult == 0 || MemorySize != BytesWritten) {
        return false;
    }
    return true;
}

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
    BufferDescriptionSecondary.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
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
        GlobalRunning = false;
    }
    break;
    case WM_CLOSE:
    {
        GlobalRunning = false;
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
        Assert(!"Keyboard input came in through a non-dispatch message");
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

internal void Win32ProcessXInputDigitalButton(
    game_button_state* Old, game_button_state* New,
    DWORD XInputButtonState, DWORD ButtonBit
) {
    New->EndedDown = (XInputButtonState & ButtonBit) == ButtonBit;
    New->HalfTransitionCount = (Old->EndedDown == New->EndedDown) ? 0 : 1;
}

internal void Win32ProcessKeyboardMessage(
    game_button_state* New,
    bool32 IsDown
) {
    Assert(New->EndedDown != IsDown);
    New->EndedDown = IsDown;
    ++New->HalfTransitionCount;
}

internal real32 Win32ProcessXInputStickValue(int16 Value, int16 Deadzone) {
    real32 Result = 0;
    if (Value < -Deadzone) {
        Result = (real32)((Value + Deadzone) / (32768.0f - Deadzone));
    } else if (Value > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        Result = (real32)((Value - Deadzone) / (32767.0f - Deadzone));
    }
    return Result;
}

internal void Win32ProcessPendingMessages(game_controller_input* KeyboardController) {
    MSG Message;
    while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
        switch (Message.message) {
        case WM_QUIT: {
            GlobalRunning = false;
        }
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            uint32 VKCode = (uint32)Message.wParam;
            bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
            bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);
            //* These are key repeats
            if (WasDown == IsDown) {
                break;
            }
            if (VKCode == 'W') {
                Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
            } else if (VKCode == 'A') {
                Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
            } else if (VKCode == 'S') {
                Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
            } else if (VKCode == 'D') {
                Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
            } else if (VKCode == 'Q') {
                Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
            } else if (VKCode == 'E') {
                Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
            } else if (VKCode == VK_UP) {
                Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
            } else if (VKCode == VK_DOWN) {
                Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
            } else if (VKCode == VK_LEFT) {
                Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
            } else if (VKCode == VK_RIGHT) {
                Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
            } else if (VKCode == VK_ESCAPE) {
                Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
            } else if (VKCode == VK_SPACE) {
                Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
            }

            bool32 AltIsDown = (Message.lParam & (1 << 29)) != 0;
            if (AltIsDown && VKCode == VK_F4) {
                GlobalRunning = false;
            }
        }
        break;
        default:
        {
            TranslateMessage(&Message);
            DispatchMessageA(&Message);
        } break;
        }
    };
}


inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End) {
    real32 SecondsElapsed = (real32)(End.QuadPart - Start.QuadPart) / (real32)GlobalPerfCounterFrequency;
    return SecondsElapsed;
}

inline LARGE_INTEGER Win32GetWallClock() {
    LARGE_INTEGER Counter;
    QueryPerformanceCounter(&Counter);
    return Counter;
}

internal void Wind32DebugDrawVertical(
    win32_offscreen_buffer* BackBuffer, int32 X, int32 Top, int32 Bottom,
    uint32 Color
) {
    uint8* Pixel =
        (uint8*)BackBuffer->Memory + X * BackBuffer->BytesPerPixel
        + Top * BackBuffer->Pitch;
    for (int Y = Top; Y < Bottom; ++Y) {
        *(uint32*)Pixel = Color;
        Pixel += BackBuffer->Pitch;
    }
}

internal void Win32DebugSyncDisplay(
    win32_offscreen_buffer* BackBuffer,
    int32 MarkerCount, win32_debug_time_marker* LastMarker,
    win32_sound_output* SoundOutput, real32 SecondsPerFrame
) {
    int32 PadX = 16;
    int32 PadY = 16;
    int32 Top = PadY;
    int32 Bottom = BackBuffer->Height - PadY;
    real32 C =
        ((real32)BackBuffer->Width - 2 * PadX) / (real32)SoundOutput->SecondaryBufferSize;
    for (int32 MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex) {
        win32_debug_time_marker ThisMarker = LastMarker[MarkerIndex];
        Assert((uint32)ThisMarker.PlayCursor < (uint32)SoundOutput->SecondaryBufferSize);
        Assert((uint32)ThisMarker.WriteCursor < (uint32)SoundOutput->SecondaryBufferSize);
        int32 X = PadX + (int32)(C * (real32)ThisMarker.PlayCursor);
        Wind32DebugDrawVertical(BackBuffer, X, Top, Bottom, 0xFFFFFF);
        X = PadX + (int32)(C * (real32)ThisMarker.WriteCursor);
        Wind32DebugDrawVertical(BackBuffer, X, Top, Bottom, 0xFF0000);
    }
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode) {

    LARGE_INTEGER PerfCounterFrequencyResult;
    QueryPerformanceFrequency(&PerfCounterFrequencyResult);
    GlobalPerfCounterFrequency = PerfCounterFrequencyResult.QuadPart;

    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR;

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

    GlobalRunning = true;

    int XOffset = 0;
    int YOffset = 0;

#define FramesAudioLatency 3
#define MonitorRefreshHz 60
#define GameRefreshHz (MonitorRefreshHz / 2)
    real32 TargetSecondsPerFrame = 1.0f / (real32)GameRefreshHz;

    //* Sample is both left/right (32 bits)
    win32_sound_output SoundOutput = {};

    SoundOutput.SamplesPerSecond = 48000;
    SoundOutput.BytesPerSample = sizeof(int16) * 2;
    SoundOutput.LatencySampleCount =
        FramesAudioLatency * (SoundOutput.SamplesPerSecond / GameRefreshHz);
    SoundOutput.RunningSampleIndex = 0;
    SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;

    Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);

    int16* Samples = (int16*)VirtualAlloc(
        0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE
    );

    Win32ClearBuffer(&SoundOutput);
    GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

    //* Input
    game_input Input[2] = {};
    game_input* NewInput = &Input[0];
    game_input* OldInput = &Input[1];

    //* Memory

#if HANDMADE_INTERNAL
    LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
    LPVOID BaseAddress = 0;
#endif

    game_memory GameMemory;
    GameMemory.IsInitialized = false;
    GameMemory.PermanentStorageSize = Megabytes(64);
    GameMemory.TransientStorageSize = Gigabytes(1);

    uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;

    GameMemory.PermanentStorage = VirtualAlloc(
        BaseAddress, TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE
    );

    GameMemory.TransientStorage =
        (uint8*)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize;


    //* Timing


    LARGE_INTEGER LastCounter = Win32GetWallClock();

    uint64 LastCycleCount = __rdtsc();

    int32 DebugLastMarkerIndex = 0;
    win32_debug_time_marker DebugLastMarker[GameRefreshHz / 2] = {};
    DWORD LastPlayCursor = 0;
    bool32 SoundIsValid = false;

    while (GlobalRunning) {
        game_controller_input* OldKeyboardController = GetController(OldInput, 0);
        game_controller_input* NewKeyboardController = GetController(NewInput, 0);
        *NewKeyboardController = {};
        NewKeyboardController->IsConnected = true;
        for (int ButtonIndex = 0;
            ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
            ++ButtonIndex) {
            NewKeyboardController->Buttons[ButtonIndex].EndedDown =
                OldKeyboardController->Buttons[ButtonIndex].EndedDown;
        }

        Win32ProcessPendingMessages(NewKeyboardController);

        DWORD MaxControllerCount = XUSER_MAX_COUNT;
        if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1)) {
            MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
        }

        for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex) {
            DWORD OurControllerIndex = ControllerIndex + 1;
            game_controller_input* OldController = GetController(OldInput, OurControllerIndex);
            game_controller_input* NewController = GetController(NewInput, OurControllerIndex);

            XINPUT_STATE ControllerState;
            if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) {
                NewController->IsConnected = true;
                XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;
                DWORD ButtonState = Pad->wButtons;

                bool32 Up = ButtonState & XINPUT_GAMEPAD_DPAD_UP;
                bool32 Down = ButtonState & XINPUT_GAMEPAD_DPAD_DOWN;
                bool32 Left = ButtonState & XINPUT_GAMEPAD_DPAD_LEFT;
                bool32 Right = ButtonState & XINPUT_GAMEPAD_DPAD_RIGHT;

                NewController->StickAverageX = Win32ProcessXInputStickValue(
                    Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
                );
                NewController->StickAverageY = Win32ProcessXInputStickValue(
                    Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
                );

                if ((NewController->StickAverageX != 0.0) || (NewController->StickAverageY != 0.0)) {
                    NewController->IsAnalog = true;
                }

                if (ButtonState & XINPUT_GAMEPAD_DPAD_UP) {
                    NewController->StickAverageY = 1;
                    NewController->IsAnalog = false;
                }
                if (ButtonState & XINPUT_GAMEPAD_DPAD_DOWN) {
                    NewController->StickAverageY = -1;
                    NewController->IsAnalog = false;
                }
                if (ButtonState & XINPUT_GAMEPAD_DPAD_LEFT) {
                    NewController->StickAverageX = -1;
                    NewController->IsAnalog = false;
                }
                if (ButtonState & XINPUT_GAMEPAD_DPAD_RIGHT) {
                    NewController->StickAverageX = 1;
                    NewController->IsAnalog = false;
                }

                real32 Threshold = 0.5f;
                Win32ProcessXInputDigitalButton(
                    &OldController->MoveLeft, &NewController->MoveLeft,
                    (NewController->StickAverageX < -Threshold ? 1 : 0), 1
                );
                Win32ProcessXInputDigitalButton(
                    &OldController->MoveRight, &NewController->MoveRight,
                    (NewController->StickAverageX > Threshold ? 1 : 0), 1
                );
                Win32ProcessXInputDigitalButton(
                    &OldController->MoveDown, &NewController->MoveDown,
                    (NewController->StickAverageY < -Threshold ? 1 : 0), 1
                );
                Win32ProcessXInputDigitalButton(
                    &OldController->MoveUp, &NewController->MoveUp,
                    (NewController->StickAverageY > Threshold ? 1 : 0), 1
                );

                Win32ProcessXInputDigitalButton(
                    &OldController->ActionDown, &NewController->ActionDown,
                    ButtonState, XINPUT_GAMEPAD_A
                );
                Win32ProcessXInputDigitalButton(
                    &OldController->ActionRight, &NewController->ActionRight,
                    ButtonState, XINPUT_GAMEPAD_B
                );
                Win32ProcessXInputDigitalButton(
                    &OldController->ActionLeft, &NewController->ActionLeft,
                    ButtonState, XINPUT_GAMEPAD_X
                );
                Win32ProcessXInputDigitalButton(
                    &OldController->ActionUp, &NewController->ActionUp,
                    ButtonState, XINPUT_GAMEPAD_Y
                );

                Win32ProcessXInputDigitalButton(
                    &OldController->LeftShoulder, &NewController->LeftShoulder,
                    ButtonState, XINPUT_GAMEPAD_LEFT_SHOULDER
                );
                Win32ProcessXInputDigitalButton(
                    &OldController->RightShoulder, &NewController->RightShoulder,
                    ButtonState, XINPUT_GAMEPAD_RIGHT_SHOULDER
                );

                Win32ProcessXInputDigitalButton(
                    &OldController->Back, &NewController->Back,
                    ButtonState, XINPUT_GAMEPAD_BACK
                );
                Win32ProcessXInputDigitalButton(
                    &OldController->Start, &NewController->Start,
                    ButtonState, XINPUT_GAMEPAD_START
                );

            } else {
                NewController->IsConnected = false;
            }
        }

        //* Sound

        DWORD ByteToLock = 0;
        DWORD TargetCursor = 0;
        DWORD BytesToWrite = 0;

        if (SoundIsValid) {
            ByteToLock =
                (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
                SoundOutput.SecondaryBufferSize;

            TargetCursor =
                (LastPlayCursor +
                    (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) %
                SoundOutput.SecondaryBufferSize;

            if (ByteToLock > TargetCursor) {
                BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock) + TargetCursor;
            } else {
                BytesToWrite = TargetCursor - ByteToLock;
            }
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

        GameUpdateAndRender(&GameMemory, NewInput, &GraphicsBuffer, &SoundBuffer);

        //* Input

        game_input* Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;

        //* Then sound again I guess

        if (SoundIsValid) {
#if HANDMADE_INTERNAL
            DWORD PlayCursorBeforeFill = 0;
            DWORD WriteCursorBeforeFill = 0;
            GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursorBeforeFill, &WriteCursorBeforeFill);
            char SoundTextBuffer[256];
            sprintf_s(
                SoundTextBuffer,
                "LPC: %u; BTL: %u, TC: %u, BTW: %u - PC: %u, WC: %u\n",
                LastPlayCursor, ByteToLock, TargetCursor, BytesToWrite,
                PlayCursorBeforeFill, WriteCursorBeforeFill
            );
            OutputDebugStringA(SoundTextBuffer);
#endif
            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
        }

        //* Timing

        real32 SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
        if (SecondsElapsedForFrame < TargetSecondsPerFrame) {
            if (SleepIsGranular) {
                DWORD SleepMS =
                    (DWORD)((TargetSecondsPerFrame - SecondsElapsedForFrame) * 1000.0f) - 1;
                if (SleepMS > 0) {
                    Sleep(SleepMS);
                }
            }
            SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
            Assert(SecondsElapsedForFrame <= TargetSecondsPerFrame);
            while (SecondsElapsedForFrame < TargetSecondsPerFrame) {
                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
            }
        } else {
            //* MISSED FRAME
        }
        LastCounter = Win32GetWallClock();

        //* Display

        win32_window_dimension Dim = Win32GetWindowDimension(Window);
#if HANDMADE_INTERNAL
        Win32DebugSyncDisplay(
            &GlobalBackBuffer,
            ArrayCount(DebugLastMarker), DebugLastMarker,
            &SoundOutput, TargetSecondsPerFrame
        );
#endif
        Win32DisplayBufferInWindow(
            DeviceContext,
            Dim.Width, Dim.Height,
            GlobalBackBuffer
        );

        DWORD PlayCursor = 0;
        DWORD WriteCursor = 0;
        if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {
            LastPlayCursor = PlayCursor;
            if (!SoundIsValid) {
                SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                SoundIsValid = true;
            }
        } else {
            SoundIsValid = false;
        }

        //* Debug sound
#if HANDMADE_INTERNAL
        {
            Assert(DebugLastMarkerIndex < ArrayCount(DebugLastMarker));
            DebugLastMarker[DebugLastMarkerIndex].PlayCursor = PlayCursor;
            DebugLastMarker[DebugLastMarkerIndex].WriteCursor = WriteCursor;
            DebugLastMarkerIndex++;
            if (DebugLastMarkerIndex == ArrayCount(DebugLastMarker)) {
                DebugLastMarkerIndex = 0;
            }
        }
#endif
        real32 FPS = 1.0f / SecondsElapsedForFrame;

        uint64 EndCycleCount = __rdtsc();
        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
        LastCycleCount = __rdtsc();
#if 0
        char OutputBuffer[256];
        sprintf_s(
            OutputBuffer,
            "Frame: %.04f s | %.02f FPS | %.02f Mcycles\n",
            SecondsElapsedForFrame, FPS, (real32)CyclesElapsed / 1000.0f / 1000.0f
        );
        OutputDebugStringA(OutputBuffer);
#endif
    }

    return (0);
}
