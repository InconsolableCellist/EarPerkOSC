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
use std::sync::mpsc::{self, Sender, Receiver};

pub struct AudioProcessor {
    last_left_volume: Arc<Mutex<f32>>,
    last_right_volume: Arc<Mutex<f32>>,
    audio_client: Arc<AudioClient>,
    capture_client: Arc<AudioCaptureClient>,
    h_event: Arc<Handle>,
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
        let audio_client = Arc::new(audio_client);
        let capture_client = Arc::new(capture_client);
        let h_event = Arc::new(h_event);
        let last_left_volume: Arc<Mutex<f32>> = Arc::new(Mutex::new(0.0));
        let last_right_volume: Arc<Mutex<f32>> = Arc::new(Mutex::new(0.0));

        let last_left_volume_clone = Arc::clone(&last_left_volume);
        let last_right_volume_clone = Arc::clone(&last_right_volume);
        let audio_client_clone = Arc::clone(&audio_client);
        let capture_client_clone = Arc::clone(&capture_client);
        let h_event_clone = Arc::clone(&h_event);
        let config_clone = Arc::clone(&config);

        // Channel for communication
        let (tx, rx): (Sender<(f32, f32)>, Receiver<(f32, f32)>) = mpsc::channel();

        std::thread::spawn(move || {
            Self::start_audio_processing(
                audio_client_clone,
                capture_client_clone,
                wave_format_ptr.get_blockalign(),
                last_left_volume_clone,
                last_right_volume_clone,
                h_event_clone,
                config_clone,
                tx,
            );
        });

        tokio::spawn(Self::process_audio_results(rx, last_left_volume, last_right_volume));

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

    fn start_audio_processing(
        audio_client: Arc<AudioClient>,
        capture_client: Arc<AudioCaptureClient>,
        block_align: u32,
        last_left_volume: Arc<Mutex<f32>>,
        last_right_volume: Arc<Mutex<f32>>,
        h_event: Arc<Handle>,
        config: Arc<Config>,
        tx: Sender<(f32, f32)>,
    ) {
        let mut sample_queue: VecDeque<u8> = VecDeque::new();
        audio_client.start_stream().unwrap();

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
            capture_client.read_from_device_to_deque(block_align as usize, &mut sample_queue).unwrap();
            if h_event.wait_for_event(1000000).is_err() {
                audio_client.stop_stream().unwrap();
                break;
            }

            // Process
            let (left_avg, right_avg) = calculate_avg_lr(&mut sample_queue, audio_client.get_mixformat().get_nchannels() as usize);
            *last_left_volume.lock().await = left_avg;
            *last_right_volume.lock().await = right_avg;
            let current_time = std::time::Instant::now();

            process_vol_overwhelm(&args_true, &args_false, Arc::clone(&config), left_avg, right_avg, &mut last_overwhelm_timestamp, current_time, &mut overwhelmingly_loud);
            if !overwhelmingly_loud {
                process_vol_perk_and_reset(
                    &args_true,
                    &args_false,
                    Arc::clone(&config),
                    &mut last_left_message_timestamp,
                    &mut last_right_message_timestamp,
                    &mut left_perked,
                    &mut right_perked,
                    left_avg,
                    right_avg,
                    current_time,
                );
            }

            // Send the processed data
            if tx.send((left_avg, right_avg)).is_err() {
                break;
            }
        }
    }

    async fn process_audio_results(
        rx: Receiver<(f32, f32)>,
        last_left_volume: Arc<Mutex<f32>>,
        last_right_volume: Arc<Mutex<f32>>,
    ) {
        while let Ok((left_avg, right_avg)) = rx.recv() {
            *last_left_volume.lock().await = left_avg;
            *last_right_volume.lock().await = right_avg;
        }
    }
}
