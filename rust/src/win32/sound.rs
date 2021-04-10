use crate::{game, Result};
use anyhow::bail;
use anyhow::Context;
use std::{mem, ptr};
use winapi::shared::{mmreg::WAVEFORMATEX, windef::HWND, winerror::SUCCEEDED};
use winapi::um::dsound::{IDirectSound, IDirectSoundBuffer, DSBUFFERDESC};

#[derive(Debug)]
pub struct Info {
    pub n_channels: u32,
    pub samples_per_second_per_channel: u32,
    pub sample_size_bytes_per_channel: u32,
    pub sample_size_bytes_all_channels: u32,
    pub buffer_size_seconds: u32,
    pub buffer_size_samples_per_channel: u32,
    pub buffer_size_bytes_all_channels: u32,
    pub latency_size_samples_per_channel: u32,
    pub latency_size_bytes_all_channels: u32,
}

impl Info {
    pub fn new(
        n_channels: u32,
        samples_per_second_per_channel: u32,
        sample_size_bytes_per_channel: u32,
        buffer_size_seconds: u32,
        latency_size_samples_per_channel: u32,
    ) -> Self {
        Self {
            n_channels,
            samples_per_second_per_channel,
            sample_size_bytes_per_channel,
            sample_size_bytes_all_channels: sample_size_bytes_per_channel * n_channels,
            buffer_size_seconds,
            buffer_size_samples_per_channel: buffer_size_seconds * samples_per_second_per_channel,
            buffer_size_bytes_all_channels: buffer_size_seconds
                * samples_per_second_per_channel
                * sample_size_bytes_per_channel
                * n_channels,
            latency_size_samples_per_channel,
            latency_size_bytes_all_channels: latency_size_samples_per_channel
                * sample_size_bytes_per_channel
                * n_channels,
        }
    }
}

pub struct Buffer<'a> {
    info: Info,
    secondary: &'a IDirectSoundBuffer,
    current_byte: u32,
}

impl<'a> game::sound::Buffer for Buffer<'a> {
    fn write(&mut self, _samples: &[i16]) -> Result<()> {
        let pos = get_current_position(self.secondary)?;

        let to = (pos.write + self.info.latency_size_bytes_all_channels)
            % self.info.buffer_size_bytes_all_channels;

        fill_buffer(
            self.secondary,
            self.current_byte,
            self.get_n_bytes_beween(to, self.current_byte),
            self.info.sample_size_bytes_all_channels,
        )?;

        self.current_byte = to;
        Ok(())
    }
    fn get_samples_per_second_per_channel(&self) -> u32 {
        self.info.samples_per_second_per_channel
    }
    fn get_n_channels(&self) -> u32 {
        self.info.n_channels
    }
}

impl<'a> Buffer<'a> {
    pub fn new(window: HWND) -> Result<Self> {
        let direct_sound = create_direct_sound(window)?;

        let info = Info::new(2, 48000, mem::size_of::<i16>() as u32, 1, 3200);

        let mut wave_format = create_wave_format(
            info.samples_per_second_per_channel,
            info.n_channels,
            info.sample_size_bytes_per_channel,
        );

        set_primary_format(direct_sound, &wave_format)?;

        let secondary_buffer = create_secondary_buffer(
            direct_sound,
            &mut wave_format,
            info.buffer_size_bytes_all_channels,
        )?;

        Ok(Self {
            info,
            secondary: secondary_buffer,
            current_byte: 0,
        })
    }

    pub fn play(&self) -> Result<()> {
        use winapi::um::dsound::DSBPLAY_LOOPING;
        unsafe {
            let code = self.secondary.Play(0, 0, DSBPLAY_LOOPING);
            if !SUCCEEDED(code) {
                bail!("failed to play sound");
            }
        };
        Ok(())
    }

    /// Like a - b but accounts for a and b being byte positions in a ring buffer
    fn get_n_bytes_beween(&self, a: u32, b: u32) -> u32 {
        if a > b {
            a - b
        } else {
            self.info.buffer_size_bytes_all_channels - b + a
        }
    }
}

