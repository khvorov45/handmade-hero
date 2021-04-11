use crate::{game, Result};

pub trait Buffer {
    fn get_new_samples_mut(&mut self) -> Result<(&mut [i16], &mut [i16])>;
    fn get_samples_per_second_per_channel(&self) -> u32;
}

pub fn play_sinewave<B: Buffer>(buffer: &mut B, state: &mut game::State) -> Result<()> {
    let wave_period = buffer.get_samples_per_second_per_channel() / state.tone_hz;

    let (left, right) = buffer.get_new_samples_mut()?;

    for i in 0..left.len() {
        let sine_value = state.t_sine.sin();
        let sample_value = (sine_value * state.volume as f32) as i16;

        left[i] = sample_value;
        right[i] = sample_value;

        let two_pi = 2.0f32 * std::f32::consts::PI;

        state.t_sine = (state.t_sine + two_pi / wave_period as f32) % two_pi;
    }

    Ok(())
}
