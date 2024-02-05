use std::io;
use std::io::{stdin, stdout, Write};
use std::time::Duration;
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
extern crate rosc;
use rosc::{OscPacket, OscType, OscMessage, OscBundle};
use std::net::{UdpSocket, SocketAddr};
use ini::Ini;
fn send_osc_message(address: &str, arguments: Vec<OscType>, target_address: String) -> Result<(), std::io::Error> {
    let message = OscMessage {
        addr: address.to_string(),
        args: arguments
    };

    let packet = OscPacket::Message(message);
    let socket = UdpSocket::bind("0.0.0.0:0")?;
    let target = SocketAddr::from(([127, 0, 0, 1], 9000));

    socket.send_to(rosc::encoder::encode(&packet).unwrap().as_slice(), target)?;
    Ok(())
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
        .set("threshold", "0.3")
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
    let mut arguments = vec![OscType::Bool(true)];

    let host = cpal::default_host();
    let mut devices = host.input_devices().unwrap();
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
    let threshold = config.section(Some("audio")).unwrap().get("threshold").unwrap().parse::<f32>().unwrap();
    let reset_timeout = config.section(Some("audio")).unwrap().get("reset_timeout_ms").unwrap().parse::<u64>().unwrap();
    let timeout = config.section(Some("audio")).unwrap().get("timeout_ms").unwrap().parse::<u64>().unwrap();
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
            // analysis method 1
            /*
            for (i, &sample) in data.iter().enumerate() {
                if sample.abs() > threshold {
                    if i % 2 == 0 {
                        // only fire every timeout ms
                        if std::time::Instant::now() - last_left_message_timestamp > Duration::from_millis(timeout) {
                            arguments[0] = OscType::Bool(true);
                            print!("L");
                            io::stdout().flush().unwrap();
                            let _ = send_osc_message(&address_left, arguments.clone(), target_address.clone());
                            last_left_message_timestamp = std::time::Instant::now();
                            left_perked = true;
                        }
                    } else {
                        if std::time::Instant::now() - last_right_message_timestamp > Duration::from_millis(timeout) {
                            arguments[0] = OscType::Bool(true);
                            print!("R");
                            io::stdout().flush().unwrap();
                            let _ = send_osc_message(&address_right, arguments.clone(), target_address.clone());
                            last_right_message_timestamp = std::time::Instant::now();
                            right_perked = true;
                        }
                    }
                } else {
                    // reset after reset_timeout ms
                    if left_perked && std::time::Instant::now() - last_left_message_timestamp > Duration::from_millis(reset_timeout) {
                        arguments[0] = OscType::Bool(false);
                        print!("!L\n");
                        io::stdout().flush().unwrap();
                        let _ = send_osc_message(&address_left, arguments.clone(), target_address.clone());
                        left_perked = false;
                    }
                    if right_perked && std::time::Instant::now() - last_right_message_timestamp > Duration::from_millis(reset_timeout) {
                        print!("!R\n");
                        io::stdout().flush().unwrap();
                        arguments[0] = OscType::Bool(false);
                        let _ = send_osc_message(&address_right, arguments.clone(), target_address.clone());
                        right_perked = false;
                    }
                }
            }
             */

            //analysis method 2
            // iterate over every sample in the buffer and create an average of the left channel and right channel
            let mut left_sum = 0.0;
            let mut right_sum = 0.0;
            for (i, &sample) in data.iter().enumerate() {
                if i % 2 == 0 {
                    left_sum += sample;
                } else {
                    right_sum += sample;
                }
            }
            let left_avg = left_sum / (data.len() as f32 / 2.0);
            let right_avg = right_sum / (data.len() as f32 / 2.0);
            // if the left is louder than the right by a threshold, perk the left ear
            // if the right is louder than the left by a threshold, perk the right ear
            // if the left and right are within the threshold of each other, perk both
            // if the left and right are below the threshold, reset both
            if left_avg - right_avg > threshold {
                if std::time::Instant::now() - last_left_message_timestamp > Duration::from_millis(timeout) {
                    arguments[0] = OscType::Bool(true);
                    print!("L");
                    io::stdout().flush().unwrap();
                    let _ = send_osc_message(&address_left, arguments.clone(), target_address.clone());
                    last_left_message_timestamp = std::time::Instant::now();
                    left_perked = true;
                }
            } else if right_avg - left_avg > threshold {
                if std::time::Instant::now() - last_right_message_timestamp > Duration::from_millis(timeout) {
                    arguments[0] = OscType::Bool(true);
                    print!("R");
                    io::stdout().flush().unwrap();
                    let _ = send_osc_message(&address_right, arguments.clone(), target_address.clone());
                    last_right_message_timestamp = std::time::Instant::now();
                    right_perked = true;
                }
            } else if left_avg > threshold && right_avg > threshold {
                if std::time::Instant::now() - last_left_message_timestamp > Duration::from_millis(timeout) {
                    arguments[0] = OscType::Bool(true);
                    print!("L");
                    io::stdout().flush().unwrap();
                    let _ = send_osc_message(&address_left, arguments.clone(), target_address.clone());
                    last_left_message_timestamp = std::time::Instant::now();
                    left_perked = true;
                }
                if std::time::Instant::now() - last_right_message_timestamp > Duration::from_millis(timeout) {
                    arguments[0] = OscType::Bool(true);
                    print!("R");
                    io::stdout().flush().unwrap();
                    let _ = send_osc_message(&address_right, arguments.clone(), target_address.clone());
                    last_right_message_timestamp = std::time::Instant::now();
                    right_perked = true;
                }
            } else {
                if left_perked && std::time::Instant::now() - last_left_message_timestamp > Duration::from_millis(reset_timeout) {
                    arguments[0] = OscType::Bool(false);
                    print!("!L\n");
                    io::stdout().flush().unwrap();
                    let _ = send_osc_message(&address_left, arguments.clone(), target_address.clone());
                    left_perked = false;
                }
                if right_perked && std::time::Instant::now() - last_right_message_timestamp > Duration::from_millis(reset_timeout) {
                    print!("!R\n");
                    io::stdout().flush().unwrap();
                    arguments[0] = OscType::Bool(false);
                    let _ = send_osc_message(&address_right, arguments.clone(), target_address.clone());
                    right_perked = false;
                }
            }
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
