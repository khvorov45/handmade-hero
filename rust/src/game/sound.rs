pub trait Buffer {
    fn get_samples(&self) -> &[i16];
    fn get_mut_samples(&mut self) -> &mut [i16];
    fn get_samples_per_second(&self) -> u32;
}

pub fn play_sinewave<B: Buffer>(buffer: &mut B, sample_count: usize) {
    static mut T_SINE: f32 = 0.0;
    let volume = 2000;
    let tone_hz = 256;
    let wave_period = buffer.get_samples_per_second() / tone_hz;
    let samples = buffer.get_mut_samples();

    let mut sample_index = 0;
    let mut ind_sample_index = 0;
    while sample_index < sample_count {
        let sine_value = unsafe { T_SINE.sin() };
        let sample_value = (sine_value * volume as f32) as i16;

        samples[ind_sample_index] = sample_value;
        samples[ind_sample_index + 1] = sample_value;

        unsafe { T_SINE += 2.0f32 * std::f32::consts::PI * 1.0f32 / wave_period as f32 };

        sample_index += 1;
        ind_sample_index += 2;
    }
}
