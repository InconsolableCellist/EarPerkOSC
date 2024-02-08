use winapi::shared::mmreg::WAVEFORMATEX;

pub fn print_banner() {
    println!("EarPerkOSC v1.0");
    println!("By Foxipso");
    println!("Support: foxipso.com");
    println!("Press Ctrl+C to exit\n");
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
    println!("Format Tag: {}", format_tag);
    println!("Channels: {}", channels);
    println!("Samples Per Sec: {}", samples_per_sec);
    println!("Avg Bytes Per Sec: {}", avg_bytes_per_sec);
    println!("Block Align: {}", block_align);
    println!("Bits Per Sample: {}", bits_per_sample);
    println!("Extra Info Size: {}", extra_info_size);
}