fn create_wave_format(
    samples_per_second_per_channel: u32,
    n_channels: u32,
    bytes_per_sample_per_channel: u32,
) -> WAVEFORMATEX {
    use winapi::shared::mmreg::WAVE_FORMAT_PCM;

    let n_block_align = n_channels * bytes_per_sample_per_channel;
    let n_avg_bytes_per_sec = n_block_align * samples_per_second_per_channel;
    WAVEFORMATEX {
        cbSize: 0,
        nAvgBytesPerSec: n_avg_bytes_per_sec,
        nBlockAlign: n_block_align as u16,
        nChannels: n_channels as u16,
        nSamplesPerSec: samples_per_second_per_channel,
        wBitsPerSample: (bytes_per_sample_per_channel * 8) as u16,
        wFormatTag: WAVE_FORMAT_PCM,
    }
}

fn create_direct_sound<'a>(window: HWND) -> Result<&'a IDirectSound> {
    use winapi::um::dsound::{DirectSoundCreate, DSSCL_PRIORITY};

    let direct_sound = unsafe {
        let mut direct_sound = mem::zeroed();
        if !SUCCEEDED(DirectSoundCreate(
            ptr::null_mut(),
            &mut direct_sound,
            ptr::null_mut(),
        )) {
            bail!("failed to create direct sound");
        }
        let direct_sound = direct_sound.as_ref().unwrap();
        let code = direct_sound.SetCooperativeLevel(window, DSSCL_PRIORITY);
        if !SUCCEEDED(code) {
            bail!("failed to set cooperative level: {:#X}", code);
        }
        direct_sound
    };

    Ok(direct_sound)
}

fn create_dsound_buffer<'a>(
    direct_sound: &IDirectSound,
    desc: &DSBUFFERDESC,
) -> Result<&'a IDirectSoundBuffer> {
    use winapi::um::dsound::LPDIRECTSOUNDBUFFER;

    let buffer = unsafe {
        let mut buffer: LPDIRECTSOUNDBUFFER = ptr::null_mut();
        let code = direct_sound.CreateSoundBuffer(desc, &mut buffer, ptr::null_mut());
        if !SUCCEEDED(code) {
            bail!("failed to create buffer: {:#X}", code);
        }
        buffer.as_ref().unwrap()
    };

    Ok(buffer)
}

fn set_primary_format(direct_sound: &IDirectSound, format: &WAVEFORMATEX) -> Result<()> {
    use winapi::um::dsound::DSBCAPS_PRIMARYBUFFER;

    let mut primary_buffer_desc: DSBUFFERDESC = unsafe { mem::zeroed() };
    primary_buffer_desc.dwSize = mem::size_of::<DSBUFFERDESC>() as u32;
    primary_buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

    let primary_buffer = create_dsound_buffer(direct_sound, &primary_buffer_desc)?;
    let code = unsafe { (*primary_buffer).SetFormat(format) };
    if !SUCCEEDED(code) {
        bail!("failed to set primary format");
    }
    Ok(())
}

fn create_secondary_buffer<'a>(
    direct_sound: &IDirectSound,
    format: &mut WAVEFORMATEX,
    size: u32,
) -> Result<&'a IDirectSoundBuffer> {
    let mut secondary_buffer_desc: DSBUFFERDESC = unsafe { mem::zeroed() };
    secondary_buffer_desc.dwSize = mem::size_of::<DSBUFFERDESC>() as u32;
    secondary_buffer_desc.dwFlags = 0;
    secondary_buffer_desc.dwBufferBytes = size;
    secondary_buffer_desc.lpwfxFormat = format;

    let secondary_buffer = create_dsound_buffer(direct_sound, &secondary_buffer_desc)
        .context("failed to create secondary buffer")?;

    clear_buffer(secondary_buffer, size)?;

    Ok(secondary_buffer)
}

