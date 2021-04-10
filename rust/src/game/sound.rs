use crate::Result;

pub trait Buffer {
    fn write(&mut self, samples: &[i16]) -> Result<()>;
    fn get_samples_per_second_per_channel(&self) -> u32;
    fn get_n_channels(&self) -> u32;
}

pub fn play_sinewave<B: Buffer>(buffer: &mut B) -> Result<()> {
    buffer.write(&[])?;
    /*static mut T_SINE: f32 = 0.0;
    let volume = 2000;
    let tone_hz = 256;
    let wave_period = buffer.get_samples_per_second() / tone_hz;
    let n_channels = buffer.get_n_channels();
    let samples = buffer.get_mut_samples()?;
    let sample_count = samples.len() as u32 / n_channels;

    let mut ind_sample_index = 0;
    for _ in 0..sample_count {
        let sine_value = unsafe { T_SINE.sin() };
        let sample_value = (sine_value * volume as f32) as i16;

        for _ in 0..n_channels {
            samples[ind_sample_index] = sample_value;
            ind_sample_index += 1;
        }

        unsafe { T_SINE += 2.0f32 * std::f32::consts::PI * 1.0f32 / wave_period as f32 };
    }*/

    Ok(())
}
