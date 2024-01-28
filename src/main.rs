use std::io::{stdin, stdout, Write};
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};

fn main() {
    let host = cpal::default_host();
    let devices = host.input_devices().unwrap();
    for (device_index, device) in devices.enumerate() {
        println!("Device {}: {:?}", device_index, device.name().unwrap())
    }

    let mut input = String::new();
    print!("Select device: ");
    stdout().flush().unwrap();
    stdin().read_line(&mut input).unwrap();
    let device_index: usize = input.trim().parse().unwrap();

    let device = host.input_devices().unwrap().nth(device_index).expect("Failed to get device.");

    let config = device.default_input_config().expect("Failed to get default input config");
    println!("Default input config: {:?}", config);

    let stream = device.build_input_stream(
        &config.into(),
        move |data: &[f32], _: &cpal::InputCallbackInfo| {
            if data.iter().any(|&x| x != 0.0) {
                println!("Audio data: {:?}", data);
            }
        },
        move |err| {
            eprintln!("an error occurred on stream: {}", err);
        },
    ).unwrap();

    stream.play().unwrap();

    // The stream is stopped when it is dropped.
    // To keep it playing, we park the thread here.
    std::thread::park();
}
