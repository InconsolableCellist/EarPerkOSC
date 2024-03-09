use std::collections::VecDeque;
use std::error::Error;

use eframe::{
    App,
    egui::{self, CentralPanel},
};
use wasapi::{AudioCaptureClient, AudioClient, Handle, WaveFormat};

use crate::config::Config;
use crate::init::init_audio;
use crate::process::{calculate_avg_lr, process_vol_overwhelm, process_vol_perk_and_reset};
use crate::audio::AudioProcessor;

pub struct EarPerkOSCUI {
    config: Config,
    audio_processor: AudioProcessor,
    last_left_volume: f32,
    last_right_volume: f32,
}

impl EarPerkOSCUI {
    pub fn new(cc: &eframe::CreationContext<'_>, config: Config) -> Self {
        let audio_processor = AudioProcessor::new(config.clone());

        audio_processor.start_stream();

        Self {
            config,
            audio_processor,
            last_left_volume: 0.0,
            last_right_volume: 0.0,
        }
    }
}

impl App for EarPerkOSCUI {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        let last_left_volume = self.audio_processor.get_last_left_volume();
        let last_right_volume = self.audio_processor.get_last_right_volume();

        CentralPanel::default().show(ctx, |ui| {
            // Top panel for status
            egui::TopBottomPanel::top("status_panel").show(ctx, |ui| {
                ui.heading("Status");
                ui.label("Status: ");
            });

            // Central panel for debug output
            CentralPanel::default().show(ctx, |ui| {
                ui.heading("Debug Output");
                egui::ScrollArea::vertical().show(ui, |ui| {
                    ui.label("Lorem Ipsum");
                });
            });

            // Bottom panel for last recorded left/right volumes
            egui::TopBottomPanel::bottom("volume_panel").show(ctx, |ui| {
                let mut left_volume         = last_left_volume;
                let mut right_volume        = last_right_volume;
                let mut volume_threshold    = self.config.volume_threshold;
                let mut overwhelming_threshold = self.config.excessive_volume_threshold;
                let mut differential_threshold = self.config.differential_threshold;
                let mut reset_timeout       = self.config.reset_timeout.as_millis() as f32;
                let mut timeout             = self.config.timeout.as_millis() as f32;

                ui.heading("Configuration");
                egui::Grid::new("volume_grid").show(ui, |ui| {
                    ui.add(egui::Slider::new(&mut timeout, 0.0..=500.0).text("Timeout (ms)"));
                    ui.add(egui::Slider::new(&mut volume_threshold, 0.0..=1.0).text("Volume Threshold"));
                    ui.add(egui::Label::new(""));
                    ui.end_row();

                    ui.add(egui::Slider::new(&mut reset_timeout, 0.0..=5000.0).text("Reset Timeout (ms)"));
                    ui.add(egui::Slider::new(&mut overwhelming_threshold, 0.0..=1.0).text("Overwhelming Threshold"));
                    ui.add(egui::Label::new(""));
                    ui.end_row();

                    ui.add(egui::Label::new(""));
                    ui.add(egui::Slider::new(&mut differential_threshold, 0.0..=0.1).text("Differential Threshold"));
                    if ui.button("Save Configuration").clicked() {
                        match crate::config::save_ini(&self.config) {
                            Ok(_) => println!("Configuration saved!"),
                            Err(e) => println!("Failed to save configuration: {}", e),
                        }
                    }
                    ui.end_row();

                    self.last_left_volume = left_volume;
                    self.last_right_volume = right_volume;
                    self.config.volume_threshold = volume_threshold;
                    self.config.excessive_volume_threshold = overwhelming_threshold;
                    self.config.differential_threshold = differential_threshold;
                    self.config.reset_timeout = std::time::Duration::from_millis(reset_timeout as u64);
                    self.config.timeout = std::time::Duration::from_millis(timeout as u64);
                });
                ui.separator();

                ui.heading("Last Recorded Volumes");
                ui.add(egui::Slider::new(&mut left_volume, 0.0..=1.0).text("Left Volume"));
                ui.add(egui::Slider::new(&mut right_volume, 0.0..=1.0).text("Right Volume"));
            });
        });
    }
}