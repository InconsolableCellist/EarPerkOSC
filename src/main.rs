mod config;
mod osc;
mod process;
mod info;
mod logging;

use std::collections::VecDeque;
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
use winapi::um::consoleapi::SetConsoleCtrlHandler;
use winapi::um::wincon::CTRL_CLOSE_EVENT;
use log::info;
use wasapi::{AudioCaptureClient, AudioClient, Direction, get_default_device, Handle, initialize_mta, SampleType, ShareMode, WaveFormat};

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

    let (h_event, audio_client, wave_format_ptr, capture_client) = match init_audio() {
        Some(value) => value,
        None => return,
    };

    // Start the audio stream
    audio_client.start_stream().unwrap();

    let chunksize = 4096;
    let blockalign = wave_format_ptr.get_blockalign();
    let buffer_frame_count = audio_client.get_bufferframecount().unwrap();
    let mut sample_queue: VecDeque<u8> = VecDeque::with_capacity(
        100 * blockalign as usize * (1024 + 2 * buffer_frame_count as usize),
    );

    while !QUIT_REQUESTED.load(Ordering::SeqCst) {
        while sample_queue.len() > (blockalign as usize * chunksize) {
            let mut chunk = vec![0f32; (blockalign as usize * chunksize)/4];
            for element in chunk.iter_mut() {
                *element = f32::from_le_bytes([
                    sample_queue.pop_front().unwrap(),
                    sample_queue.pop_front().unwrap(),
                    sample_queue.pop_front().unwrap(),
                    sample_queue.pop_front().unwrap()
                ])
            }

            let (left_avg, right_avg) = calculate_avg_lr(&chunk);
            let current_time = std::time::Instant::now();
            process_vol_overwhelm(&args_true, &args_false, &config, left_avg, right_avg, &mut last_overwhelm_timestamp, current_time, &mut overwhelmingly_loud);
            if !overwhelmingly_loud {
                process_vol_perk_and_reset(&args_true, &args_false, &config, &mut last_left_message_timestamp, &mut last_right_message_timestamp,
                                           &mut left_perked, &mut right_perked, left_avg, right_avg, current_time);
            }
        }
        capture_client.read_from_device_to_deque(blockalign as usize, &mut sample_queue).unwrap();
        if h_event.wait_for_event(1000000).is_err() {
            audio_client.stop_stream().unwrap();
            break;
        }
    }
}

fn init_audio() -> Option<(Handle, AudioClient, WaveFormat, AudioCaptureClient)> {
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