use crate::Result;

pub trait Buffer {
    fn get_new_samples_mut(&mut self) -> Result<(&mut [i16], &mut [i16])>;
    fn play_new_samples(&mut self) -> Result<()>;
    fn get_samples_per_second_per_channel(&self) -> u32;
}

pub fn play_sinewave<B: Buffer>(buffer: &mut B) -> Result<()> {
    static mut T_SINE: f32 = 0.0;
    let volume = 2000;
    let tone_hz = 256;
    let wave_period = buffer.get_samples_per_second_per_channel() / tone_hz;

    let (left, right) = buffer.get_new_samples_mut()?;

    for i in 0..left.len() {
        let sine_value = unsafe { T_SINE.sin() };
        let sample_value = (sine_value * volume as f32) as i16;

        left[i] = sample_value;
        right[i] = sample_value;

        let two_pi = 2.0f32 * std::f32::consts::PI;

        unsafe { T_SINE = (T_SINE + two_pi / wave_period as f32) % two_pi };
    }

    buffer.play_new_samples()?;

    Ok(())
}
