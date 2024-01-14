use std::{ffi::OsString, iter::once, os::windows::ffi::OsStrExt, path::Path};

use windows::{
    core::PCWSTR,
    Win32::Storage::FileSystem::{GetLogicalDrives, QueryDosDeviceW},
};

pub fn get_drives() -> Vec<[String; 2]> {
    let mut result: Vec<[String; 2]> = vec![];
    for drive in b'A'..=b'Z' {
        let bit_position = drive - b'A';

        if (unsafe { GetLogicalDrives() } & (1 << bit_position)) != 0 {
            let mut buffer = [0u16; 1024];
            let path_wide: Vec<u16> = OsString::from(format!("{}:", drive as char))
                .encode_wide()
                .chain(once(0))
                .collect();
            unsafe {
                QueryDosDeviceW(PCWSTR(path_wide.as_ptr()), Some(&mut buffer));
            }
            let null_char_position = buffer.iter().position(|&c| c == 0).unwrap_or(buffer.len());

            result.push([
                format!("{}:", drive as char),
                String::from_utf16(&buffer[..null_char_position])
                    .unwrap()
                    .to_string(),
            ]);

            // println!("{}:{}", drive as char, String::from_utf16_lossy(&buffer));
        }
    }
    result
}

pub fn make_backup(file_path: &str) -> Option<String> {
    let path = Path::new(&file_path);
    let backup_name = uuid::Uuid::new_v4().to_string();
    if path.exists() && path.is_file() {
        let max_attempts = 5;
        let mut attempt = 0;
        loop {
            match std::fs::copy(
                path,
                format!(
                    "C:\\Users\\WDAGUtilityAccount\\Desktop\\folder2\\{}",
                    backup_name
                ),
            ) {
                Ok(_) => {
                    return Some(backup_name);
                }
                Err(err) => {
                    println!("Error backing up {} \n Error: {}", file_path, err);
                    //if error is not handle lock dont attempt more.
                    if let Some(raw_os_err) = err.raw_os_error() {
                        if raw_os_err != 32 {
                            return None;
                        }
                    }

                    // Increment the attempt counter
                    attempt += 1;

                    // Check if we have reached the maximum number of attempts
                    if attempt >= max_attempts {
                        println!("Max attempts reached. Backup failed.");
                        return None;
                    }

                    // Wait for a short duration before retrying
                    println!("Retrying in 1 second...");
                    std::thread::sleep(std::time::Duration::from_secs(1));
                }
            }
        }
    }
    return None;
}

pub fn parse_ignored_path(string: &str, drives: &Vec<[String; 2]>) -> [u16; 2048] {
    let mut path: String = string.to_string();
    for drive in drives.into_iter() {
        path = path.replace(&drive[0], &drive[1]);
    }

    let vec = OsString::from(format!("{}\0", path))
        .encode_wide()
        .collect::<Vec<u16>>();
    let mut temp: [u16; 2048] = [0; 2048];
    for i in 0..vec.len() {
        temp[i] = vec[i];
    }
    temp
}
