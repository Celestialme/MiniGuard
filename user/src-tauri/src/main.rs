// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]
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
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![get_logs])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
#[tauri::command]
fn get_logs() -> Vec<sql::Log> {
    let connection = rusqlite::Connection::open("./sq.db").unwrap();
    let logs = sql::get_logs(&connection);
    println!("{:?}", logs);
    println!("I was invoked from JS!");
    logs
}
