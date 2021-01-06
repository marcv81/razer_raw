use std::error::Error;
use std::io::Read;
use std::io::Write;

/// Requests and responses.
struct Message {
    data: [u8; 90],
}

impl Message {
    /// Returns a zero-initialized message.
    fn new() -> Message {
        return Message { data: [0u8; 90] };
    }

    /// Calculates the checksum of a message.
    fn checksum(&self) -> u8 {
        let mut result = 0u8;
        for i in 2..88 {
            result ^= self.data[i];
        }
        result
    }

    /// Sends a request to a device node. Returns the response received.
    fn raw_query(&self, file_name: &str, delay: u64) -> Result<Message, Box<dyn Error>> {
        let mut response = Message::new();
        let mut f = std::fs::OpenOptions::new()
            .read(true)
            .write(true)
            .open(file_name)?;
        f.write_all(&self.data)?;
        std::thread::sleep(std::time::Duration::from_millis(delay));
        f.read_exact(&mut response.data)?;
        Ok(response)
    }

    /// Sends a request to a device node. Validates and returns the response received.
    fn query(&self, file_name: &str, delay: u64) -> Result<Message, Box<dyn Error>> {
        let response = self.raw_query(file_name, delay)?;
        if response.data[0] != 2 {
            Err("Invalid response status")?;
        }
        if response.data[88] != response.checksum() {
            Err("Invalid response checksum")?;
        }
        for i in 1..8 {
            if response.data[i] != self.data[i] {
                Err("Mismatched request and response")?;
            }
        }
        Ok(response)
    }
}

/// Reads the battery level from a device node.
pub fn battery_level(file_name: &str) -> u8 {
    /// Parses a response for the battery level.
    /// Converts from a value in the 0-255 range to a percentage.
    fn parse_response(message: Message) -> u8 {
        ((message.data[9] as f32) * 100.0 / 255.0) as u8
    }

    let mut request = Message::new();
    request.data[1] = 0x1f;
    request.data[5] = 0x02;
    request.data[6] = 0x07;
    request.data[7] = 0x80;
    request.data[88] = request.checksum();
    let response = request.query(file_name, 5);
    return match response {
        Ok(message) => parse_response(message),
        Err(_) => 0,
    };
}

/// Reads the serial number from a device node.
pub fn serial_number(file_name: &str) -> String {
    /// Parses a response for the serial number.
    fn parse_response(message: Message) -> String {
        let mut result = String::new();
        let mut i = 8;
        while message.data[i] != 0 {
            result.push(message.data[i] as char);
            i += 1;
        }
        result
    }

    let mut request = Message::new();
    request.data[1] = 0x08;
    request.data[5] = 0x16;
    request.data[7] = 0x82;
    request.data[88] = request.checksum();
    let response = request.query(file_name, 5);
    match response {
        Ok(message) => parse_response(message),
        Err(_) => String::new(),
    }
}

/// Returns the list of device nodes.
pub fn file_names() -> Result<Vec<String>, Box<dyn Error>> {
    /// Checks if a string starts with the specified prefix.
    fn starts_with(s: &str, prefix: &str) -> bool {
        return s.len() >= prefix.len() && s[..prefix.len()] == *prefix;
    }

    let mut result: Vec<String> = Vec::new();
    for entry in std::fs::read_dir("/dev")? {
        let path = entry?.path();
        let file_name = path
            .file_name()
            .ok_or("Path has no file name")?
            .to_str()
            .ok_or("Cannot convert file name to str")?;
        if starts_with(file_name, "razer") {
            let path_name = path.to_str().ok_or("Cannot convert path to str")?;
            result.push(path_name.to_string());
        }
    }
    Ok(result)
}
