use std::default;

use rusqlite::Connection;

use crate::utils::make_backup;
#[derive(Debug, serde::Serialize)]
pub struct Log {
    pub id: Option<i32>,
    pub original_file_name: Option<String>,
    pub current_file_name: Option<String>,
    pub original_operation: Option<i32>,
    pub current_operation: Option<i32>,
    pub backup_name: Option<String>,
    pub is_alive: Option<bool>,
    pub count: Option<i32>,
    pub date: Option<String>,
}
#[derive(Debug, serde::Serialize)]
pub struct IgnorePath {
    pub id: Option<i32>,
    pub path: String,
}
impl Default for Log {
    fn default() -> Self {
        Log {
            id: None,
            original_file_name: Some(String::from("")),
            current_file_name: Some(String::from("")),
            original_operation: None,
            current_operation: None,
            backup_name: Some(String::from("")),
            is_alive: Some(true),
            count: Some(0),
            date: None,
        }
    }
}
pub fn setup_tables(conn: &Connection) {
    conn.execute(
        "CREATE TABLE IF NOT EXISTS LOGS (
    id    INTEGER PRIMARY KEY,
    original_fileName  TEXT NOT NULL ,
    current_fileName  TEXT NOT NULL ,
    original_operation  INTEGER NOT NULL,
    current_operation  INTEGER NOT NULL,
    backup_name  TEXT,
    isAlive  BOOLEAN DEFAULT 1,
    count	INTEGER,
    date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (current_fileName,isAlive)
)",
        (), // empty list of parameters.
    )
    .unwrap();

    conn.execute(
        "CREATE TABLE IF NOT EXISTS IGNORE_PATHS (
    id    INTEGER PRIMARY KEY,
    path  TEXT NOT NULL UNIQUE
)",
        (), // empty list of parameters.
    )
    .unwrap();
}

