-- CloudMeeting 认证库表结构
CREATE DATABASE IF NOT EXISTS CCMeeting
  DEFAULT CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE CCMeeting;

CREATE TABLE IF NOT EXISTS sys_users (
    user_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '用户唯一标识',
    username VARCHAR(50) NOT NULL COMMENT '用户名（登录凭证）',
    password_hash VARCHAR(255) NOT NULL COMMENT '加密后的密码哈希值',
    status TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '账号状态：1-正常, 2-禁用, 3-锁定',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (user_id),
    UNIQUE KEY uk_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户主表';

CREATE TABLE IF NOT EXISTS sys_user_profiles (
    user_id BIGINT UNSIGNED NOT NULL COMMENT '关联用户主表ID',
    nickname VARCHAR(100) DEFAULT NULL COMMENT '用户昵称',
    avatar_url VARCHAR(500) DEFAULT NULL COMMENT '头像CDN地址',
    info TEXT COMMENT '个人简介/信息',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户资料表';

CREATE TABLE IF NOT EXISTS sys_roles (
    role_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '角色主键',
    role_name VARCHAR(50) NOT NULL COMMENT '角色名称',
    role_key VARCHAR(50) NOT NULL COMMENT '角色唯一标识符(如admin)',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    PRIMARY KEY (role_id),
    UNIQUE KEY uk_role_key (role_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='角色表';

CREATE TABLE IF NOT EXISTS sys_permissions (
    perm_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '权限主键',
    perm_code VARCHAR(100) NOT NULL COMMENT '权限标识(如sys:user:add)',
    description VARCHAR(255) DEFAULT NULL COMMENT '权限描述',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    PRIMARY KEY (perm_id),
    UNIQUE KEY uk_perm_code (perm_code)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='权限表';

CREATE TABLE IF NOT EXISTS sys_user_roles (
    user_id BIGINT UNSIGNED NOT NULL COMMENT '用户ID',
    role_id BIGINT UNSIGNED NOT NULL COMMENT '角色ID',
    PRIMARY KEY (user_id, role_id),
    INDEX idx_role_id (role_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户与角色关联表';

CREATE TABLE IF NOT EXISTS sys_role_permissions (
    role_id BIGINT UNSIGNED NOT NULL COMMENT '角色ID',
    perm_id BIGINT UNSIGNED NOT NULL COMMENT '权限ID',
    PRIMARY KEY (role_id, perm_id),
    INDEX idx_perm_id (perm_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='角色与权限关联表';

CREATE TABLE IF NOT EXISTS sys_user_login_logs (
    log_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '日志主键',
    user_id BIGINT UNSIGNED NOT NULL COMMENT '用户ID',
    login_ip VARCHAR(45) NOT NULL COMMENT '登录IP地址',
    device_info VARCHAR(255) DEFAULT NULL COMMENT '设备/客户端信息',
    status TINYINT UNSIGNED NOT NULL COMMENT '登录状态：1-成功, 0-失败',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '登录时间',
    PRIMARY KEY (log_id),
    INDEX idx_user_id (user_id),
    INDEX idx_created_at (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户登录日志表';
