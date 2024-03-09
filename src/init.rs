use wasapi::{AudioCaptureClient, AudioClient, Direction, get_default_device, Handle, initialize_mta, SampleType, ShareMode, WaveFormat};

pub fn init_audio() -> Option<(Handle, AudioClient, WaveFormat, AudioCaptureClient)> {
    initialize_mta().unwrap();

    let device = get_default_device(&Direction::Render);
    let mut audio_client = device.unwrap().get_iaudioclient().unwrap();

    let desired_format = WaveFormat::new(32, 32, &SampleType::Float, 44100, 2, None);

    let (_def_time, min_time) = audio_client.get_periods().unwrap();

    audio_client.initialize_client(
        &desired_format,
        min_time,
        &Direction::Capture,
        &ShareMode::Shared,
        true,
    ).unwrap();

    let h_event = audio_client.set_get_eventhandle().unwrap();

    let render_client = audio_client.get_audiocaptureclient().unwrap();

    return Some((h_event, audio_client, desired_format, render_client));
}
