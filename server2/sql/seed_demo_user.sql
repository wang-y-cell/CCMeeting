-- 演示用户（密码算法：SHA-256 十六进制，与 server2 util::sha256_hex 一致）
-- 用户名: demo
-- 密码:   demo123
-- SHA256(demo123) = d3ad9315b7be5dd53b31a273b3b3aba5defe700808305aa16a3062b76658a791

USE CCMeeting;

INSERT INTO sys_users (username, password_hash, status)
VALUES (
    'demo',
    'd3ad9315b7be5dd53b31a273b3b3aba5defe700808305aa16a3062b76658a791',
    1
)
ON DUPLICATE KEY UPDATE
    password_hash = VALUES(password_hash),
    status = 1;

SET @uid := (SELECT user_id FROM sys_users WHERE username = 'demo' LIMIT 1);

INSERT INTO sys_user_profiles (user_id, nickname, avatar_url, info)
VALUES (
    @uid,
    '演示用户',
    'https://cdn.example.com/avatar/demo.png',
    'CloudMeeting 演示账号'
)
ON DUPLICATE KEY UPDATE
    nickname = VALUES(nickname),
    avatar_url = VALUES(avatar_url),
    info = VALUES(info);
