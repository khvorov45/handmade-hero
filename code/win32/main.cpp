#include "../game/lib.hpp"

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
    DWORD SafetyBytes;
};

struct win32_window_dimension {
    int Width;
    int Height;
};

struct win32_debug_time_marker {
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputLocation;
    DWORD OutputByteCount;
    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
    DWORD ExpectedFlipCursor;
};

struct win32_recorded_input {
    int InputCount;
    game_input* InputStream;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_replay_buffer {
    HANDLE FileHandle;
    HANDLE MemoryMap;
    char ReplayFilename[WIN32_STATE_FILE_NAME_COUNT];
    void* MemoryBlock;
};
struct win32_state {
    uint64 TotalSize;
    void* GameMemoryBlock;
    win32_replay_buffer ReplayBuffers[4];

    HANDLE RecordingHandle;
    int InputRecordingIndex;

    HANDLE PlayingHandle;
    int InputPlayingIndex;

    char EXEFileName[WIN32_STATE_FILE_NAME_COUNT];
    char* OnePastLastExeFilenameSlash;
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
global_variable int64 GlobalPause;
global_variable bool32 DEBUGGlobalShowCursor;

WINDOWPLACEMENT GlobalWindowPosition = { sizeof(GlobalWindowPosition) };

internal void ToggleFullscreen(HWND Window) {
    // https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
    DWORD Style = GetWindowLong(Window, GWL_STYLE);
    if (Style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
        if (GetWindowPlacement(Window, &GlobalWindowPosition) &&
            GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo)) {
            SetWindowLong(
                Window, GWL_STYLE,
                Style & ~WS_OVERLAPPEDWINDOW
            );
            SetWindowPos(
                Window, HWND_TOP,
                MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED
            );
        }
    } else {
        SetWindowLong(Window, GWL_STYLE, Style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(Window, &GlobalWindowPosition);
        SetWindowPos(
            Window, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED
        );
    }
}

void CatStrings(
    size_t SourceACount, char* SourceA,
    size_t SourceBCount, char* SourceB,
    size_t DestCount, char* Dest
) {
    for (int Index = 0; Index < SourceACount; Index++) {
        *Dest++ = *SourceA++;
    }
    for (int Index = 0; Index < SourceBCount; Index++) {
        *Dest++ = *SourceB++;
    }
    *Dest = '\0';
}

internal int32 StringLength(char* String) {
    int32 Count = 0;
    while (*String++ != '\0') {
        ++Count;
    }
    return Count;
}

internal void Win32GetExeFileName(win32_state* State) {
    GetModuleFileNameA(0, State->EXEFileName, sizeof(State->EXEFileName));
    State->OnePastLastExeFilenameSlash = State->EXEFileName;
    for (char* Scan = State->EXEFileName; *Scan; ++Scan) {
        if (*Scan == '\\') {
            State->OnePastLastExeFilenameSlash = Scan + 1;
        }
    }
}

internal void Win32BuildExePathFilename(win32_state* State, char* Filename, size_t DestCount, char* Dest) {
    CatStrings(
        State->OnePastLastExeFilenameSlash - State->EXEFileName, State->EXEFileName,
        StringLength(Filename), Filename,
        DestCount, Dest
    );
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory) {
    if (!BitmapMemory) {
        return;
    }
    VirtualFree(BitmapMemory, 0, MEM_RELEASE);
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGWin32ReadEntireFile) {

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

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile) {
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

internal FILETIME Win32GetLastWriteTime(char* Filename) {
    WIN32_FILE_ATTRIBUTE_DATA Data;
    BOOL FindResult = GetFileAttributesExA(Filename, GetFileExInfoStandard, &Data);
    if (FindResult == FALSE) {
        FILETIME Result = {};
        return Result;
    }
    return Data.ftLastWriteTime;
}

struct win32_game_code {
    HMODULE GameCodeDLL;
    FILETIME LastWriteTime;
    game_get_sound_samples* GetSoundSamples;
    game_update_and_render* UpdateAndRender;
    bool32 IsValid;
};

internal win32_game_code Win32LoadGameCode(char* Source, char* Locked) {
    win32_game_code Result = {};

    Result.GetSoundSamples = 0;
    Result.UpdateAndRender = 0;

    Result.LastWriteTime = Win32GetLastWriteTime(Source);

    CopyFileA(Source, Locked, FALSE);
    HMODULE GameCodeDLL = LoadLibraryA(Locked);
    if (!GameCodeDLL) {
        return Result;
    }
    Result.GameCodeDLL = GameCodeDLL;

    game_get_sound_samples* GetSoundSamples = (game_get_sound_samples*)GetProcAddress(GameCodeDLL, "GameGetSoundSamples");
    game_update_and_render* UpdateAndRender = (game_update_and_render*)GetProcAddress(GameCodeDLL, "GameUpdateAndRender");
    Result.IsValid = (GetSoundSamples && UpdateAndRender);
    if (!Result.IsValid) {
        return Result;
    }

    Result.GetSoundSamples = GetSoundSamples;
    Result.UpdateAndRender = UpdateAndRender;

    return Result;
}

internal void Win32UnloadCode(win32_game_code* GameCode) {
    if (GameCode->GameCodeDLL) {
        FreeLibrary(GameCode->GameCodeDLL);
        GameCode->GameCodeDLL = 0;
    }
    GameCode->IsValid = false;
    GameCode->GetSoundSamples = 0;
    GameCode->UpdateAndRender = 0;
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
    Buffer->Info.bmiHeader.biHeight = Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    Buffer->BytesPerPixel = 4;
    Buffer->Pitch = Align16(Width * Buffer->BytesPerPixel);

    int BitmapMemorySize = Buffer->Pitch * Height;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

}

internal void Win32DisplayBufferInWindow(
    HDC DeviceContext,
    int WindowWidth, int WindowHeight,
    win32_offscreen_buffer* Buffer
) {
    if (WindowWidth >= Buffer->Width * 2 && WindowHeight >= Buffer->Height * 2) {
        StretchDIBits(
            DeviceContext,
            0, 0, Buffer->Width * 2, Buffer->Height * 2,
            0, 0, Buffer->Width, Buffer->Height,
            Buffer->Memory, &Buffer->Info,
            DIB_RGB_COLORS, SRCCOPY
        );
    } else {
#if 0
        int32 OffsetX = 10;
        int32 OffsetY = 10;
#else

        int32 OffsetX = 0;
        int32 OffsetY = 0;
#endif
        PatBlt(DeviceContext, 0, 0, WindowWidth, OffsetY, BLACKNESS);
        PatBlt(DeviceContext, 0, 0, OffsetX, WindowHeight, BLACKNESS);
        PatBlt(DeviceContext, OffsetX + Buffer->Width, 0, WindowWidth, WindowHeight, BLACKNESS);
        PatBlt(DeviceContext, 0, OffsetY + Buffer->Height, WindowWidth, WindowHeight, BLACKNESS);

        StretchDIBits(
            DeviceContext,
            OffsetX, OffsetY, Buffer->Width, Buffer->Height,
            0, 0, Buffer->Width, Buffer->Height,
            Buffer->Memory, &Buffer->Info,
            DIB_RGB_COLORS, SRCCOPY
        );
    }
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
    UINT Message,
    WPARAM WParam,
    LPARAM LParam) {
    LRESULT Result = 0;

    switch (Message) {
    case WM_SETCURSOR: {
        if (DEBUGGlobalShowCursor) {
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } else {
            SetCursor(0);
        }
    } break;
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
        if (WParam == TRUE) {
            SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
        } else {
            SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 128, LWA_ALPHA);
        }
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
        Win32DisplayBufferInWindow(
            DeviceContext,
            Dim.Width, Dim.Height,
            &GlobalBackBuffer
        );

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
    if (New->EndedDown != IsDown) {
        New->EndedDown = IsDown;
        ++New->HalfTransitionCount;
    }
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

internal void Win32GetInputFileLocation(win32_state* State, bool32 InputStream, int32 SlotIndex, size_t DestCount, char* Dest) {
    char Temp[64];
    wsprintf(Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
    Win32BuildExePathFilename(State, Temp, DestCount, Dest);
}

internal win32_replay_buffer* Win32GetReplayBuffer(win32_state* State, uint32 Index) {
    Assert(Index < ArrayCount(State->ReplayBuffers));
    return &State->ReplayBuffers[Index];
}

internal void Win32BeginRecordingInput(win32_state* State, int InputRecordingIndex) {
    win32_replay_buffer* ReplayBuffer = Win32GetReplayBuffer(State, InputRecordingIndex);
    if (ReplayBuffer->MemoryBlock == 0) {
        return;
    }

    State->InputRecordingIndex = InputRecordingIndex;

    char Filename[WIN32_STATE_FILE_NAME_COUNT];
    Win32GetInputFileLocation(State, true, InputRecordingIndex, sizeof(Filename), Filename);

    State->RecordingHandle = CreateFileA(
        Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0
    );

    CopyMemory(ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
}

internal void Win32EndRecordingInput(win32_state* Win32State) {
    CloseHandle(Win32State->RecordingHandle);
    Win32State->InputRecordingIndex = 0;
}

internal void Win32BeginInputPlayback(win32_state* State, int InputPlayingIndex) {
    win32_replay_buffer* ReplayBuffer = Win32GetReplayBuffer(State, InputPlayingIndex);
    if (ReplayBuffer->MemoryBlock == 0) {
        return;
    }

    State->InputPlayingIndex = InputPlayingIndex;

    char Filename[WIN32_STATE_FILE_NAME_COUNT];
    Win32GetInputFileLocation(State, true, InputPlayingIndex, sizeof(Filename), Filename);
    State->PlayingHandle = CreateFileA(
        Filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0
    );

    CopyMemory(State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize);
}

internal void Win32EndInputPlayback(win32_state* Win32State) {
    CloseHandle(Win32State->PlayingHandle);
    Win32State->InputPlayingIndex = 0;
}

internal void Win32RecordInput(win32_state* Win32State, game_input* NewInput) {
    DWORD BytesWritten;
    WriteFile(Win32State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void Win32PlayBackInput(win32_state* Win32State, game_input* NewInput) {
    DWORD BytesRead;
    if (ReadFile(Win32State->PlayingHandle, NewInput, sizeof(*NewInput), &BytesRead, 0) != 0) {
        if (BytesRead == 0) {
            int PlayingIndex = Win32State->InputPlayingIndex;
            Win32EndInputPlayback(Win32State);
            Win32BeginInputPlayback(Win32State, PlayingIndex);
            ReadFile(Win32State->PlayingHandle, NewInput, sizeof(*NewInput), &BytesRead, 0);
        }
    }
}

internal void Win32ProcessPendingMessages(win32_state* Win32State, game_controller_input* KeyboardController) {
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
            } else if (VKCode == VK_SPACE) {
                Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
            } else if (VKCode == VK_ESCAPE) {
                Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
            }
#if HANDMADE_INTERNAL
            else if (VKCode == 'P') {
                if (IsDown) {
                    GlobalPause = !GlobalPause;
                }
            } else if (VKCode == 'L') {
                if (IsDown) {
                    if (Win32State->InputPlayingIndex == 0) {
                        if (Win32State->InputRecordingIndex == 0) {
                            Win32BeginRecordingInput(Win32State, 1);
                        } else {
                            Win32EndRecordingInput(Win32State);
                            Win32BeginInputPlayback(Win32State, 1);
                        }
                    } else {
                        Win32EndInputPlayback(Win32State);
                    }

                }
            }
#endif
            bool32 AltIsDown = (Message.lParam & (1 << 29)) != 0;
            if (AltIsDown && VKCode == VK_F4) {
                GlobalRunning = false;
            }
            if (AltIsDown && VKCode == VK_RETURN && IsDown) {
                ToggleFullscreen(Message.hwnd);
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
# if 0
internal void Wind32DebugDrawVertical(
    win32_offscreen_buffer* BackBuffer, int32 X, int32 Top, int32 Bottom,
    uint32 Color
) {
    if (X < 0 || X >= BackBuffer->Width || Top >= BackBuffer->Height || Bottom < 0) {
        return;
    }
    if (Top < 0) {
        Top = 0;
    }
    if (Bottom >= BackBuffer->Height) {
        Bottom = BackBuffer->Height - 1;
    }
    uint8* Pixel =
        (uint8*)BackBuffer->Memory + X * BackBuffer->BytesPerPixel
        + Top * BackBuffer->Pitch;
    for (int Y = Top; Y < Bottom; ++Y) {
        *(uint32*)Pixel = Color;
        Pixel += BackBuffer->Pitch;
    }
}

internal void Win32DrawMarker(
    DWORD Value,
    win32_offscreen_buffer* BackBuffer,
    int32 PadX, real32 C, int32 Top, int32 Bottom, int32 Color
) {
    int32 X = PadX + (int32)(C * (real32)Value);
    Wind32DebugDrawVertical(BackBuffer, X, Top, Bottom, Color);
}

internal void Win32DebugSyncDisplay(
    win32_offscreen_buffer* BackBuffer,
    int32 MarkerCount, win32_debug_time_marker* LastMarker,
    int32 CurrentMarkerIndex,
    win32_sound_output* SoundOutput, real32 SecondsPerFrame
) {
    int32 PadX = 16;
    int32 PadY = 16;
    int32 LineHeight = 64;
    real32 C =
        ((real32)BackBuffer->Width - 2 * PadX) / (real32)SoundOutput->SecondaryBufferSize;
    uint32 ColorPlay = 0xFFFFFF;
    uint32 ColorWrite = 0xFF0000;
    uint32 ColorExpectedFlip = 0x00FF00;
    uint32 ColorPlayWindow = 0xFF00FF;
    for (int32 MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex) {
        int32 Top = PadY;
        int32 Bottom = PadY + LineHeight;
        win32_debug_time_marker ThisMarker = LastMarker[MarkerIndex];
        if (MarkerIndex == CurrentMarkerIndex) {
            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            Win32DrawMarker(ThisMarker.OutputPlayCursor, BackBuffer, PadX, C, Top, Bottom, ColorPlay);
            Win32DrawMarker(ThisMarker.OutputWriteCursor, BackBuffer, PadX, C, Top, Bottom, ColorWrite);
            Win32DrawMarker(ThisMarker.ExpectedFlipCursor, BackBuffer, PadX, C, Top, Bottom + 2 * (LineHeight + PadY), ColorExpectedFlip);

            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            Win32DrawMarker(ThisMarker.OutputLocation, BackBuffer, PadX, C, Top, Bottom, ColorPlay);
            Win32DrawMarker(ThisMarker.OutputLocation + ThisMarker.OutputByteCount, BackBuffer, PadX, C, Top, Bottom, ColorWrite);

            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;
        }
        Assert((uint32)ThisMarker.FlipPlayCursor < (uint32)SoundOutput->SecondaryBufferSize);
        Assert((uint32)ThisMarker.FlipWriteCursor < (uint32)SoundOutput->SecondaryBufferSize);
        Win32DrawMarker(ThisMarker.FlipPlayCursor, BackBuffer, PadX, C, Top, Bottom, ColorPlay);
        Win32DrawMarker(ThisMarker.FlipPlayCursor + 480 * SoundOutput->BytesPerSample, BackBuffer, PadX, C, Top, Bottom, ColorPlayWindow);
        Win32DrawMarker(ThisMarker.FlipWriteCursor, BackBuffer, PadX, C, Top, Bottom, ColorWrite);
    }
}
#endif

internal void HandleDebugCycleCounters(game_memory* GameMemory) {
#if HANDMADE_INTERNAL
    OutputDebugStringA("DEBUG CYCLE COUNTS:\n");
    for (int32 CounterIndex = 0; CounterIndex < ArrayCount(GameMemory->Counters); CounterIndex++) {
        debug_cycle_counter* Counter = GameMemory->Counters + CounterIndex;
        if (Counter->HitCount) {
            char TextBuffer[256];
            _snprintf_s(TextBuffer, sizeof(TextBuffer), "    %d: %I64ucy %uh %I64ucy/h\n", CounterIndex, Counter->CycleCount, Counter->HitCount, Counter->CycleCount / Counter->HitCount);
            OutputDebugStringA(TextBuffer);
        }
        Counter->CycleCount = 0;
        Counter->HitCount = 0;
    }
#endif
}

struct platform_work_queue_entry {
    platform_work_queue_callback* Callback;
    void* Data;
};

struct platform_work_queue {
    uint32 volatile CompletionGoal;
    uint32 volatile CompletionCount;
    uint32 volatile NextEntryToWrite;
    uint32 volatile NextEntryToRead;
    HANDLE Semaphore;
    platform_work_queue_entry Entries[256];
};

/// Single producer - one thread calls this
internal void
Win32AddEntry(platform_work_queue* Queue, platform_work_queue_callback* Callback, void* Data) {
    uint32 NewNextEntryToWrite = (Queue->NextEntryToWrite + 1) % ArrayCount(Queue->Entries);
    Assert(NewNextEntryToWrite != Queue->NextEntryToRead);
    platform_work_queue_entry* Entry = Queue->Entries + Queue->NextEntryToWrite;
    Entry->Data = Data;
    Entry->Callback = Callback;
    ++Queue->CompletionGoal;
    _WriteBarrier();
    Queue->NextEntryToWrite = NewNextEntryToWrite;
    ReleaseSemaphore(Queue->Semaphore, 1, 0);
}

internal bool32 Win32DoNextWorkQueueEntry(platform_work_queue* Queue) {
    bool32 ShouldSleep = false;
    uint32 OriginalNextentryToRead = Queue->NextEntryToRead;
    uint32 NewNextEntryToRead = (OriginalNextentryToRead + 1) % ArrayCount(Queue->Entries);
    if (OriginalNextentryToRead != Queue->NextEntryToWrite) {
        uint32 Index = InterlockedCompareExchange(
            (LONG volatile*)&Queue->NextEntryToRead,
            NewNextEntryToRead,
            OriginalNextentryToRead
        );
        if (Index == OriginalNextentryToRead) {
            platform_work_queue_entry Entry = Queue->Entries[Index];
            Entry.Callback(Queue, Entry.Data);
            InterlockedIncrement((LONG volatile*)&Queue->CompletionCount);
        }
    } else {
        ShouldSleep = true;
    }
    return ShouldSleep;
}

internal PLATFORM_WORK_QUEUE_CALLBACK(DoWorkerWork) {
    char Buffer[256];
    wsprintf(Buffer, "Thread %u: %s\n", GetCurrentThreadId(), (char*)Data);
    OutputDebugStringA(Buffer);
}

internal void Win32CompleteAllWork(platform_work_queue* Queue) {
    while (Queue->CompletionGoal != Queue->CompletionCount) {
        Win32DoNextWorkQueueEntry(Queue);
    }
    Queue->CompletionCount = 0;
    Queue->CompletionGoal = 0;
}

DWORD WINAPI ThreadProc(LPVOID lpParameter) {
    platform_work_queue* Queue = (platform_work_queue*)lpParameter;
    for (;;) {
        if (Win32DoNextWorkQueueEntry(Queue)) {
            WaitForSingleObjectEx(Queue->Semaphore, INFINITE, FALSE);
        }
    }
}

internal void Win32MakeQueue(platform_work_queue* Queue, uint32 ThreadCount) {
    Queue->CompletionCount = 0;
    Queue->CompletionGoal = 0;
    Queue->NextEntryToRead = 0;
    Queue->NextEntryToWrite = 0;
    uint32 InitialCount = 0;
    Queue->Semaphore = CreateSemaphoreExA(0, InitialCount, ThreadCount, 0, 0, SEMAPHORE_ALL_ACCESS);
    char* Param = "Thread Started\n";
    for (uint32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex) {
        DWORD ThreadId;
        HANDLE ThreadHandle = CreateThread(0, 0, ThreadProc, Queue, 0, &ThreadId);
        CloseHandle(ThreadHandle);
    }
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode) {

    win32_state Win32State = {};

    platform_work_queue HighPriorityQueue;
    Win32MakeQueue(&HighPriorityQueue, 6);
    platform_work_queue LowPriorityQueue;
    Win32MakeQueue(&LowPriorityQueue, 2);

#if 0
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "S 0000\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "S 1111\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "S 2222\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "S 3333\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "S 4444\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "S 5555\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "S 6666\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "S 7777\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "S 8888\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "S 9999\n");

    Sleep(1000);

    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "SS 0000\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "SS 1111\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "SS 2222\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "SS 3333\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "SS 4444\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "SS 5555\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "SS 6666\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "SS 7777\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "SS 8888\n");
    Win32AddEntry(&HighPriorityQueue, DoWorkerWork, "SS 9999\n");

    Win32CompleteAllWork(&HighPriorityQueue);
#endif

    Win32GetExeFileName(&Win32State);

    char GameCodeDLLFullPathSource[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildExePathFilename(&Win32State, "game_lib.dll", sizeof(GameCodeDLLFullPathSource), GameCodeDLLFullPathSource);

    char GameCodeDLLFullPathLocked[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildExePathFilename(&Win32State, "game_lib.lock.dll", sizeof(GameCodeDLLFullPathLocked), GameCodeDLLFullPathLocked);

    LARGE_INTEGER PerfCounterFrequencyResult;
    QueryPerformanceFrequency(&PerfCounterFrequencyResult);
    GlobalPerfCounterFrequency = PerfCounterFrequencyResult.QuadPart;

    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR;

    Win32LoadXInput();

#if HANDMADE_INTERNAL
    DEBUGGlobalShowCursor = true;
#endif
    WNDCLASSA WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackBuffer, 1920, 1080);

    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);

    RegisterClassA(&WindowClass);

    HWND Window = CreateWindowExA(
        0, //WS_EX_TOPMOST | WS_EX_LAYERED,
        WindowClass.lpszClassName,
        "Handmade hero",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        GlobalBackBuffer.Width + 50,
        GlobalBackBuffer.Height + 100,
        0,
        0,
        Instance,
        0
    );

#if 0
    int32 MonitorRefreshHz = 60;
    HDC RefreshDC = GetDC(Window);
    int32 Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
    ReleaseDC(Window, RefreshDC);
    if (Win32RefreshRate > 1) {
        MonitorRefreshHz = Win32RefreshRate;
    }
#endif

    real32 GameRefreshHz = 60;
    real32 TargetSecondsPerFrame = 1.0f / GameRefreshHz;

    GlobalRunning = true;
    GlobalPause = false;

    int XOffset = 0;
    int YOffset = 0;

    //* Sample is both left/right (32 bits)
    win32_sound_output SoundOutput = {};

    SoundOutput.SamplesPerSecond = 48000;
    SoundOutput.BytesPerSample = sizeof(int16) * 2;
    SoundOutput.SafetyBytes =
        (int32)((real32)SoundOutput.SamplesPerSecond * (real32)SoundOutput.BytesPerSample / 10);
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
    GameMemory.PermanentStorageSize = Megabytes(256);
    GameMemory.TransientStorageSize = Gigabytes(1);

    Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;

    Win32State.GameMemoryBlock = VirtualAlloc(
        BaseAddress, Win32State.TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE
    );
    GameMemory.PermanentStorage = Win32State.GameMemoryBlock;

    GameMemory.TransientStorage =
        (uint8*)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize;

    for (int32 ReplayIndex = 0; ReplayIndex < ArrayCount(Win32State.ReplayBuffers); ++ReplayIndex) {
        win32_replay_buffer* ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];

        Win32GetInputFileLocation(&Win32State, false, ReplayIndex, sizeof(ReplayBuffer->ReplayFilename), ReplayBuffer->ReplayFilename);
        ReplayBuffer->FileHandle = CreateFileA(
            ReplayBuffer->ReplayFilename, GENERIC_WRITE | GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0
        );

        LARGE_INTEGER MaxSize;
        MaxSize.QuadPart = Win32State.TotalSize;

        ReplayBuffer->MemoryMap = CreateFileMapping(
            ReplayBuffer->FileHandle, 0, PAGE_READWRITE,
            MaxSize.HighPart, MaxSize.LowPart, 0
        );
        ReplayBuffer->MemoryBlock = MapViewOfFile(
            ReplayBuffer->MemoryMap,
            FILE_MAP_ALL_ACCESS, 0, 0, Win32State.TotalSize
        );
    }

    GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
    GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
    GameMemory.DEBUGPlatformReadEntireFile = DEBUGWin32ReadEntireFile;
    GameMemory.PlatformAddEntry = Win32AddEntry;
    GameMemory.PlatformCompleteAllWork = Win32CompleteAllWork;
    GameMemory.HighPriorityQueue = &HighPriorityQueue;
    GameMemory.LowPriorityQueue = &LowPriorityQueue;

    //* Timing

    LARGE_INTEGER LastCounter = Win32GetWallClock();
    LARGE_INTEGER FlipWallClock = {};

    uint64 LastCycleCount = __rdtsc();

    int32 DebugLastMarkerIndex = 0;
    win32_debug_time_marker DebugLastMarker[30] = {};
    DWORD AudioLatencyBytes = 0;
    real32 AudioLatencySeconds = 0;
    bool32 SoundIsValid = false;

    win32_game_code GameCode = Win32LoadGameCode(GameCodeDLLFullPathSource, GameCodeDLLFullPathLocked);

    while (GlobalRunning) {

        NewInput->ExecutableReloaded = false;

        FILETIME NewDLLWriteTime = Win32GetLastWriteTime(GameCodeDLLFullPathSource);
        if (CompareFileTime(&NewDLLWriteTime, &GameCode.LastWriteTime) != 0) {
            Win32UnloadCode(&GameCode);
            GameCode = Win32LoadGameCode(GameCodeDLLFullPathSource, GameCodeDLLFullPathLocked);
            NewInput->ExecutableReloaded = true;
        }

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

        Win32ProcessPendingMessages(&Win32State, NewKeyboardController);

        if (GlobalPause) {
            continue;
        }

        NewInput->dtForFrame = TargetSecondsPerFrame;

        //* Mouse
        POINT MouseP;
        GetCursorPos(&MouseP);
        ScreenToClient(Window, &MouseP);
        NewInput->MouseX = MouseP.x;
        NewInput->MouseY = MouseP.y;
        NewInput->MouseZ = 0;
        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0], GetKeyState(VK_LBUTTON) & (1 << 15));
        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1], GetKeyState(VK_RBUTTON) & (1 << 15));
        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2], GetKeyState(VK_MBUTTON) & (1 << 15));
        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
        Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));

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
                NewController->IsAnalog = OldController->IsAnalog;
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

