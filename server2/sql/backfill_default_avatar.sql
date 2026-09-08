-- 将缺失头像的用户回填为默认 URL（部署前请按实际 public_base_url 修改）
-- 用法: sqlite3 data/auth.db < server2/sql/backfill_default_avatar.sql

UPDATE sys_user_profiles
SET avatar_url = 'http://127.0.0.1:9000/static/avatar/default.png'
WHERE avatar_url IS NULL OR avatar_url = '';
