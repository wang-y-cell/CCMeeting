-- 演示用户（密码算法：SHA-256 十六进制，与 server2 util::sha256_hex 一致）
-- 用户名: demo
-- 密码:   demo123
-- SHA256(demo123) = d3ad9315b7be5dd53b31a273b3b3aba5defe700808305aa16a3062b76658a791
--
-- 用法: sqlite3 data/auth.db < server2/sql/seed_demo_user.sql

INSERT INTO sys_users (username, password_hash, status)
VALUES (
    'demo',
    'd3ad9315b7be5dd53b31a273b3b3aba5defe700808305aa16a3062b76658a791',
    1
)
ON CONFLICT(username) DO UPDATE SET
    password_hash = excluded.password_hash,
    status = 1;

INSERT INTO sys_user_profiles (user_id, nickname, avatar_url, info)
SELECT
    user_id,
    '演示用户',
    'https://cdn.example.com/avatar/demo.png',
    'CloudMeeting 演示账号'
FROM sys_users
WHERE username = 'demo'
ON CONFLICT(user_id) DO UPDATE SET
    nickname = excluded.nickname,
    avatar_url = excluded.avatar_url,
    info = excluded.info;
