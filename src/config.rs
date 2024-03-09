use std::net::SocketAddr;
use std::time::Duration;
use ini::Ini;

pub struct Config {
    pub address: SocketAddr,
    pub address_left: String,
    pub address_right: String,
    pub address_overwhelmingly_loud: String,

    pub differential_threshold: f32,
    pub volume_threshold: f32,
    pub excessive_volume_threshold: f32,
    pub reset_timeout: Duration,
    pub timeout: Duration,
}


pub fn create_config_ini_if_not_exists() -> Result<(), std::io::Error> {
    if std::path::Path::new("config.ini").exists() {
        return Ok(());
    }
    let mut config = Ini::new();
    config.with_section(None::<String>).set("encoding", "utf-8");
    config.with_section(Some("version"))
        .set("version", "1.0");
    config.with_section(Some("connection"))
        .set("address", "127.0.0.1:9000")
        .set("osc_address_left", "/avatar/parameters/EarPerkLeft")
        .set("osc_address_right", "/avatar/parameters/EarPerkRight")
        .set("osc_address_overwhelmingly_loud", "/avatar/parameters/EarOverwhelm");
    config.with_section(Some("audio"))
        .set("differential_threshold", "0.01")
        .set("volume_threshold", "0.2")
        .set("excessive_volume_threshold", "0.5")
        .set("reset_timeout_ms", "1000")
        .set("timeout_ms", "100");
    config.write_to_file("config.ini").unwrap();
    Ok(())
}

pub fn save_ini(config: &Config) -> Result<(), Box<dyn std::error::Error>> {
    let mut out_config = Ini::new();
    // Set the values in the Ini from the Config
    out_config.with_section(None::<String>).set("encoding", "utf-8");
    out_config.with_section(Some("version"))
        .set("version", "1.0");
    out_config.with_section(Some("connection"))
        .set("address", &config.address.to_string())
        .set("osc_address_left", &config.address_left)
        .set("osc_address_right", &config.address_right)
        .set("osc_address_overwhelmingly_loud", &config.address_overwhelmingly_loud);
    out_config.with_section(Some("audio"))
        .set("differential_threshold", &config.differential_threshold.to_string())
        .set("volume_threshold", &config.volume_threshold.to_string())
        .set("excessive_volume_threshold", &config.excessive_volume_threshold.to_string())
        .set("reset_timeout_ms", &config.reset_timeout.as_millis().to_string())
        .set("timeout_ms", &config.timeout.as_millis().to_string());
    // Write the Ini to the config.ini file
    out_config.write_to_file("config.ini")?;
    Ok(())
}

pub fn read_config_ini() -> Result<Config, Box<dyn std::error::Error>> {
    create_config_ini_if_not_exists()?;
    let config = Ini::load_from_file("config.ini")?;
    let connection = config.section(Some("connection")).ok_or("Missing 'connection' section")?;
    let audio = config.section(Some("audio")).ok_or("Missing 'audio' section")?;

    // set address as a SocketAddr
    let address = connection.get("address").ok_or("Missing 'address'")?.parse()?;
    let address_left = connection.get("osc_address_left").ok_or("Missing 'osc_address_left'")?.to_owned();
    let address_right = connection.get("osc_address_right").ok_or("Missing 'osc_address_right'")?.to_owned();
    let address_overwhelmingly_loud = connection.get("osc_address_overwhelmingly_loud").ok_or("Missing 'osc_address_overwhelmingly_loud'")?.to_owned();

    let differential_threshold = audio.get("differential_threshold").ok_or("Missing 'differential_threshold'")?.parse()?;
    let volume_threshold = audio.get("volume_threshold").ok_or("Missing 'volume_threshold'")?.parse()?;
    let excessive_volume_threshold = audio.get("excessive_volume_threshold").ok_or("Missing 'excessive_volume_threshold'")?.parse()?;
    let reset_timeout = Duration::from_millis(audio.get("reset_timeout_ms").ok_or("Missing 'reset_timeout_ms'")?.parse()?);
    let timeout = Duration::from_millis(audio.get("timeout_ms").ok_or("Missing 'timeout_ms'")?.parse()?);

    Ok(Config {
        address,
        address_left,
        address_right,
        address_overwhelmingly_loud,
        differential_threshold,
        volume_threshold,
        excessive_volume_threshold,
        reset_timeout,
        timeout,
    })
}
