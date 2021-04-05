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
        uint32 VKCode = (uint32)WParam;
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

        DWORD MaxControllerCount = XUSER_MAX_COUNT;
        if (MaxControllerCount > ArrayCount(NewInput->Controllers)) {
            MaxControllerCount = ArrayCount(NewInput->Controllers);
        }

        for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex) {
            game_controller_input* OldController = &OldInput->Controllers[ControllerIndex];
            game_controller_input* NewController = &NewInput->Controllers[ControllerIndex];

            XINPUT_STATE ControllerState;
            if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) {
                XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;
                DWORD ButtonState = Pad->wButtons;

                bool32 Up = ButtonState & XINPUT_GAMEPAD_DPAD_UP;
                bool32 Down = ButtonState & XINPUT_GAMEPAD_DPAD_DOWN;
                bool32 Left = ButtonState & XINPUT_GAMEPAD_DPAD_LEFT;
                bool32 Right = ButtonState & XINPUT_GAMEPAD_DPAD_RIGHT;

                NewController->IsAnalog = true;

                int16 StickX = Pad->sThumbLX;
                int16 StickY = Pad->sThumbLY;

                real32 X;
                if (StickX < 0) {
                    X = StickX / 32768.0f;
                } else {
                    X = StickX / 32767.0f;
                }

                real32 Y;
                if (StickX < 0) {
                    Y = (real32)StickY / 32768.0f;
                } else {
                    Y = (real32)StickY / 32767.0f;
                }

                NewController->StartX = OldController->EndX;
                NewController->StartY = OldController->EndY;

                NewController->MinX = NewController->MaxX = NewController->EndX = X;
                NewController->MinY = NewController->MaxY = NewController->EndY = Y;

                Win32ProcessXInputDigitalButton(
                    &OldController->Down, &NewController->Down,
                    ButtonState, XINPUT_GAMEPAD_A
                );
                Win32ProcessXInputDigitalButton(
                    &OldController->Right, &NewController->Right,
                    ButtonState, XINPUT_GAMEPAD_B
                );
                Win32ProcessXInputDigitalButton(
                    &OldController->Left, &NewController->Left,
                    ButtonState, XINPUT_GAMEPAD_X
                );
                Win32ProcessXInputDigitalButton(
                    &OldController->Up, &NewController->Up,
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

            } else {
            }
        }

        //* Sound

        bool32 SoundIsValid = false;
        DWORD ByteToLock = 0;
        DWORD TargetCursor = 0;
        DWORD BytesToWrite = 0;

        DWORD PlayCursor = 0;
        DWORD WriteCursor = 0;

        HRESULT GetCurrentPositionResult =
            GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
        if (SUCCEEDED(GetCurrentPositionResult)) {
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

        GameUpdateAndRender(&GameMemory, NewInput, &GraphicsBuffer, &SoundBuffer);

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

        //* Input

        game_input* Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;

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