        //* Game

        game_offscreen_buffer GraphicsBuffer = {};
        GraphicsBuffer.Memory = GlobalBackBuffer.Memory;
        GraphicsBuffer.Width = GlobalBackBuffer.Width;
        GraphicsBuffer.Height = GlobalBackBuffer.Height;
        GraphicsBuffer.Pitch = GlobalBackBuffer.Pitch;

        if (Win32State.InputRecordingIndex) {
            Win32RecordInput(&Win32State, NewInput);
        }
        if (Win32State.InputPlayingIndex) {
            Win32PlayBackInput(&Win32State, NewInput);
        }
        if (GameCode.UpdateAndRender != 0) {
            GameCode.UpdateAndRender(&GameMemory, NewInput, &GraphicsBuffer);
            HandleDebugCycleCounters(&GameMemory);
        }

        //* Input

        game_input* Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;

        //* Sound

        LARGE_INTEGER AudioWallClock = Win32GetWallClock();
        real32 SecSinceFlip = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);

        DWORD PlayCursor = 0;
        DWORD WriteCursor = 0;
        if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK) {

            if (!SoundIsValid) {
                SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                SoundIsValid = true;
            }

            DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
                SoundOutput.SecondaryBufferSize;

            DWORD ExpectedSoundBytesPerFrame =
                (int32)((real32)SoundOutput.SamplesPerSecond * (real32)SoundOutput.BytesPerSample / GameRefreshHz);
            DWORD ExpectedBytesUntilFlip = (DWORD)((TargetSecondsPerFrame - SecSinceFlip) *
                (real32)(SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample));
            DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip;

            DWORD SafeWriteCursor = WriteCursor;
            if (SafeWriteCursor < PlayCursor) {
                SafeWriteCursor += SoundOutput.SecondaryBufferSize;
            }
            Assert(SafeWriteCursor >= PlayCursor);
            SafeWriteCursor += SoundOutput.SafetyBytes;

            bool32 AudioIsLowLatency = SafeWriteCursor < ExpectedFrameBoundaryByte;

            DWORD TargetCursor = 0;
            if (AudioIsLowLatency) {
                TargetCursor = ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;
            } else {
                TargetCursor = WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes;

            }
            TargetCursor = TargetCursor % SoundOutput.SecondaryBufferSize;

            DWORD BytesToWrite = 0;
            if (ByteToLock > TargetCursor) {
                BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock) + TargetCursor;
            } else {
                BytesToWrite = TargetCursor - ByteToLock;
            }

            game_sound_buffer SoundBuffer = {};
            SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
            SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
            SoundBuffer.Samples = Samples;
            if (GameCode.GetSoundSamples != 0) {
                GameCode.GetSoundSamples(&GameMemory, &SoundBuffer);
            }

