use std::collections::VecDeque;
use std::io;
use std::io::Write;
use std::time::Instant;
use rosc::OscType;
use crate::config::Config;
use crate::osc::send_osc_message;

/// This function processes the volume and checks for ear perking conditions.
/// If the left channel's average volume is louder than the right by a certain threshold, it sends an OSC message to perk the left ear.
/// If the right channel's average volume is louder than the left by a certain threshold, it sends an OSC message to perk the right ear.
/// If the average volumes of both channels are within the threshold of each other and above a volume threshold, it sends OSC messages to perk both ears.
/// If the average volumes of both channels are below the volume threshold, it sends OSC messages to reset both ears.
///
/// # Arguments
/// * `args_true` - The arguments to be sent with the OSC message when the ear is to be perked.
/// * `args_false` - The arguments to be sent with the OSC message when the ear is to be reset.
/// * `config` - The configuration settings.
/// * `last_left_message_timestamp` - The timestamp of the last message sent to the left ear.
/// * `last_right_message_timestamp` - The timestamp of the last message sent to the right ear.
/// * `left_perked` - A boolean indicating whether the left ear is perked.
/// * `right_perked` - A boolean indicating whether the right ear is perked.
/// * `left_avg` - The average volume of the left channel.
/// * `right_avg` - The average volume of the right channel.
/// * `current_time` - The current time.
pub fn process_vol_perk_and_reset(args_true: &Vec<OscType>, args_false: &Vec<OscType>, config: &Config, last_left_message_timestamp: &mut Instant,
                              last_right_message_timestamp: &mut Instant, left_perked: &mut bool, right_perked: &mut bool,
                              left_avg: f32, right_avg: f32, current_time: Instant) {
    if left_avg > config.differential_threshold
        && right_avg > config.differential_threshold
        && left_avg > config.volume_threshold
        && right_avg > config.volume_threshold {
        if current_time - *last_left_message_timestamp > config.timeout &&
            current_time - *last_right_message_timestamp > config.timeout {
            print!("B");
            send_osc_message(&config.address_left, &args_true, &config.address);
            send_osc_message(&config.address_right, &args_true, &config.address);
            *last_left_message_timestamp = current_time;
            *last_right_message_timestamp = current_time;
            *left_perked = true;
            *right_perked = true;
        }
    } else if (left_avg - right_avg > config.differential_threshold) && left_avg > config.volume_threshold {
        if current_time - *last_left_message_timestamp > config.timeout {
            print!("L");
            send_osc_message(&config.address_left, &args_true, &config.address);
            *last_left_message_timestamp = current_time;
            *left_perked = true;
        }
    } else if (right_avg - left_avg > config.differential_threshold) && right_avg > config.volume_threshold {
        if current_time - *last_right_message_timestamp > config.timeout {
            print!("R");
            send_osc_message(&config.address_right, &args_true, &config.address);
            *last_right_message_timestamp = current_time;
            *right_perked = true;
        }
    } else {
        if *left_perked && current_time - *last_left_message_timestamp > config.reset_timeout {
            print!("!L\n");
            send_osc_message(&config.address_left, &args_false, &config.address);
            *left_perked = false;
        }
        if *right_perked && current_time - *last_right_message_timestamp > config.reset_timeout {
            print!("!R\n");
            send_osc_message(&config.address_right, &args_false, &config.address);
            *right_perked = false;
        }
    }
    io::stdout().flush().unwrap();
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
/// * `config` - The configuration settings.
/// * `left_avg` - The average volume of the left channel.
/// * `right_avg` - The average volume of the right channel.
/// * `last_overwhelm_timestamp` - The timestamp of the last message sent for an overwhelmingly loud volume.
/// * `current_time` - The current time.
/// * `overwhelmingly_loud` - A boolean indicating the current state of whether the volume is overwhelmingly loud.
pub fn process_vol_overwhelm(args_true: &Vec<OscType>, args_false: &Vec<OscType>, config:&Config,
                         left_avg: f32, right_avg: f32, last_overwhelm_timestamp: &mut Instant, current_time: Instant,
                         overwhelmingly_loud: &mut bool) {
    if left_avg > config.excessive_volume_threshold || right_avg > config.excessive_volume_threshold {
        print!("O");
        io::stdout().flush().unwrap();
        send_osc_message(&config.address_overwhelmingly_loud, &args_true, &config.address);
        *last_overwhelm_timestamp = current_time;
        *overwhelmingly_loud = true;
    } else if *overwhelmingly_loud && current_time - *last_overwhelm_timestamp > config.reset_timeout {
        print!("!O");
        io::stdout().flush().unwrap();
        send_osc_message(&config.address_overwhelmingly_loud, &args_false, &config.address);
        *overwhelmingly_loud = false;
    }
}

pub fn calculate_avg_lr(sample_queue: &mut VecDeque<u8>, num_channels: usize) -> (f32, f32) {
    let mut left_volume = 0.0;
    let mut right_volume = 0.0;
    let mut count = 0;
    while sample_queue.len() >= num_channels * 4 {
        let mut left_bytes = [0u8; 4];
        let mut right_bytes = [0u8; 4];
        for byte in left_bytes.iter_mut() {
            *byte = sample_queue.pop_front().unwrap();
        }
        for byte in right_bytes.iter_mut() {
            *byte = sample_queue.pop_front().unwrap();
        }
        let left = f32::from_le_bytes(left_bytes);
        let right = f32::from_le_bytes(right_bytes);
        left_volume += left.abs();
        right_volume += right.abs();
        count += 1;
    }
    left_volume /= count as f32;
    right_volume /= count as f32;
    (left_volume, right_volume)
}