-- 旧库手动迁移（服务启动时也会自动 ALTER）
ALTER TABLE sys_user_profiles ADD COLUMN gender TEXT;
ALTER TABLE sys_user_profiles ADD COLUMN birthday TEXT;
ALTER TABLE sys_user_profiles ADD COLUMN address TEXT;
ALTER TABLE sys_user_profiles ADD COLUMN phone TEXT;
ALTER TABLE sys_user_profiles ADD COLUMN email TEXT;
ALTER TABLE sys_user_profiles ADD COLUMN extra_json TEXT;