pub fn operation_write(conn: &Connection, file_name: &str) {
    let rows =  conn.execute(
        "INSERT OR IGNORE INTO LOGS (original_fileName,current_fileName,original_operation,current_operation,backup_name,count) 
        VALUES (?1 , ?2, ?3, ?4, ?5, ?6)
       ;",
        (&file_name,&file_name,2,2,"",0),
    )
    .unwrap();
    if rows > 0 {
        // if original operation is write, we will make backup. dont update deleted files
        match make_backup(&file_name) {
            Some(backup_name) => {
                conn.execute(
                    "UPDATE OR IGNORE LOGS SET backup_name = ?1  WHERE 
                    current_fileName = ?2 AND isAlive = 1;",
                    (&backup_name, &file_name),
                )
                .unwrap();
            }
            None => {
                conn.execute(
                    "DELETE FROM LOGS WHERE  current_fileName = ?1;",
                    (&file_name,),
                )
                .unwrap();
            }
        };
    }
    //if current file name exist and is alive update count and current operation
    conn.execute(
        "UPDATE OR IGNORE LOGS SET count = count + 1, current_operation = 2, date = CURRENT_TIMESTAMP WHERE 
         current_fileName = ?1 AND isAlive = 1;",
        (&file_name,),
    )
    .unwrap();
    //if original operation was rename set it to write since rename does not produce backup
    let rows = conn
        .execute(
            "UPDATE OR IGNORE LOGS SET  original_operation = 2 WHERE 
            original_operation = 3
            AND
            current_fileName = ?1
           ;",
            (&file_name,),
        )
        .unwrap();
    if rows > 0 {
        //make backup if we changed rename to write
        let backup_name = make_backup(&file_name);
        conn.execute(
            "UPDATE OR IGNORE LOGS SET backup_name = ?1  WHERE 
        current_fileName = ?2 AND isAlive = 1;",
            (&backup_name, &file_name),
        )
        .unwrap();
        return;
    }
}
pub fn operation_rename(conn: &Connection, rename_path: &str) {
    let parts: Vec<&str> = rename_path.split("->").collect();
    let original_file_name = parts[0];
    let current_file_name = parts[1];
    // rename file, if from_name is same as current_filename dont make new entry since its just rename chain.
    conn.execute(
        "INSERT OR IGNORE INTO LOGS (original_fileName,current_fileName,original_operation,current_operation,count) 
        SELECT ?1 , ?2, ?3, ?4, ?5
         WHERE NOT EXISTS (
            SELECT 1
            FROM LOGS
            WHERE current_fileName = ?1 
        );",
        (&original_file_name,&current_file_name,3,3,0),
    )
    .unwrap();
    //if from_name is same as current_filename, update old entry with new current_filename
    conn.execute(
        "UPDATE OR IGNORE LOGS SET count = count + 1, current_operation = 3, current_fileName = ?2, date = CURRENT_TIMESTAMP   WHERE 
         current_fileName = ?1 AND isAlive = 1;",
        (&original_file_name, &current_file_name),
    )
    .unwrap();
}
pub fn operation_create(conn: &Connection, create_path: &str) {
    println!("create {}", create_path);
    conn.execute(
        "INSERT OR IGNORE INTO LOGS (original_fileName,current_fileName,original_operation,current_operation,count) 
        VALUES (?1 , ?2, ?3, ?4, ?5);",
        (&create_path,&create_path,1,1,0),
    )
    .unwrap();
    conn.execute(
        "UPDATE OR IGNORE LOGS SET count = count + 1, current_operation = 1,date = CURRENT_TIMESTAMP  WHERE 
        current_fileName = ?1;",
        (&create_path,),
    )
    .unwrap();
}
pub fn operation_delete(conn: &Connection, delete_path: &str) {
    // if file was created by us, and because we dont have backup just delete entry.
    let row_count = conn
        .execute(
            "DELETE FROM LOGS WHERE  current_fileName = ?1 AND original_operation = 1 ;",
            (&delete_path,),
        )
        .unwrap();
    if row_count != 0 {
        println!("deleting from dB {}", delete_path);
        return;
    }
    let rows = conn.execute(
        "INSERT OR IGNORE INTO LOGS (original_fileName,current_fileName,original_operation,current_operation,backup_name,isAlive,count) 
        SELECT  ?1 , ?2, ?3, ?4, ?5, ?6 , ?7
        WHERE NOT EXISTS (
            SELECT 1
            FROM LOGS
            WHERE current_fileName = ?1 
        );",
        (&delete_path,&delete_path,4,4,"",0,0),
    )
    .unwrap();
    if rows > 0 {
        // if original operation is delete, we will make backup
        match make_backup(&delete_path) {
            Some(backup_name) => {
                conn.execute(
                    "UPDATE OR IGNORE LOGS SET backup_name = ?1  WHERE 
                    current_fileName = ?2;",
                    (&backup_name, &delete_path),
                )
                .unwrap();
            }
            None => {
                conn.execute(
                    "DELETE FROM LOGS WHERE  current_fileName = ?1;",
                    (&delete_path,),
                )
                .unwrap();
            }
        }
        return;
    }
    //if original operation was rename set it to DELETE since rename does not produce backup
    conn.execute(
        "UPDATE OR IGNORE LOGS SET count = count + 1, current_operation = 4, isAlive = 0,date = CURRENT_TIMESTAMP  WHERE 
        current_fileName = ?1;",
        (&delete_path,),
    )
    .unwrap();

    let rows = conn
        .execute(
            "UPDATE OR IGNORE LOGS SET  original_operation = 4, isAlive = 0 WHERE 
        original_operation = 3
        AND
         current_fileName = ?1;",
            (&delete_path,),
        )
        .unwrap();
    if rows > 0 {
        //make backup if we changed rename to write
        let backup_name = make_backup(&delete_path);

        conn.execute(
            "UPDATE OR IGNORE LOGS SET backup_name = ?1  WHERE 
            current_fileName = ?2 ",
            (&backup_name, &delete_path),
        )
        .unwrap();
        return;
    }
}
pub fn get_logs(conn: &Connection) -> Vec<Log> {
    let mut stmt = conn
    .prepare("SELECT id, original_fileName,current_fileName,original_operation,current_operation, backup_name,isAlive,count,date FROM LOGS LIMIT 100 ;")
    .unwrap();
    let mut logs: Vec<Log> = vec![];
    match stmt.query_map([], |row| {
        Ok(Log {
            id: row.get(0).unwrap(),
            original_file_name: row.get(1).unwrap(),
            current_file_name: row.get(2).unwrap(),
            original_operation: row.get(3).unwrap(),
            current_operation: row.get(4).unwrap(),
            backup_name: row.get(5).unwrap(),
            is_alive: row.get(6).unwrap(),
            count: row.get(7).unwrap(),
            date: row.get(8).unwrap(),
            ..Log::default()
        })
    }) {
        Ok(entry) => {
            for e in entry {
                logs.push(e.unwrap());
            }
        }
        Err(_) => {}
    };
    logs
}

pub fn get_ignore_paths(conn: &Connection) -> Vec<IgnorePath> {
    let mut stmt = conn.prepare("SELECT  id,path from IGNORE_PATHS;").unwrap();
    let mut paths: Vec<IgnorePath> = vec![];
    match stmt.query_map([], |row| {
        Ok(IgnorePath {
            id: row.get(0).unwrap(),
            path: row.get(1).unwrap(),
        })
    }) {
        Ok(entry) => {
            for e in entry {
                paths.push(e.unwrap());
            }
        }
        Err(_) => {}
    };
    paths
}

pub fn add_ignore_path(conn: &Connection, path: String) {
    conn.execute(
        "INSERT OR IGNORE INTO IGNORE_PATHS (path) values (?1)
        ;",
        (&path,),
    )
    .unwrap();
}
pub fn remove_ignore_path(conn: &Connection, path_id: i32) {
    conn.execute("DELETE FROM IGNORE_PATHS WHERE  id = ?1;", (&path_id,))
        .unwrap();
}
