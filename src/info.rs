use winapi::shared::mmreg::WAVEFORMATEX;
use log::info;

pub fn print_banner() {
    info!("EarPerkOSC v1.0");
    info!("By Foxipso");
    info!("Support: foxipso.com");
    info!("Press Ctrl+C to exit\n");
}

pub fn print_wave_format_information(wave_format: WAVEFORMATEX) {
    let format_tag = wave_format.wFormatTag;
    let channels = wave_format.nChannels;
    let samples_per_sec = wave_format.nSamplesPerSec;
    let avg_bytes_per_sec = wave_format.nAvgBytesPerSec;
    let block_align = wave_format.nBlockAlign;
    let bits_per_sample = wave_format.wBitsPerSample;
    let extra_info_size = wave_format.cbSize;

    // Print out the audio format details
    info!("Format Tag: {}", format_tag);
    info!("Channels: {}", channels);
    info!("Samples Per Sec: {}", samples_per_sec);
    info!("Avg Bytes Per Sec: {}", avg_bytes_per_sec);
    info!("Block Align: {}", block_align);
    info!("Bits Per Sample: {}", bits_per_sample);
    info!("Extra Info Size: {}", extra_info_size);
}

