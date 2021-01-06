mod razer;

use crate::razer::*;
use std::error::Error;

fn main() -> Result<(), Box<dyn Error>> {
    for file_name in file_names()? {
        let b = battery_level(&file_name);
        let sn = serial_number(&file_name);
        if b > 0 && sn.len() > 0 {
            println!("razer,sn={} battery={}", sn, b);
        }
    }
    Ok(())
}
