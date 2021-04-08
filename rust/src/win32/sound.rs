use crate::{game, Result};
use anyhow::bail;
use anyhow::Context;
use std::{mem, ptr};
use winapi::shared::{mmreg::WAVEFORMATEX, windef::HWND, winerror::SUCCEEDED};
use winapi::um::dsound::{IDirectSound, IDirectSoundBuffer, DSBUFFERDESC};

pub struct Buffer<'a> {
    samples_per_second: u32,
    n_channels: u32,
    secondary: &'a IDirectSoundBuffer,
    size: u32,
    _latency_sample_count: u32,
}

impl<'a> game::sound::Buffer for Buffer<'a> {
    fn write(&mut self, _samples: &[i16]) -> Result<()> {
        let first_byte = 0;
        let bytes_to_write = self.size;
        let lock_result = lock_buffer(self.secondary, first_byte, bytes_to_write)
            .context("failed to lock buffer for writing")?;

        unlock_buffer(self.secondary, lock_result)?;
        Ok(())
    }
    fn get_samples_per_second(&self) -> u32 {
        self.samples_per_second
    }
    fn get_n_channels(&self) -> u32 {
        self.n_channels
    }
}

impl<'a> Buffer<'a> {
    pub fn new(window: HWND) -> Result<Self> {
        let samples_per_second = 48000;
        let n_channels = 2;
        let latency_sample_count = samples_per_second / 15;
        let bytes_per_sample = (mem::size_of::<i16>() * n_channels) as u32;
        let secondary_buffer_size = samples_per_second * bytes_per_sample;

        let mut wave_format = create_wave_format(samples_per_second);

        let direct_sound = create_direct_sound(window)?;

        set_primary_format(direct_sound, &wave_format)?;

        let secondary_buffer =
            create_secondary_buffer(direct_sound, &mut wave_format, secondary_buffer_size)?;

        Ok(Self {
            samples_per_second,
            n_channels: n_channels as u32,
            secondary: secondary_buffer,
            size: secondary_buffer_size,
            _latency_sample_count: latency_sample_count,
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
}

fn create_wave_format(samples_per_second: u32) -> WAVEFORMATEX {
    use winapi::shared::mmreg::WAVE_FORMAT_PCM;

    let n_channels = 2;
    let bits_per_sample = 16;
    let n_block_align = n_channels * bits_per_sample / 8;
    let n_avg_bytes_per_sec = n_block_align * samples_per_second;
    WAVEFORMATEX {
        cbSize: 0,
        nAvgBytesPerSec: n_avg_bytes_per_sec,
        nBlockAlign: n_block_align as u16,
        nChannels: n_channels as u16,
        nSamplesPerSec: samples_per_second,
        wBitsPerSample: bits_per_sample as u16,
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
    region1: *mut u8,
    region1_size: u32,
    region2: *mut u8,
    region2_size: u32,
}

fn lock_buffer(buffer: &IDirectSoundBuffer, start: u32, size: u32) -> Result<LockResult> {
    let lock_result = unsafe {
        let mut region1 = mem::zeroed();
        let mut region1_size = mem::zeroed();
        let mut region2 = mem::zeroed();
        let mut region2_size = mem::zeroed();
        let code = buffer.Lock(
            start,
            size,
            &mut region1,
            &mut region1_size,
            &mut region2,
            &mut region2_size,
            0,
        );
        if !SUCCEEDED(code) {
            bail!("failed to lock buffer: {:#X}", code);
        }
        LockResult {
            region1: region1.cast(),
            region1_size,
            region2: region2.cast(),
            region2_size,
        }
    };
    Ok(lock_result)
}

fn unlock_buffer(buffer: &IDirectSoundBuffer, lock_result: LockResult) -> Result<()> {
    unsafe {
        let code = buffer.Unlock(
            lock_result.region1.cast(),
            lock_result.region1_size,
            lock_result.region2.cast(),
            lock_result.region2_size,
        );
        if !SUCCEEDED(code) {
            bail!("failed to unlock buffer for clearing");
        }
    }
    Ok(())
}

fn clear_buffer(buffer: &IDirectSoundBuffer, size: u32) -> Result<()> {
    let lock_result = lock_buffer(buffer, 0, size)?;
    unsafe {
        let dest_byte = lock_result.region1;
        for _ in 0..lock_result.region1_size {
            *dest_byte = 0;
            *dest_byte += 1;
        }

        let dest_byte = lock_result.region2;
        for _ in 0..lock_result.region2_size {
            *dest_byte = 0;
            *dest_byte += 1;
        }
    }
    unlock_buffer(buffer, lock_result)?;
    Ok(())
}
