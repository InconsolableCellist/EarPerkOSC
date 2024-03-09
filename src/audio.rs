use tokio::sync::Mutex;
use std::sync::Arc;
use std::collections::VecDeque;
use std::io;
use std::io::Write;
use std::time::Instant;
use rosc::OscType;
use wasapi::{AudioCaptureClient, AudioClient, Handle, WaveFormat};
use crate::config::Config;
use crate::init::init_audio;
use crate::osc::send_osc_message;
use crate::process::{calculate_avg_lr, process_vol_overwhelm, process_vol_perk_and_reset};

pub struct AudioProcessor {
    last_left_volume: Arc<Mutex<f32>>,
    last_right_volume: Arc<Mutex<f32>>,
    audio_client: AudioClient,
    capture_client: AudioCaptureClient,
    h_event: Handle,
    wave_format_ptr: WaveFormat,
    block_align: u32,
    last_left_message_timestamp: Instant,
    last_right_message_timestamp: Instant,
    left_perked: bool,
    right_perked: bool,
    overwhelmingly_loud: bool,
    last_overwhelm_timestamp: Instant,
}

impl AudioProcessor {
    pub fn new(config: Arc<Config>) -> Self {
        let (h_event, audio_client, wave_format_ptr, capture_client) = init_audio().unwrap();
        let last_left_volume: Arc<Mutex<f32>> = Arc::new(Mutex::new(0.0));
        let last_right_volume: Arc<Mutex<f32>> = Arc::new(Mutex::new(0.0));

        let last_left_volume_clone = Arc::clone(&last_left_volume);
        let last_right_volume_clone = Arc::clone(&last_right_volume);

        tokio::spawn(start_audio_processing(audio_client.clone(), capture_client.clone(),
                                            wave_format_ptr.get_blockalign(),
                                            last_left_volume_clone, last_right_volume_clone,
                                            h_event.clone(), config.clone()));

        Self {
            last_left_volume,
            last_right_volume,
            audio_client,
            capture_client,
            h_event,
            wave_format_ptr,
            block_align: wave_format_ptr.get_blockalign(),
            last_left_message_timestamp: Instant::now(),
            last_right_message_timestamp: Instant::now(),
            left_perked: false,
            right_perked: false,
            overwhelmingly_loud: false,
            last_overwhelm_timestamp: Instant::now(),
        }
    }

    pub async fn get_last_left_volume(&self) -> f32 {
        *self.last_left_volume.lock().await
    }

    pub async fn get_last_right_volume(&self) -> f32 {
        *self.last_right_volume.lock().await
    }

    pub fn get_wave_format(&self) -> &WaveFormat {
        &self.wave_format_ptr
    }

    async fn start_audio_processing(audio_client: AudioClient, capture_client: AudioCaptureClient,
                                    block_align: u32, last_left_volume: Arc<Mutex<f32>>,
                                    last_right_volume: Arc<Mutex<f32>>, h_event: Handle,
                                    config: Arc<Config>) {
        let mut sample_queue: VecDeque<u8> = VecDeque::new();
        audio_client.start_stream();

        let mut last_left_message_timestamp = std::time::Instant::now();
        let mut last_right_message_timestamp = std::time::Instant::now();
        let mut last_overwhelm_timestamp = std::time::Instant::now();

        let mut left_perked = false;
        let mut right_perked = false;
        let mut overwhelmingly_loud = false;

        let args_true: Vec<OscType> = vec![OscType::Bool(true)];
        let args_false: Vec<OscType> = vec![OscType::Bool(false)];

        loop {
            // Capture into sample_queue
            capture_client.read_from_device_to_deque(
                block_align as usize,
                &mut sample_queue,
            ).unwrap();
            if h_event.wait_for_event(1000000).is_err() {
                audio_client.stop_stream().unwrap();
                break;
            }

            // Process
            let (left_avg, right_avg) = calculate_avg_lr(&mut sample_queue, audio_client.get_waveformat().get_nchannels() as usize);
            *last_left_volume.lock().await = left_avg;
            *last_right_volume.lock().await = right_avg;
            let current_time = std::time::Instant::now();

            process_vol_overwhelm(&args_true, &args_false, Arc::clone(&config), left_avg, right_avg, &mut last_overwhelm_timestamp, current_time, &mut overwhelmingly_loud);
            if !overwhelmingly_loud {

                process_vol_perk_and_reset(&args_true, &args_false, Arc::clone(&config), &mut last_left_message_timestamp, &mut last_right_message_timestamp,
                                           &mut left_perked, &mut right_perked, left_avg, right_avg, current_time);
            }
        }
    }

}