#if HANDMADE_INTERNAL
            win32_debug_time_marker* Marker = &DebugLastMarker[DebugLastMarkerIndex];
            Marker->OutputPlayCursor = PlayCursor;
            Marker->OutputWriteCursor = WriteCursor;
            Marker->OutputLocation = ByteToLock;
            Marker->OutputByteCount = BytesToWrite;
            Marker->ExpectedFlipCursor = ExpectedFrameBoundaryByte;

            if (WriteCursor > PlayCursor) {
                AudioLatencyBytes = WriteCursor - PlayCursor;
            } else {
                AudioLatencyBytes =
                    SoundOutput.SecondaryBufferSize - PlayCursor + WriteCursor;
            }

            AudioLatencySeconds =
                (real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample /
                (real32)SoundOutput.SamplesPerSecond;
#if 0
            char SoundTextBuffer[256];
            sprintf_s(
                SoundTextBuffer,
                "LPC: %u; BTL: %u, TC: %u, BTW: %u - PC: %u, WC: %u\n",
                PlayCursor, ByteToLock, TargetCursor, BytesToWrite,
                PlayCursor, WriteCursor
            );
            OutputDebugStringA(SoundTextBuffer);
#endif
#endif
            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
        } else {
            SoundIsValid = false;
        }


        //* Timing

        real32 SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
        if (SecondsElapsedForFrame < TargetSecondsPerFrame) {
            if (SleepIsGranular) {
                DWORD SleepMS =
                    (DWORD)((TargetSecondsPerFrame - SecondsElapsedForFrame) * 1000.0f);
                if (SleepMS > 0) {
                    Sleep(SleepMS);
                }
            }
            SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
#if 0
            Assert(SecondsElapsedForFrame <= TargetSecondsPerFrame);
#endif
            while (SecondsElapsedForFrame < TargetSecondsPerFrame) {
                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
            }
        } else {
            //* MISSED FRAME
        }
        LastCounter = Win32GetWallClock();

        //* Display

        win32_window_dimension Dim = Win32GetWindowDimension(Window);
