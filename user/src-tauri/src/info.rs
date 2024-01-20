use std::process::Command;

fn main() {
    let speed = get_memory("speed");
    println!(
        "RAM CAPACITY {:?}",
        get_memory("capacity")
            .unwrap()
            .into_iter()
            .map(|s| s.parse::<i64>().unwrap())
            .reduce(|acc, v| acc + v)
            .unwrap()
    );
    if let Some(vendor) = get_product("manufacturer") {
        println!("PRODUCT VENDOR {}", vendor[0]);
    }
    if let Some(model) = get_product("model") {
        println!("PRODUCT MODEL {}", model[0]);
    }

    println!("GET SERIAL NUMBER {}", get_mother_board("serialnumber")[0]);
    if speed.is_some() {
        let speed = speed.unwrap()[0].parse().unwrap();
        println!("RAM SPEED {}", speed);
        println!("RAM TYPE {}", get_ddr_type_by_speed(speed));
    }
    println!(
        "DISPLAY RESOLUTION X {}",
        get_display("currenthorizontalresolution")[0]
    );
    println!(
        "DISPLAY RESOLUTION Y {}",
        get_display("currentverticalresolution")[0]
    );
    println!("PROCESSOR {}", get_cpu("name")[0]);
    println!("BIOS VERSION {}", get_bios("smbiosbiosversion")[0]);
    println!("BIOS VENDOR {}", get_bios("Manufacturer")[0]);
    println!("OPERATING SYSTEM {}", get_os("caption")[0]);
    println!(
        "BATTERY STATUS {}",
        if let Some(v) = get_battery("EstimatedChargeRemaining") {
            if let Some(capacity) = get_battery("DesignCapacity") {
                println!("BATTERY DESIGN CAPACITY {}", capacity[0]);
            }
            if let Some(capacity) = get_battery("FullChargeCapacity") {
                println!("BATTERY FullCharge Capacity {}", capacity[0]);
            }
            v[0].clone()
        } else {
            "NO BATTERY".to_string()
        }
    );

    println!("IS LICECENSED {}", get_activation_status());
    let disks = get_disks();
    if disks.is_some() {
        let disks = disks.unwrap();
        for disk in disks {
            println!(
                "DISK {} , interface {} , type {}",
                disk.friendlyname,
                parse_bus_type(disk.bustype),
                parse_media_type(disk.mediatype)
            );
        }
    }
    if detect_rights() {
        println!("SECURE BOOT {}", get_secure_boot()[0]);

        println!("BIOS TYPE {}", get_bios_type());
    }
    std::thread::sleep(std::time::Duration::from_secs(10));
}

fn get_memory(info_type: &str) -> Option<Vec<String>> {
    let output = Command::new("wmic")
        .arg("memorychip")
        .arg("get")
        .arg(info_type)
        .output()
        .expect("Failed to execute command");
    let res = String::from_utf8_lossy(&output.stdout).to_string();

    let res = res
        .trim()
        .split("\n")
        .map(|s| s.trim().to_owned())
        .collect::<Vec<String>>();
    if res.len() > 1 {
        Some(res[1..].to_vec())
    } else {
        None
    }
}
fn get_mother_board(info_type: &str) -> Vec<String> {
    let output = Command::new("wmic")
        .arg("baseboard")
        .arg("get")
        .arg(info_type)
        .output()
        .expect("Failed to execute command");
    let res = String::from_utf8_lossy(&output.stdout).to_string();

    let res = res
        .trim()
        .split("\n")
        .map(|s| s.trim().to_owned())
        .collect::<Vec<String>>();

    res[1..].to_vec()
}

#[derive(Debug, serde::Deserialize)]
struct PhysicalDisk {
    node: String,
    friendlyname: String,
    mediatype: i8,
    bustype: i8,
}
fn get_disks() -> Option<Vec<PhysicalDisk>> {
    let output = Command::new("wmic")
        .arg("/namespace:\\\\root\\microsoft\\windows\\storage")
        .arg("path")
        .arg("MSFT_PhysicalDisk")
        .arg("get")
        .arg("FriendlyName,bustype,MediaType,spindlespeed")
        .arg("/format:csv")
        .output()
        .expect("Failed to execute command");
    let res = String::from_utf8_lossy(&output.stdout)
        .to_string()
        .to_lowercase();
    let mut rdr = csv::Reader::from_reader(res.as_bytes());
    let mut res: Vec<PhysicalDisk> = Vec::new();
    for result in rdr.deserialize() {
        // The iterator yields Result<StringRecord, Error>, so we check the
        // error here.
        let record: PhysicalDisk = result.unwrap();
        if record.bustype != 15 {
            // 15 is virtual disk
            res.push(record)
        }
    }
    if res.len() > 1 {
        Some(res)
    } else {
        None
    }
}

