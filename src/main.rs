use std::io;
use std::io::{Write};
use std::time::{Duration, Instant};
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
use winapi::shared::mmreg::WAVEFORMATEX;
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
        .set("osc_address_right", "/avatar/parameters/EarPerkRight")
        .set("osc_address_overwhelmingly_loud", "/avatar/parameters/EarOverwhelm");
    config.with_section(Some("audio"))
        .set("differential_threshold", "0.01")
        .set("volume_threshold", "0.1")
        .set("excessive_volume_threshold", "0.4")
        .set("reset_timeout_ms", "500")
        .set("timeout_ms", "100");
    config.write_to_file("config.ini").unwrap();
    Ok(())
}

fn read_config_ini() -> Result<ini::Ini, ini::Error> {
    create_config_ini_if_not_exists().unwrap();
    let config = Ini::load_from_file("config.ini")?;
    Ok(config)
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
    let mut last_overwhelm_timestamp = std::time::Instant::now();

    let mut left_perked = false;
    let mut right_perked = false;
    let mut overwhelmingly_loud = false;

    let config = read_config_ini().unwrap();
    let address_left = config.section(Some("connection")).unwrap().get("osc_address_left").unwrap().to_owned();
    let address_right = config.section(Some("connection")).unwrap().get("osc_address_right").unwrap().to_owned();
    let address_overwhelmingly_loud = config.section(Some("connection")).unwrap().get("osc_address_overwhelmingly_loud").unwrap().to_owned();
    let target_address = format!("{}:{}", config.section(Some("connection")).unwrap().get("address").unwrap(), config.section(Some("connection")).unwrap().get("port").unwrap());
    let differential_threshold = config.section(Some("audio")).unwrap().get("differential_threshold").unwrap().parse::<f32>().unwrap();
    let volume_threshold = config.section(Some("audio")).unwrap().get("volume_threshold").unwrap().parse::<f32>().unwrap();
    let excessive_volume_threshold = config.section(Some("audio")).unwrap().get("excessive_volume_threshold").unwrap().parse::<f32>().unwrap();

    let reset_timeout = Duration::from_millis(config.section(Some("audio")).unwrap().get("reset_timeout_ms").unwrap().parse::<u64>().unwrap());
    let timeout = Duration::from_millis(config.section(Some("audio")).unwrap().get("timeout_ms").unwrap().parse::<u64>().unwrap());
    let running = Arc::new(AtomicBool::new(true));
    let r = running.clone();

    ctrlc::set_handler(move || {
        r.store(false, Ordering::SeqCst);
    }).expect("Error setting Ctrl-C handler");

    print_banner();

    println!("Now listening for stereo audio and sending OSC messages for ear perk on/off...");
    println!("L: perk left ear, R: perk right ear, B: perk both ears, !L: reset left ear, !R: reset right ear\n\
        O: Overwhelmingly loud!, !O: Overwhelming reset\n");

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
        print_wave_format_information(wave_format);

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

        if capture_client.is_null() {
            println!("Failed to get capture client");
            println!("This might happen because the audio device is in use by another application. Please close any other applications using the audio device and try again.");
            return;
        }

        // Start the audio stream
        (*audio_client).Start();

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
                (left_avg, right_avg) = calculate_avg_lr(&samples);
                let current_time = std::time::Instant::now();

                // Fold both ears if the volume is overwhelmingly loud (exceeding `excessive_volume_threshold`). Also
                // reset both ears if it's not.
                // If it's not, proceed to perk left, right, both, or unperk based on volume differential and volume, and timing
                process_vol_overwhelm(&args_true, &args_false, &address_overwhelmingly_loud, &target_address, excessive_volume_threshold, left_avg,
                                                            right_avg, &mut last_overwhelm_timestamp, current_time, reset_timeout, &mut overwhelmingly_loud);
                if !overwhelmingly_loud {
                    process_vol_perk_and_reset(&args_true, &args_false, &mut last_left_message_timestamp, &mut last_right_message_timestamp,
                                               &mut left_perked, &mut right_perked, &address_left, &address_right, &target_address,
                                               differential_threshold, volume_threshold, reset_timeout, timeout, left_avg, right_avg, current_time);
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

/// This function processes the volume and checks for ear perking conditions.
/// If the left channel's average volume is louder than the right by a certain threshold, it sends an OSC message to perk the left ear.
/// If the right channel's average volume is louder than the left by a certain threshold, it sends an OSC message to perk the right ear.
/// If the average volumes of both channels are within the threshold of each other and above a volume threshold, it sends OSC messages to perk both ears.
/// If the average volumes of both channels are below the volume threshold, it sends OSC messages to reset both ears.
///
/// # Arguments
/// * `args_true` - The arguments to be sent with the OSC message when the ear is to be perked.
/// * `args_false` - The arguments to be sent with the OSC message when the ear is to be reset.
/// * `last_left_message_timestamp` - The timestamp of the last message sent to the left ear.
/// * `last_right_message_timestamp` - The timestamp of the last message sent to the right ear.
/// * `left_perked` - A boolean indicating whether the left ear is perked.
/// * `right_perked` - A boolean indicating whether the right ear is perked.
/// * `address_left` - The OSC address for the left ear.
/// * `address_right` - The OSC address for the right ear.
/// * `target_address` - The target address to send the OSC message to.
/// * `differential_threshold` - The minimum difference between the left and right channels to trigger a left or right-only ear perk.
/// * `volume_threshold` - The minimum volume to trigger an ear perk.
/// * `reset_timeout` - The time to wait after the last sound before sending the unperk message.
/// * `timeout` - The time to wait before trying to perk the ear again.
/// * `left_avg` - The average volume of the left channel.
/// * `right_avg` - The average volume of the right channel.
/// * `current_time` - The current time.
fn process_vol_perk_and_reset(args_true: &Vec<OscType>, args_false: &Vec<OscType>, last_left_message_timestamp: &mut Instant,
                              last_right_message_timestamp: &mut Instant, left_perked: &mut bool, right_perked: &mut bool,
                              address_left: &String, address_right: &String, target_address: &String, differential_threshold: f32,
                              volume_threshold: f32, reset_timeout: Duration, timeout: Duration, left_avg: f32, right_avg: f32, current_time: Instant) {
    if (left_avg - right_avg > differential_threshold) && left_avg > volume_threshold {
        if current_time - *last_left_message_timestamp > timeout {
            print!("L");
            send_osc_message(&address_left, &args_true, target_address.clone());
            *last_left_message_timestamp = current_time;
            *left_perked = true;
        }
    } else if (right_avg - left_avg > differential_threshold) && right_avg > volume_threshold {
        if current_time - *last_right_message_timestamp > timeout {
            print!("R");
            send_osc_message(&address_right, &args_true, target_address.clone());
            *last_right_message_timestamp = current_time;
            *right_perked = true;
        }
    } else if left_avg > differential_threshold
        && right_avg > differential_threshold
        && left_avg > volume_threshold
        && right_avg > volume_threshold {
        if current_time - *last_left_message_timestamp > timeout &&
            current_time - *last_right_message_timestamp > timeout {
            print!("B");
            send_osc_message(&address_left, &args_true, target_address.clone());
            send_osc_message(&address_right, &args_true, target_address.clone());
            *last_left_message_timestamp = current_time;
            *last_right_message_timestamp = current_time;
            *left_perked = true;
            *right_perked = true;
        }
    } else {
        if *left_perked && current_time - *last_left_message_timestamp > reset_timeout {
            print!("!L\n");
            send_osc_message(&address_left, &args_false, target_address.clone());
            *left_perked = false;
        }
        if *right_perked && current_time - *last_right_message_timestamp > reset_timeout {
            print!("!R\n");
            send_osc_message(&address_right, &args_false, target_address.clone());
            *right_perked = false;
        }
    }
}

/// This function processes the volume and checks if it is overwhelmingly loud.
/// If the average volume of either the left or right channel exceeds the `excessive_volume_threshold`,
/// it sends an OSC message with `args_true` to the `address_overwhelmingly_loud` and returns `true`.
/// If the volume does not exceed the threshold, it sends an OSC message with `args_false` and returns `false`.
/// No action is taken if the volume is not overwhelmingly loud and the last message was sent less than `reset_timeout` ago.
///
/// # Arguments
/// * `args_true` - The arguments to be sent with the OSC message when the volume is overwhelmingly loud.
/// * `args_false` - The arguments to be sent with the OSC message when the volume is not overwhelmingly loud.
/// * `address_overwhelmingly_loud` - The OSC address to send the message to.
/// * `target_address` - The target address to send the OSC message to.
/// * `excessive_volume_threshold` - The threshold for the volume to be considered overwhelmingly loud.
/// * `left_avg` - The average volume of the left channel.
/// * `right_avg` - The average volume of the right channel.
/// * `last_overwhelm_timestamp` - The timestamp of the last message sent for an overwhelmingly loud volume.
/// * `current_time` - The current time.
/// * `reset_timeout` - The time to wait before sending the reset message.
/// * `overwhelmingly_loud` - A boolean indicating the current state of whether the volume is overwhelmingly loud.
fn process_vol_overwhelm(args_true: &Vec<OscType>, args_false: &Vec<OscType>, address_overwhelmingly_loud: &String,
                         target_address: &String, excessive_volume_threshold: f32, left_avg: f32, right_avg: f32,
                         last_overwhelm_timestamp: &mut Instant, current_time: Instant, reset_timeout: Duration,
                         overwhelmingly_loud: &mut bool) {
    if left_avg > excessive_volume_threshold || right_avg > excessive_volume_threshold {
        print!("O");
        send_osc_message(&address_overwhelmingly_loud, &args_true, target_address.clone());
        *last_overwhelm_timestamp = current_time;
        *overwhelmingly_loud = true;
    } else if *overwhelmingly_loud && current_time - *last_overwhelm_timestamp > reset_timeout {
        print!("!O");
        send_osc_message(&address_overwhelmingly_loud, &args_false, target_address.clone());
        *overwhelmingly_loud = false;
    }
}

fn print_wave_format_information(wave_format: WAVEFORMATEX) {
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

fn calculate_avg_lr(samples: &[f32]) -> (f32, f32) {
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