#if 0
        Win32DebugSyncDisplay(
            &GlobalBackBuffer,
            ArrayCount(DebugLastMarker), DebugLastMarker,
            DebugLastMarkerIndex == 0 ? ArrayCount(DebugLastMarker) - 1 : DebugLastMarkerIndex - 1,
            &SoundOutput, TargetSecondsPerFrame
        );
#endif
        HDC DeviceContext = GetDC(Window);
        Win32DisplayBufferInWindow(
            DeviceContext,
            Dim.Width, Dim.Height,
            &GlobalBackBuffer
        );
        ReleaseDC(Window, DeviceContext);
        FlipWallClock = Win32GetWallClock();

        //* Debug sound
#if HANDMADE_INTERNAL
        {
            DWORD FlipPlayCursor = 0;
            DWORD FlipWriteCursor = 0;
            if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&FlipPlayCursor, &FlipWriteCursor))) {
                Assert(DebugLastMarkerIndex < ArrayCount(DebugLastMarker));
                win32_debug_time_marker* Marker = &DebugLastMarker[DebugLastMarkerIndex];
                Marker->FlipPlayCursor = FlipPlayCursor;
                Marker->FlipWriteCursor = FlipWriteCursor;
                DebugLastMarkerIndex++;
                if (DebugLastMarkerIndex == ArrayCount(DebugLastMarker)) {
                    DebugLastMarkerIndex = 0;
                }
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
