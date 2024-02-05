use std::io;
use std::io::{Write};
use std::time::Duration;
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
extern crate rosc;
use rosc::{OscPacket, OscType, OscMessage};
use std::net::{UdpSocket, SocketAddr};
use ini::Ini;
use ms_dtyp;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

extern crate winapi;
use std::ptr::null_mut;
use ms_dtyp::{BYTE, DWORD};
use winapi::{Class, Interface};
use winapi::um::audioclient::*;
use winapi::um::mmdeviceapi::*;
use winapi::um::combaseapi::*;
use winapi::shared::mmreg::{WAVE_FORMAT_EXTENSIBLE, WAVEFORMATEX, WAVEFORMATEXTENSIBLE};
use winapi::um::audiosessiontypes::{AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK};

fn send_osc_message(address: &str, arguments: &Vec<OscType>, target_address: String) {
    let message = OscMessage {
        addr: address.to_string(),
        args: arguments.clone()
    };

    let packet = OscPacket::Message(message);
    let socket = UdpSocket::bind("0.0.0.0:0").unwrap_or_else(|_| panic!("Could not bind to address"));
    let target = target_address.parse::<SocketAddr>().unwrap_or_else(|_| panic!("Could not parse target address"));
    let encoded_packet = rosc::encoder::encode(&packet).unwrap();

    socket.send_to(encoded_packet.as_slice(), target).expect("Failed to send message");
}


fn create_config_ini_if_not_exists() -> Result<(), std::io::Error> {
    if std::path::Path::new("config.ini").exists() {
        return Ok(());
    }
    let mut config = Ini::new();
    config.with_section(None::<String>).set("encoding", "utf-8");
    config.with_section(Some("version"))
        .set("version", "1.0");
    config.with_section(Some("connection"))
        .set("address", "127.0.0.1")
        .set("port", "9000")
        .set("osc_address_left", "/avatar/parameters/EarPerkLeft")
        .set("osc_address_right", "/avatar/parameters/EarPerkRight");
    config.with_section(Some("audio"))
        .set("input_device", "CABLE Output (VB-Audio Virtual Cable)")
        .set("differential_threshold", "0.01")
        .set("reset_timeout_ms", "1000")
        .set("timeout_ms", "100")
        .set("buffer_size_ms", "100");
    config.write_to_file("config.ini").unwrap();
    Ok(())
}

fn read_config_ini() -> Result<ini::Ini, ini::Error> {
    create_config_ini_if_not_exists().unwrap();
    let config = Ini::load_from_file("config.ini")?;
    Ok(config)
}

fn print_devices() {
    let host = cpal::default_host();
    let devices = host.input_devices().unwrap();
    println!("----");
    for (device_index, device) in devices.enumerate() {
        println!("Device {}: {:?}", device_index, device.name().unwrap())
    }
    println!("----");
}

fn print_banner() {
    println!("EarPerkOSC v1.0");
    println!("By Foxipso");
    println!("Support: foxipso.com");
    println!("Press Ctrl+C to exit\n");
}

