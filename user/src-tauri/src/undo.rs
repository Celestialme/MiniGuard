use std::path::Path;

use rusqlite::Connection;

use crate::{sql::Log, OperationType, BACKUP_DIR};

pub fn undo(conn: &Connection, index: i32) {
    let mut stmt = conn
        .prepare("SELECT original_fileName,current_fileName,original_operation, backup_name,date FROM LOGS where id = ?1")
        .unwrap();
    let entry = match stmt.query_row([index], |row| {
        Ok(Log {
            original_file_name: row.get(0).unwrap(),
            current_file_name: row.get(1).unwrap(),
            original_operation: row.get(2).unwrap(),
            backup_name: row.get(3).unwrap(),
            date: row.get(4).unwrap(),
            ..Log::default()
        })
    }) {
        Ok(entry) => entry,
        Err(_) => {
            println!("Entry not found");
            return;
        }
    };

    match entry.original_operation.unwrap() {
        delete_write
            if delete_write == OperationType::Delete as i32
                || delete_write == OperationType::Write as i32 =>
        {
            match entry.backup_name {
                Some(backup_name) => {
                    println!("Undoing {}", entry.original_file_name.as_ref().unwrap());
                    let original_file_name_path = entry.original_file_name.unwrap();
                    let backup_path = &format!("{}\\{}", BACKUP_DIR, backup_name);
                    let _ = std::fs::remove_file(entry.current_file_name.unwrap());
                    match std::fs::copy(backup_path, &original_file_name_path) {
                        Ok(_) => {
                            std::fs::remove_file(backup_path).unwrap();
                            conn.execute(
                                "
                            DELETE FROM LOGS where id = ?1;",
                                (index,),
                            )
                            .unwrap();
                            println!("Date {}", entry.date.as_ref().unwrap());
                            sync(conn, &original_file_name_path, &entry.date.unwrap());
                        }
                        Err(_) => {
                            println!("BACKUP NOT FOUND!!");
                        }
                    }
                }
                None => {}
            }
        }
        create if create == OperationType::Create as i32 => {
            let _ = std::fs::remove_file(entry.current_file_name.unwrap());
            conn.execute(
                "
                DELETE FROM LOGS where id = ?1;",
                (index,),
            )
            .unwrap();
            sync(
                conn,
                &entry.original_file_name.unwrap(),
                &entry.date.unwrap(),
            );
        }
        rename if rename == OperationType::Rename as i32 => {
            let _ = std::fs::rename(
                entry.current_file_name.unwrap(),
                entry.original_file_name.as_ref().unwrap(),
            );
            conn.execute(
                "
                DELETE FROM LOGS where id = ?1;",
                (index,),
            )
            .unwrap();
            sync(
                conn,
                &entry.original_file_name.unwrap(),
                &entry.date.unwrap(),
            );
        }
        _ => {}
    }
}
fn sync(conn: &Connection, original_file_name: &str, date: &str) {
    conn.execute(
        "
    DELETE FROM LOGS WHERE original_fileName = ?1 AND date >= ?2;
    ",
        (original_file_name, date),
    )
    .unwrap();
}
