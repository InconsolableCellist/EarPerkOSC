use std::io;
use std::io::{Write};
use std::time::Duration;
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
extern crate rosc;
use rosc::{OscPacket, OscType, OscMessage};
use std::net::{UdpSocket, SocketAddr};
use ini::Ini;
fn send_osc_message(address: &str, arguments: &Vec<OscType>, target_address: String) {
    let message = OscMessage {
        addr: address.to_string(),
        args: arguments.clone()
    };

    let packet = OscPacket::Message(message);
    let socket = UdpSocket::bind("0.0.0.0:0").unwrap_or_else(|_| panic!("Could not bind to address"));
    let target = target_address.parse::<SocketAddr>().unwrap_or_else(|_| panic!("Could not parse target address"));
    let encoded_packet = rosc::encoder::encode(&packet).unwrap();

    socket.send_to(encoded_packet.as_slice(), target).expect(return);
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

    let host = cpal::default_host();
    let devices = host.input_devices().unwrap();
    let mut device_index: Option<usize> = None;
    let devices_vec: Vec<_> = devices.collect();

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

    print_banner();
    print_devices();

    let mut iter = 0;
    for device in devices_vec.iter() {
        if let Ok(name) = device.name() {
            if name == config.section(Some("audio")).unwrap().get("input_device").unwrap() {
                device_index = Some(iter);
                println!("Found default device: {}", name);
                println!("You can change the device in config.ini");
                break;
            }
        }
        iter += 1;
    }

    let device = if let Some(index) = device_index {
        devices_vec.get(index).expect("Failed to get device.")
    } else {
        devices_vec.first().expect("Failed to get device.")
    };

    let config = device.default_input_config().expect("Failed to get default input config");
    println!("\nDefault input config: {:?}", config);
    println!("Now listening for stereo audio and sending OSC messages for ear perk on/off...");
    println!("L: perk left ear, R: perk right ear, !L: reset left ear, !R: reset right ear\n\n");

    let stream = device.build_input_stream(
        &config.into(),
        move |data: &[f32], _: &cpal::InputCallbackInfo| {
            let left_avg:f32;
            let right_avg:f32;
            (left_avg, right_avg) = calculate_avg_lr(&data);
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
        },
        move |err| {
            eprintln!("an error occurred on stream: {}", err);
        },
        Some(Duration::from_millis(buffer_size_ms)),
    ).unwrap();

    stream.play().unwrap();

    // The stream is stopped when it is dropped.
    // To keep it playing, we park the thread here.
    std::thread::park();
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
