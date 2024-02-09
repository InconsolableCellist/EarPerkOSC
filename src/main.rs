mod config;
mod osc;
mod process;
mod info;
mod logging;
use logging::setup_logger;
use info::{print_banner, print_wave_format_information};
use process::{calculate_avg_lr, process_vol_overwhelm, process_vol_perk_and_reset};
use config::read_config_ini;

use std::io;
use std::io::Write;
use std::time::Duration;
extern crate rosc;
use rosc::OscType;
use ms_dtyp;
use std::sync::atomic::{AtomicBool, Ordering};

extern crate winapi;
use std::ptr::null_mut;
use ms_dtyp::{BYTE, DWORD};
use winapi::{Class, Interface};
use winapi::um::audioclient::*;
use winapi::um::mmdeviceapi::*;
use winapi::um::combaseapi::*;
use winapi::shared::mmreg::WAVEFORMATEX;
use winapi::um::audiosessiontypes::{AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK};
use winapi::um::consoleapi::SetConsoleCtrlHandler;
use winapi::um::wincon::CTRL_CLOSE_EVENT;
use log::info;

static QUIT_REQUESTED: AtomicBool = AtomicBool::new(false);

unsafe extern "system" fn ctrl_handler(ctrl_type: u32) -> i32 {
    match ctrl_type {
        CTRL_CLOSE_EVENT => {
            QUIT_REQUESTED.store(true, Ordering::SeqCst);
            1
        }
        _ => 0,
    }
}


fn main() {
    ctrlc::set_handler(move || {
        QUIT_REQUESTED.store(true, Ordering::SeqCst);
    }).expect("Error setting Ctrl-C handler");
    setup_logger().unwrap_or_else(|e| panic!("Failed to set up logger: {}", e));

    unsafe {
        SetConsoleCtrlHandler(Some(ctrl_handler), 1);
    }
    let args_true = vec![OscType::Bool(true)];
    let args_false = vec![OscType::Bool(false)];

    let mut last_left_message_timestamp = std::time::Instant::now();
    let mut last_right_message_timestamp = std::time::Instant::now();
    let mut last_overwhelm_timestamp = std::time::Instant::now();

    let mut left_perked = false;
    let mut right_perked = false;
    let mut overwhelmingly_loud = false;

    let config = read_config_ini().unwrap_or_else(|e| panic!("Failed to read config: {}", e));
    print_banner();

    info!("Now listening for stereo audio and sending OSC messages for ear perk on/off...");
    info!("L: perk left ear, R: perk right ear, B: perk both ears, !L: reset left ear, !R: reset right ear\n\
        O: Overwhelmingly loud!, !O: Overwhelming reset\n");

    unsafe {
        let (device_enumerator, device, audio_client, wave_format_ptr, capture_client) = match init_audio() {
            Some(value) => value,
            None => return,
        };

        // Start the audio stream
        (*audio_client).Start();

        while !QUIT_REQUESTED.load(Ordering::SeqCst) {
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
                process_vol_overwhelm(&args_true, &args_false, &config, left_avg, right_avg, &mut last_overwhelm_timestamp, current_time, &mut overwhelmingly_loud);
                if !overwhelmingly_loud {
                    process_vol_perk_and_reset(&args_true, &args_false, &config, &mut last_left_message_timestamp, &mut last_right_message_timestamp,
                                               &mut left_perked, &mut right_perked, left_avg, right_avg, current_time);
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

        (*audio_client).Release();
        (*device).Release();
        (*device_enumerator).Release();
        (*capture_client).Release();

        CoUninitialize();

    }
}

fn init_audio() -> Option<(*mut IMMDeviceEnumerator, *mut IMMDevice, *mut IAudioClient, *mut WAVEFORMATEX, *mut IAudioCaptureClient)> {
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
            info!("Failed to get capture client");
            info!("This might happen because the audio device is in use by another application. Please close any other applications using the audio device and try again.");
            return None;
        }
        Some((device_enumerator, device, audio_client, wave_format_ptr, capture_client))
    }
}