use crate::{game, Result};
use winapi::shared::mmreg::WAVEFORMATEX;

pub struct Buffer {
    samples_per_second: u32,
}

impl game::sound::Buffer for Buffer {
    fn get_samples(&self) -> &[i16] {
        &[]
    }
    fn get_mut_samples(&mut self) -> &mut [i16] {
        &mut []
    }
    fn get_samples_per_second(&self) -> u32 {
        self.samples_per_second
    }
}

impl Buffer {
    pub fn new() -> Result<Self> {
        let samples_per_second = 48000;

        let mut _wave_format = create_wave_format(samples_per_second);

        Ok(Self { samples_per_second })
    }
}

fn create_wave_format(samples_per_second: u32) -> WAVEFORMATEX {
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
        wFormatTag: 0,
    }
}