fn get_bios_type() -> String {
    let output = Command::new("bcdedit")
        .output()
        .expect("Failed to execute command");
    let res = String::from_utf8_lossy(&output.stdout)
        .to_string()
        .to_lowercase();

    if res.contains("winload.exe") {
        "BIOS".to_string()
    } else if res.contains("winload.efi") {
        "UEFI".to_string()
    } else {
        "UNKNOWN".to_string()
    }
}
fn get_activation_status() -> String {
    let output = Command::new("cscript")
        .args(["//nologo", "C:\\Windows\\System32\\slmgr.vbs", "/xpr"])
        .output()
        .expect("Failed to execute command");
    let res = String::from_utf8_lossy(&output.stdout)
        .to_string()
        .to_lowercase();
    let not_activated = res.contains("notification mode");
    if not_activated {
        "false".to_string()
    } else {
        "true".to_string()
    }
}
fn get_cpu(info_type: &str) -> Vec<String> {
    let output = Command::new("wmic")
        .arg("cpu")
        .arg("get")
        .arg(info_type)
        .output()
        .expect("Failed to execute command");
    let res = String::from_utf8_lossy(&output.stdout).to_string();
    let res = res
        .trim()
        .split("\n")
        .map(|s| s.trim().to_owned())
        .collect::<Vec<String>>();

    res[1..].to_vec()
}
fn get_os(info_type: &str) -> Vec<String> {
    let output = Command::new("wmic")
        .arg("os")
        .arg("get")
        .arg(info_type)
        .output()
        .expect("Failed to execute command");
    let res = String::from_utf8_lossy(&output.stdout).to_string();
    let res = res
        .trim()
        .split("\n")
        .map(|s| s.trim().to_owned())
        .collect::<Vec<String>>();

    res[1..].to_vec()
}
fn get_bios(info_type: &str) -> Vec<String> {
    let output = Command::new("wmic")
        .arg("bios")
        .arg("get")
        .arg(info_type)
        .output()
        .expect("Failed to execute command");
    let res = String::from_utf8_lossy(&output.stdout).to_string();
    let res = res
        .trim()
        .split("\n")
        .map(|s| s.trim().to_owned())
        .collect::<Vec<String>>();

    res[1..].to_vec()
}
fn get_display(info_type: &str) -> Vec<String> {
    let output = Command::new("wmic")
        .arg("path")
        .arg("Win32_VideoController")
        .arg("get")
        .arg(info_type)
        .output()
        .expect("Failed to execute command");
    let res = String::from_utf8_lossy(&output.stdout).to_string();
    let res = res
        .trim()
        .split("\n")
        .map(|s| s.trim().to_owned())
        .collect::<Vec<String>>();

    res[1..].to_vec()
}

fn get_secure_boot() -> Vec<String> {
    let output = Command::new("powershell")
        .arg("Confirm-SecureBootUEFI")
        .output()
        .expect("Failed to execute command");
    let res = String::from_utf8_lossy(&output.stdout)
        .to_string()
        .to_lowercase();
    let res = res
        .trim()
        .split("\n")
        .map(|s| s.trim().to_owned())
        .collect::<Vec<String>>();
    if output.status.success() {
        res
    } else {
        vec!["false".to_string()]
    }
}
fn get_battery(info_type: &str) -> Option<Vec<String>> {
    let output = Command::new("wmic")
        .arg("path")
        .arg("Win32_Battery")
        .arg("get")
        .arg(info_type)
        .output()
        .expect("Failed to execute command");
    let res = String::from_utf8_lossy(&output.stdout).to_string();
    let res = res
        .trim()
        .split("\n")
        .map(|s| s.trim().to_owned())
        .collect::<Vec<String>>();
    if res.len() > 1 {
        Some(res[1..].to_vec())
    } else {
        None
    }
}
fn get_product(info_type: &str) -> Option<Vec<String>> {
    let output = Command::new("wmic")
        .arg("computersystem")
        .arg("get")
        .arg(info_type)
        .output()
        .expect("Failed to execute command");
    let res = String::from_utf8_lossy(&output.stdout).to_string();
    let res = res
        .trim()
        .split("\n")
        .map(|s| s.trim().to_owned())
        .collect::<Vec<String>>();
    if res.len() > 1 {
        Some(res[1..].to_vec())
    } else {
        None
    }
}
fn get_ddr_type_by_speed(speed: i32) -> String {
    let ddr_speeds = [
        [266, 400],
        [533, 800],
        [800, 1600],
        [2133, 4800],
        [4800, 6400],
    ];
    let mut res = "DDR".to_string();
    for i in (0..ddr_speeds.len()).rev() {
        let range = ddr_speeds[i];

        if speed >= range[0] {
            res = format!("DDR{}", i + 1);
            break;
        }
    }
    res
}

fn detect_rights() -> bool {
    let output = Command::new("net.exe")
        .arg("session")
        .output()
        .expect("Failed to execute command");
    output.status.success()
}

fn parse_bus_type(bus_type: i8) -> String {
    match bus_type {
        3 | 11 => "SATA".to_owned(),
        7 => "USB".to_owned(),
        17 => "NVME".to_owned(),
        _ => "Unknown".to_owned(),
    }
}
fn parse_media_type(media_type: i8) -> String {
    match media_type {
        3 => "HDD".to_owned(),
        4 => "SSD".to_owned(),
        5 => "SCM".to_owned(),
        _ => "Unspecified".to_owned(),
    }
}
