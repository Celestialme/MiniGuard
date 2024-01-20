use crate::{
    sql,
    utils::{self, parse_ignored_path},
};
use rusqlite::Connection;
use std::{
    ffi::OsString,
    fs::{self, OpenOptions},
    io::{stdin, Read, Write},
    mem,
    os::{raw::c_void, windows::ffi::OsStrExt},
    path::Path,
    process::{self, Command},
    sync::{Arc, Mutex},
};
use tauri::async_runtime::TokioHandle;
use windows::{
    core::w,
    Win32::{
        Foundation::HANDLE,
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
pub enum OperationType {
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

pub fn initialize_driver(connection: &Connection, port: &HANDLE, drives: &Vec<[String; 2]>) {
    let ignored_paths_vec = sql::get_ignore_paths(&connection);
    let mut ignored_paths: [[u16; 2048]; 10] = [[0; 2048]; 10];
    for i in 0..ignored_paths_vec.len() {
        let path = parse_ignored_path(&ignored_paths_vec[i].path, &drives);
        ignored_paths[i] = path;
    }
    let mut bytes_returned: u32 = 0;
    let main_pid = process::id();
    let input_buffer = Message {
        pid: main_pid as _,
        array: ignored_paths,
    };
    let p_input_buffer = &input_buffer as *const Message as *mut c_void;
    unsafe {
        FilterSendMessage(
            port.clone(),
            p_input_buffer,
            200,
            None,
            0,
            &mut bytes_returned,
        )
        .expect("could not send message");
    }
}

pub fn start_listening(port: HANDLE, db_changed: Arc<Mutex<bool>>) {
    let connection = rusqlite::Connection::open("./sq.db").unwrap();
    sql::setup_tables(&connection);

    let drives = utils::get_drives();
    initialize_driver(&connection, &port, &drives);
    unsafe {
        std::thread::spawn(move || loop {
            let drives = drives.clone();
            const BUFFER_SIZE: usize = 1024 * 5;
            let mut buffer: [u8; BUFFER_SIZE] = [0; BUFFER_SIZE];
            match FilterGetMessage(
                port,
                buffer.as_mut_ptr() as *mut _,
                BUFFER_SIZE as u32,
                None,
            ) {
                Ok(_) => {}
                Err(_) => continue,
            };
            *db_changed.lock().unwrap() = true;
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
    }
}
