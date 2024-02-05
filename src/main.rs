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
        .set("input_device", "CABLE Output (VB-Audio Virtual Cable)");
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
    for (device_index, device) in devices.enumerate() {
        println!("Device {}: {:?}", device_index, device.name().unwrap())
    }
}

/*
fn receive_osc_message() -> Result<(), std::io::Error> {
    let socket = UdpSocket::bind("0.0.0.0:12345")?;
    let mut buf = [0; 1024];

    loop {
        let (size, _) = socket.recv_from(&mut buf)?;
        let packet = rosc::decoder::decode_udp(&buf[..size]);

        match packet {
            Ok(osc_packet) => {
                match osc_packet {
                    OscPacket::Message(msg) => {
                        println!("Received OSC message: {:?}", msg);
                    }
                    OscPacket::Bundle(bundle) => {
                        for msg in bundle.content {
                            if let OscPacket::Message(inner_msg) = msg {
                                println!("Received OSC message from bundle: {:?}", inner_msg);
                            }
                        }
                    }
                }
            },
            Err(e) => {
                eprintln!("Error decoding OSC packet: {}", e);
            }
        }

    }
}
*/

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
        for (device_index, device) in host.input_devices().unwrap().enumerate() {
            println!("Device {}: {:?}", device_index, device.name().unwrap())
        }
        let mut input = String::new();
        print!("Select device: ");
        stdout().flush().unwrap();
        stdin().read_line(&mut input).unwrap();
        let device_index: usize = input.trim().parse().unwrap();

        devices_vec.get(device_index).expect("Failed to get device.")
    };

    let config = device.default_input_config().expect("Failed to get default input config");
    println!("Default input config: {:?}", config);

    let stream = device.build_input_stream(
        &config.into(),
        move |data: &[f32], _: &cpal::InputCallbackInfo| {
            let threshold = 0.3;
            for (i, &sample) in data.iter().enumerate() {
                if sample.abs() > threshold {
                    if i % 2 == 0 {
                        if std::time::Instant::now() - last_left_message_timestamp > Duration::from_millis(100) {
                            arguments[0] = OscType::Bool(true);
                            println!("L");
                            if let Err(e) = send_osc_message(&address_left, arguments.clone(), target_address.clone()) {
                                eprintln!("Error sending OSC message: {}", e);
                            }
                            last_left_message_timestamp = std::time::Instant::now();
                            left_perked = true;
                        }
                    } else {
                        if std::time::Instant::now() - last_right_message_timestamp > Duration::from_millis(100) {
                            arguments[0] = OscType::Bool(true);
                            println!("R");
                            if let Err(e) = send_osc_message(&address_right, arguments.clone(), target_address.clone()) {
                                eprintln!("Error sending OSC message: {}", e);
                            }
                            last_right_message_timestamp = std::time::Instant::now();
                            right_perked = true;
                        }
                    }
                } else {
                    if left_perked && std::time::Instant::now() - last_left_message_timestamp > Duration::from_millis(1000) {
                        arguments[0] = OscType::Bool(false);
                        println!("!L");
                        if let Err(e) = send_osc_message(&address_left, arguments.clone(), target_address.clone()) {
                            eprintln!("Error sending OSC message: {}", e);
                        }
                        left_perked = false;
                    }
                    if right_perked && std::time::Instant::now() - last_right_message_timestamp > Duration::from_millis(1000) {
                        println!("!R");
                        arguments[0] = OscType::Bool(false);
                        if let Err(e) = send_osc_message(&address_right, arguments.clone(), target_address.clone()) {
                            eprintln!("Error sending OSC message: {}", e);
                        }
                        right_perked = false;
                    }
                }
            }
        },
        move |err| {
            eprintln!("an error occurred on stream: {}", err);
        },
        Some(Duration::from_millis(500)),
    ).unwrap();

    stream.play().unwrap();

    // The stream is stopped when it is dropped.
    // To keep it playing, we park the thread here.
    std::thread::park();
}
