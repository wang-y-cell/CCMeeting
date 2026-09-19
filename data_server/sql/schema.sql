-- CloudMeeting 认证库表结构（SQLite）
-- 一般由服务启动时自动建表；也可手动：
--   sqlite3 data/auth.db < server2/sql/schema.sql

CREATE TABLE IF NOT EXISTS sys_users (
    user_id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    status INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS sys_user_profiles (
    user_id INTEGER PRIMARY KEY,
    nickname TEXT,
    avatar_url TEXT,
    info TEXT,
    gender TEXT,
    birthday TEXT,
    address TEXT,
    phone TEXT,
    email TEXT,
    extra_json TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (user_id) REFERENCES sys_users(user_id)
);

CREATE TABLE IF NOT EXISTS sys_roles (
    role_id INTEGER PRIMARY KEY AUTOINCREMENT,
    role_name TEXT NOT NULL,
    role_key TEXT NOT NULL UNIQUE,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS sys_permissions (
    perm_id INTEGER PRIMARY KEY AUTOINCREMENT,
    perm_code TEXT NOT NULL UNIQUE,
    description TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS sys_user_roles (
    user_id INTEGER NOT NULL,
    role_id INTEGER NOT NULL,
    PRIMARY KEY (user_id, role_id)
);

CREATE TABLE IF NOT EXISTS sys_role_permissions (
    role_id INTEGER NOT NULL,
    perm_id INTEGER NOT NULL,
    PRIMARY KEY (role_id, perm_id)
);

CREATE TABLE IF NOT EXISTS sys_user_login_logs (
    log_id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    login_ip TEXT NOT NULL,
    device_info TEXT,
    status INTEGER NOT NULL,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_login_logs_user_id ON sys_user_login_logs(user_id);
CREATE INDEX IF NOT EXISTS idx_login_logs_created_at ON sys_user_login_logs(created_at);
