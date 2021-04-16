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

pub struct NewSamples {
    storage: Vec<i16>,
    info: Option<NewSamplesInfo>,
}

impl NewSamples {
    pub fn new(size: usize) -> Self {
        Self {
            storage: vec![0; size],
            info: None,
        }
    }
    pub fn get_mut(&mut self) -> Option<&mut [i16]> {
        let info = self.info.as_ref()?;
        Some(&mut self.storage[0..info.size_samples_all_channels as usize])
    }
}

pub struct NewSamplesInfo {
    target: u32,
    size_bytes: u32,
    size_samples_all_channels: u32,
}

pub struct Buffer<'a> {
    info: Info,
    secondary: &'a IDirectSoundBuffer,
    current_byte: u32,
    new_samples: NewSamples,
}

impl<'a> game::sound::Buffer for Buffer<'a> {
    fn get_new_samples_mut(&mut self) -> Result<&mut [i16]> {
        let pos = get_current_position(self.secondary)?;

        let to = (pos.play + self.info.latency_size_bytes_all_channels)
            % self.info.buffer_size_bytes_all_channels;

        let bytes_to_write = self.get_n_bytes_beween(to, self.current_byte);
        let samples_all_channels = bytes_to_write / self.info.sample_size_bytes_per_channel;

        self.new_samples.info = Some(NewSamplesInfo {
            target: to,
            size_bytes: bytes_to_write,
            size_samples_all_channels: samples_all_channels,
        });

        Ok(self.new_samples.get_mut().unwrap())
    }

    fn get_samples_per_second_per_channel(&self) -> u32 {
        self.info.samples_per_second_per_channel
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
            secondary: secondary_buffer,
            current_byte: 0,
            new_samples: NewSamples::new(
                (info.samples_per_second_per_channel * info.buffer_size_seconds * info.n_channels)
                    as usize,
            ),
            info,
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

    pub fn play_new_samples(&mut self) -> Result<()> {
        if self.new_samples.info.is_none() {
            return Ok(());
        }

        let new_samples_size = self.new_samples.info.as_ref().unwrap().size_bytes;

        fill_buffer(
            self.secondary,
            self.new_samples.get_mut().unwrap(),
            self.current_byte,
            new_samples_size,
            self.info.sample_size_bytes_per_channel,
        )?;

        self.current_byte = self.new_samples.info.as_ref().unwrap().target;
        self.new_samples.info = None;
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
    unsafe {
        let mut dest_byte = lock_result.region1.cast::<u8>();
        for _ in 0..lock_result.region1_bytes {
            *dest_byte = 0;
            dest_byte = dest_byte.add(1);
        }

        dest_byte = lock_result.region2.cast::<u8>();
        for _ in 0..lock_result.region2_bytes {
            *dest_byte = 0;
            dest_byte = dest_byte.add(1);
        }
    }
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
    samples: &mut [i16],
    first_byte: u32,
    bytes_to_write: u32,
    bytes_per_sample_per_channel: u32,
) -> Result<()> {
    let lock_result = lock_buffer(buffer, first_byte, bytes_to_write)?;
    let region1_sample_count_all_channels =
        lock_result.region1_bytes / bytes_per_sample_per_channel;
    fill_buffer_region(
        samples,
        lock_result.region1,
        region1_sample_count_all_channels,
    );
    let total_samples = samples.len();
    fill_buffer_region(
        &mut samples[region1_sample_count_all_channels as usize..total_samples],
        lock_result.region2,
        lock_result.region2_bytes / bytes_per_sample_per_channel,
    );
    unlock_buffer(buffer, lock_result)?;
    Ok(())
}

fn fill_buffer_region(samples: &mut [i16], start: *mut i16, sample_count_all_channels: u32) {
    let mut dest = start;
    unsafe {
        for i in 0..sample_count_all_channels {
            *dest = samples[i as usize];
            dest = dest.add(1);
        }
    }
}