struct LockResult {
    region1: *mut i16,
    region1_bytes: u32,
    region2: *mut i16,
    region2_bytes: u32,
}

fn lock_buffer(
    buffer: &IDirectSoundBuffer,
    first_byte: u32,
    bytes_to_write: u32,
) -> Result<LockResult> {
    let lock_result = unsafe {
        let mut region1 = mem::zeroed();
        let mut region1_bytes = mem::zeroed();
        let mut region2 = mem::zeroed();
        let mut region2_bytes = mem::zeroed();
        let code = buffer.Lock(
            first_byte,
            bytes_to_write,
            &mut region1,
            &mut region1_bytes,
            &mut region2,
            &mut region2_bytes,
            0,
        );
        if !SUCCEEDED(code) {
            bail!("failed to lock buffer: {:#X}", code);
        }
        LockResult {
            region1: region1.cast(),
            region1_bytes,
            region2: region2.cast(),
            region2_bytes,
        }
    };
    Ok(lock_result)
}

fn unlock_buffer(buffer: &IDirectSoundBuffer, lock_result: LockResult) -> Result<()> {
    unsafe {
        let code = buffer.Unlock(
            lock_result.region1.cast(),
            lock_result.region1_bytes,
            lock_result.region2.cast(),
            lock_result.region2_bytes,
        );
        if !SUCCEEDED(code) {
            bail!("failed to unlock buffer for clearing");
        }
    }
    Ok(())
}

fn clear_buffer(buffer: &IDirectSoundBuffer, size: u32) -> Result<()> {
    let lock_result = lock_buffer(buffer, 0, size)?;
    /*unsafe {
        let mut dest_byte = lock_result.region1;
        for _ in 0..lock_result.region1_size {
            *dest_byte = 0;
            dest_byte = dest_byte.add(1);
        }

        dest_byte = lock_result.region2;
        for _ in 0..lock_result.region2_size {
            *dest_byte = 0;
            dest_byte = dest_byte.add(1);
        }
    }*/
    unlock_buffer(buffer, lock_result)?;
    Ok(())
}

#[derive(Debug)]
struct Position {
    pub play: u32,
    pub write: u32,
}

fn get_current_position(buffer: &IDirectSoundBuffer) -> Result<Position> {
    let pos = unsafe {
        let mut play_cursor = mem::zeroed();
        let mut write_cursor = mem::zeroed();
        let code = buffer.GetCurrentPosition(&mut play_cursor, &mut write_cursor);
        if !SUCCEEDED(code) {
            bail!("failed to get current position");
        }
        Position {
            play: play_cursor,
            write: write_cursor,
        }
    };
    Ok(pos)
}

fn fill_buffer(
    buffer: &IDirectSoundBuffer,
    first_byte: u32,
    bytes_to_write: u32,
    bytes_per_sample_all_channels: u32,
) -> Result<()> {
    let lock_result = lock_buffer(buffer, first_byte, bytes_to_write)?;
    fill_buffer_region(
        lock_result.region1,
        lock_result.region1_bytes / bytes_per_sample_all_channels,
    );
    fill_buffer_region(
        lock_result.region2,
        lock_result.region2_bytes / bytes_per_sample_all_channels,
    );
    unlock_buffer(buffer, lock_result)?;
    Ok(())
}

fn fill_buffer_region(start: *mut i16, sample_count_for_each_channel: u32) {
    static mut T_SINE: f32 = 0.0;
    let volume = 2000;
    let tone_hz = 256;
    let wave_period = 48000 / tone_hz;

    let mut dest = start;
    unsafe {
        for _ in 0..sample_count_for_each_channel {
            let sine_value = T_SINE.sin();
            let sample_value = (sine_value * volume as f32) as i16;
            *dest = sample_value;
            dest = dest.add(1);
            *dest = sample_value;
            dest = dest.add(1);
            T_SINE = (T_SINE + 2.0f32 * std::f32::consts::PI / wave_period as f32)
                % (2f32 * std::f32::consts::PI);
        }
    }
}
