// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]
mod listen;
mod sql;
mod undo;
mod utils;

use listen::{initialize_driver, start_listening};
use std::path::Path;
use std::sync::OnceLock;
use std::{
    process::Command,
    sync::{Arc, Mutex},
};
use tauri::Manager;
use windows::Win32::Foundation::HANDLE;
use windows::{core::w, Win32::Storage::InstallableFileSystems::FilterConnectCommunicationPort};
static DB_CHANGED: OnceLock<Arc<Mutex<bool>>> = OnceLock::new();
static PORT: OnceLock<HANDLE> = OnceLock::new();
pub const BACKUP_DIR: &str = "C:\\Users\\WDAGUtilityAccount\\Desktop\\folder2";
fn main() {
    std::fs::create_dir_all(BACKUP_DIR).unwrap();
    let db_changed = DB_CHANGED
        .get_or_init(|| Arc::new(Mutex::new(false)))
        .clone();
    tauri::Builder::default()
        .setup(|app| {
            let app = app.app_handle().clone();
            let port;
            let port_name = w!("\\MiniGuard");

            unsafe {
                port = FilterConnectCommunicationPort(port_name, 0, None, 0, None)
                    .expect("could not connect to port");
            }
            PORT.set(port).unwrap();

            let connection = rusqlite::Connection::open("./sq.db").unwrap();

            sql::add_ignore_path(
                &connection,
                format!("{}\\AppData", dirs_next::home_dir().unwrap().display()),
            );
            connection.close().unwrap();
            start_listening(port.clone(), db_changed.clone());
            std::thread::spawn(move || loop {
                if *db_changed.lock().unwrap() == false {
                    std::thread::sleep(std::time::Duration::from_secs(1));
                    continue;
                }
                *db_changed.lock().unwrap() = false;
                let connection = rusqlite::Connection::open("./sq.db").unwrap();
                let logs = sql::get_logs(&connection);
                app.emit_all("updated_logs", &logs).unwrap();
                connection.close().unwrap();
                std::thread::sleep(std::time::Duration::from_secs(1));
            });
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            get_logs,
            run_exe,
            undo,
            get_ignore_paths,
            add_ignore_path,
            remove_ignore_path
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
#[tauri::command]
fn get_logs() -> Vec<sql::Log> {
    let connection = rusqlite::Connection::open("./sq.db").unwrap();
    let logs = sql::get_logs(&connection);
    connection.close().unwrap();
    logs
}
#[tauri::command]
fn run_exe(path: String) -> bool {
    match Command::new(path.clone()).spawn() {
        Ok(_) => {
            let connection = rusqlite::Connection::open("./sq.db").unwrap();

            let path = Path::new(&path);
            sql::add_ignore_path(
                &connection,
                path.parent().unwrap().to_path_buf().display().to_string(),
            );

            connection.close().unwrap();
            true
        }
        Err(_) => false,
    }
}
#[tauri::command]
fn undo(id: i32) {
    println!("undo: {}", id);
    let connection = rusqlite::Connection::open("./sq.db").unwrap();
    undo::undo(&connection, id);
    let db_changed = DB_CHANGED
        .get_or_init(|| Arc::new(Mutex::new(false)))
        .clone();
    *db_changed.lock().unwrap() = true;
    connection.close().unwrap();
}
#[tauri::command]
fn get_ignore_paths() -> Vec<sql::IgnorePath> {
    let connection = rusqlite::Connection::open("./sq.db").unwrap();
    let logs = sql::get_ignore_paths(&connection);
    connection.close().unwrap();
    logs
}
#[tauri::command]
fn add_ignore_path(path: String) {
    let connection = rusqlite::Connection::open("./sq.db").unwrap();
    sql::add_ignore_path(&connection, path);
    let drives = utils::get_drives();
    initialize_driver(&connection, &PORT.get().unwrap(), &drives);
    connection.close().unwrap();
}
#[tauri::command]
fn remove_ignore_path(path_id: i32) {
    let connection = rusqlite::Connection::open("./sq.db").unwrap();
    sql::remove_ignore_path(&connection, path_id);
    let drives = utils::get_drives();
    initialize_driver(&connection, &PORT.get().unwrap(), &drives);
    connection.close().unwrap();
}
