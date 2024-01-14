mod sql;
mod undo;
mod utils;
use std::{
    ffi::OsString,
    fs::{self, OpenOptions},
    io::{stdin, Read, Write},
    mem,
    os::{raw::c_void, windows::ffi::OsStrExt},
    path::Path,
    process::{self, Command},
};
use utils::parse_ignored_path;
use windows::{
    core::w,
    Win32::{
        Storage::InstallableFileSystems::{
            FilterConnectCommunicationPort, FilterGetMessage, FilterReplyMessage,
            FilterSendMessage, FILTER_MESSAGE_HEADER, FILTER_REPLY_HEADER,
        },
        System::IO::OVERLAPPED,
    },
};

#[repr(C)]
struct Message {
    pid: std::os::raw::c_long,
    array: [[u16; 2048]; 10],
}
#[repr(C)]
struct Receive {
    header: FILTER_MESSAGE_HEADER,
    operation_type: std::os::raw::c_int,
    data: [u16; 2048],
    overlapped: OVERLAPPED,
}

#[repr(i32)]
enum OperationType {
    Create = 1,
    Write = 2,
    Rename = 3,
    Delete = 4,
}

#[repr(C)]
struct Reply {
    header: FILTER_REPLY_HEADER,
    data: [u16; 40],
}
const BACKUP_DIR: &str = "C:\\Users\\WDAGUtilityAccount\\Desktop\\folder2";

fn main() {
    let drives = utils::get_drives();
    let mut ignored_paths: [[u16; 2048]; 10] = [[0; 2048]; 10];
    ignored_paths[0] = parse_ignored_path("C:\\Users\\WDAGUtilityAccount\\AppData", &drives);
    ignored_paths[1] =
        parse_ignored_path("C:\\Users\\WDAGUtilityAccount\\Desktop\\folder", &drives);
    let connection = rusqlite::Connection::open("./sq.db").unwrap();
    sql::setup_tables(&connection);
    let main_pid = process::id();
    let port_name = w!("\\MiniGuard");

    let output_buffer: [u8; 1024] = [0; 1024];
    let mut bytes_returned: u32 = 0;
    let port;
    unsafe {
        port = FilterConnectCommunicationPort(port_name, 0, None, 0, None)
            .expect("could not connect to port");
    }

    unsafe {
        let input_buffer = Message {
            pid: main_pid as _,
            array: ignored_paths,
        };
        let p_input_buffer = &input_buffer as *const Message as *mut c_void;
        println!("input buffer: {:?}", mem::size_of_val(&p_input_buffer));
        FilterSendMessage(port, p_input_buffer, 200, None, 0, &mut bytes_returned)
            .expect("could not send message");
        std::thread::spawn(move || loop {
            let drives = drives.clone();
            const BUFFER_SIZE: usize = 1024 * 5;
            let mut buffer: [u8; BUFFER_SIZE] = [0; BUFFER_SIZE];
            FilterGetMessage(
                port,
                buffer.as_mut_ptr() as *mut _,
                BUFFER_SIZE as u32,
                None,
            )
            .unwrap();
            let data: Receive = std::ptr::read(buffer.as_ptr() as *const _);
            let null_char_position = data
                .data
                .iter()
                .position(|&c| c == 0)
                .unwrap_or(data.data.len());
            let mut text = String::from_utf16(&data.data[..null_char_position])
                .unwrap()
                .to_string();

            for drive in drives.into_iter() {
                text = text.replace(&drive[1], &drive[0]);
            }
            let reply = Reply {
                header: FILTER_REPLY_HEADER {
                    Status: windows::Win32::Foundation::NTSTATUS(0),
                    MessageId: data.header.MessageId,
                },
                data: {
                    let vec = OsString::from("OK\0").encode_wide().collect::<Vec<u16>>();
                    let mut temp: [u16; 40] = [0; 40];
                    for i in 0..vec.len() {
                        temp[i] = vec[i];
                    }
                    temp
                },
            };

            match data.operation_type {
                write if write == OperationType::Write as i32 => {
                    sql::operation_write(&connection, &text);

                    // println!("{}", text);
                    // println!("Write");
                }
                create if create == OperationType::Create as i32 => {
                    sql::operation_create(&connection, &text);

                    println!("Create");
                }
                rename if rename == OperationType::Rename as i32 => {
                    // println!("{}", text);
                    // println!("Rename");
                    sql::operation_rename(&connection, &text);
                }
                delete if delete == OperationType::Delete as i32 => {
                    sql::operation_delete(&connection, &text);
                    // println!("{}", text);
                    // println!("Delete");
                }

                _ => {
                    println!("unknown")
                }
            };
            FilterReplyMessage(
                port,
                &reply as *const Reply as *const FILTER_REPLY_HEADER,
                1024,
            )
            .unwrap();
        });
        let connection = rusqlite::Connection::open("./sq.db").unwrap();
        loop {
            let mut command = String::new();
            stdin().read_line(&mut command).unwrap();
            if command.starts_with("undo") {
                let index = command.split(" ").collect::<Vec<&str>>();
                let index = index[1];
                undo::undo(
                    &connection,
                    match index.trim().parse() {
                        Ok(x) => x,
                        Err(_) => 0,
                    },
                );
            } else {
                match Command::new(command.trim()).spawn() {
                    Ok(_) => {
                        println!("Program started");
                    }
                    Err(_) => {
                        println!("Porgram not found");
                    }
                };
            }
        }
    }
    //TODO chronology

    //delete file called some.txt
    // std::fs::remove_file("some.txt").unwrap();
    // std::fs::File::create("some.txt").unwrap();
    // std::fs::rename("some.txt", "other.txt").unwrap();
    // std::fs::remove_file("some.txt").unwrap();
    // let content = std::fs::read("some.txt").unwrap();
    // println!("{}", String::from_utf8(content).unwrap());
    // let mut file = std::fs::File::create("some.txt").unwrap();

    // let mut file = OpenOptions::new()
    //     .write(true)
    //     .append(true)
    //     .open("some.txt")
    //     .unwrap();

    // file.write_all(b"Hello, world!").unwrap();
}
