use std::net::{SocketAddr, UdpSocket};
use rosc::{OscMessage, OscPacket, OscType};

pub fn send_osc_message(osc_address: &String, arguments: &Vec<OscType>, target: &SocketAddr) {
    let message = OscMessage {
        addr: osc_address.to_string(),
        args: arguments.clone()
    };

    let packet = OscPacket::Message(message);
    let socket = UdpSocket::bind("0.0.0.0:0").unwrap_or_else(|_| panic!("Could not bind to address"));
    let encoded_packet = rosc::encoder::encode(&packet).unwrap();

    socket.send_to(encoded_packet.as_slice(), target).expect("Failed to send message");
}