fn main() {
    let args_true = vec![OscType::Bool(true)];
    let args_false = vec![OscType::Bool(false)];

    let mut last_left_message_timestamp = std::time::Instant::now();
    let mut last_right_message_timestamp = std::time::Instant::now();

    let mut left_perked = false;
    let mut right_perked = false;

    let config = read_config_ini().unwrap();
    let address_left = config.section(Some("connection")).unwrap().get("osc_address_left").unwrap().to_owned();
    let address_right = config.section(Some("connection")).unwrap().get("osc_address_right").unwrap().to_owned();
    let target_address = format!("{}:{}", config.section(Some("connection")).unwrap().get("address").unwrap(), config.section(Some("connection")).unwrap().get("port").unwrap());
    let threshold = config.section(Some("audio")).unwrap().get("differential_threshold").unwrap().parse::<f32>().unwrap();
    let reset_timeout = Duration::from_millis(config.section(Some("audio")).unwrap().get("reset_timeout_ms").unwrap().parse::<u64>().unwrap());
    let timeout = Duration::from_millis(config.section(Some("audio")).unwrap().get("timeout_ms").unwrap().parse::<u64>().unwrap());
    let buffer_size_ms = config.section(Some("audio")).unwrap().get("buffer_size_ms").unwrap().parse::<u64>().unwrap();
    let running = Arc::new(AtomicBool::new(true));
    let r = running.clone();

    ctrlc::set_handler(move || {
        r.store(false, Ordering::SeqCst);
    }).expect("Error setting Ctrl-C handler");

    print_banner();

    println!("Now listening for stereo audio and sending OSC messages for ear perk on/off...");
    println!("L: perk left ear, R: perk right ear, B: perk both ears, !L: reset left ear, !R: reset right ear\n\n");

    unsafe {
        CoInitializeEx(null_mut(), COINITBASE_MULTITHREADED);

        // Get the enumerator for audio endpoint devices
        let mut device_enumerator: *mut IMMDeviceEnumerator = null_mut();
        CoCreateInstance(
            &MMDeviceEnumerator::uuidof(),
            null_mut(),
            CLSCTX_ALL,
            &IMMDeviceEnumerator::uuidof(),
            &mut device_enumerator as *mut _ as *mut _,
        );

        // Get the default audio endpoint in eRender (playback) role
        let mut device: *mut IMMDevice = null_mut();
        (*device_enumerator).GetDefaultAudioEndpoint(eRender, eConsole, &mut device);

        // Activate an IAudioClient interface on the endpoint
        let mut audio_client: *mut IAudioClient = null_mut();
        (*device).Activate(
            &IAudioClient::uuidof(),
            CLSCTX_ALL,
            null_mut(),
            &mut audio_client as *mut _ as *mut _,
        );

        // Get audio client's mix format
        let mut wave_format_ptr: *mut WAVEFORMATEX = null_mut();
        (*audio_client).GetMixFormat(&mut wave_format_ptr);

        // Dereference the pointer to get the WAVEFORMATEX structure
        let wave_format = *wave_format_ptr;
        print_wave_format_information(&mut wave_format_ptr, wave_format);
        // let sample_rate = wave_format.nSamplesPerSec;
        // let buffer_size_s = buffer_size_ms as f32 / 1000.0;
        // let mut num_frames = (sample_rate as f32 * buffer_size_s) as u32;

        // Initialize the audio client for loopback capture
        (*audio_client).Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK,
            0,
            0,
            wave_format_ptr,
            null_mut(),
        );

        // get the capture client
        let mut capture_client: *mut IAudioCaptureClient = null_mut();
        (*audio_client).GetService(
            &IAudioCaptureClient::uuidof(),
            &mut capture_client as *mut _ as *mut _,
        );

        // Start the audio stream
        (*audio_client).Start();

        let mut start = std::time::Instant::now();
        loop {
            if !running.load(Ordering::SeqCst) {
                break;
            }

            let mut packet_length: u32 = 0;
            (*capture_client).GetNextPacketSize(&mut packet_length);
            while packet_length != 0 {
                // get the available data in the capture buffer
                let mut buffer: *mut BYTE = null_mut();
                let mut frames_available: u32 = 0;
                let mut flags: DWORD = 0;
                (*capture_client).GetBuffer(
                    &mut buffer,
                    &mut frames_available,
                    &mut flags,
                    null_mut(), // pts timestamp
                    null_mut(), // duration
                );

                let samples = std::slice::from_raw_parts(buffer as *const f32, frames_available as usize);
                let left_avg:f32;
                let right_avg:f32;
                (left_avg, right_avg) = calculate_avg_lr_new(&samples);
                let current_time = std::time::Instant::now();

                // if the left is louder than the right by a threshold, perk the left ear
                // if the right is louder than the left by a threshold, perk the right ear
                // if the left and right are within the threshold of each other, perk both
                // if the left and right are below the threshold, reset both
                if left_avg - right_avg > threshold {
                    if current_time - last_left_message_timestamp > timeout {
                        print!("L");
                        send_osc_message(&address_left, &args_true, target_address.clone());
                        last_left_message_timestamp = current_time;
                        left_perked = true;
                    }
                } else if right_avg - left_avg > threshold {
                    if current_time - last_right_message_timestamp > timeout {
                        print!("R");
                        send_osc_message(&address_right, &args_true, target_address.clone());
                        last_right_message_timestamp = current_time;
                        right_perked = true;
                    }
                } else if left_avg > threshold && right_avg > threshold {
                    if current_time - last_left_message_timestamp > timeout &&
                        current_time - last_right_message_timestamp > timeout {
                        print!("B");
                        send_osc_message(&address_left, &args_true, target_address.clone());
                        send_osc_message(&address_right, &args_true, target_address.clone());
                        last_left_message_timestamp = current_time;
                        last_right_message_timestamp = current_time;
                        left_perked = true;
                        right_perked = true;
                    }
                } else {
                    if left_perked && current_time - last_left_message_timestamp > reset_timeout {
                        print!("!L\n");
                        send_osc_message(&address_left, &args_false, target_address.clone());
                        left_perked = false;
                    }
                    if right_perked && current_time - last_right_message_timestamp > reset_timeout {
                        print!("!R\n");
                        send_osc_message(&address_right, &args_false, target_address.clone());
                        right_perked = false;
                    }
                }
                io::stdout().flush().unwrap();


                (*capture_client).ReleaseBuffer(frames_available);
                (*capture_client).GetNextPacketSize(&mut packet_length);
            }
            // Sleep for a short time to avoid busy-waiting
            std::thread::sleep(Duration::from_millis(20));
        }


        (*audio_client).Stop();

        // Release Resources
        if !wave_format_ptr.is_null() {
            CoTaskMemFree(wave_format_ptr as *mut _);
        }
        (*capture_client).Release();
        (*audio_client).Release();
        (*device).Release();
        (*device_enumerator).Release();

        CoUninitialize();

    }
}

fn print_wave_format_information(wave_format_ptr: &mut *mut WAVEFORMATEX, wave_format: WAVEFORMATEX) {
// Copy fields to local variables
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

fn calculate_avg_lr(data: &[f32]) -> (f32, f32) {
    let mut left_sum = 0.0;
    let mut right_sum = 0.0;
    for (i, &sample) in data.iter().enumerate() {
        if i % 2 == 0 {
            left_sum += sample.abs();
        } else {
            right_sum += sample.abs();
        }
    }
    (left_sum / (data.len() as f32/2.0), right_sum / (data.len() as f32/2.0))
}

fn calculate_avg_lr_new(samples: &[f32]) -> (f32, f32) {
    let mut left_volume = 0.0;
    let mut right_volume = 0.0;
    let mut count = 0;
    for chunk in samples.chunks(2) {
        if let [left, right] = chunk {
            left_volume += left.abs();
            right_volume += right.abs();
            count += 1;
        }
    }
    left_volume /= count as f32;
    right_volume /= count as f32;
    (left_volume, right_volume)
}