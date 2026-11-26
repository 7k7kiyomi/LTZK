-- Run the first block while connected to PostgreSQL's maintenance database, usually YOUR_PASSWORD.
-- Example:
--   psql -U YOUR_PASSWORD -d YOUR_PASSWORD -f E:/AAALTZK/database/postgis_user_permissions_setup.sql

SELECT 'CREATE DATABASE ltzk_platform WITH ENCODING ''UTF8'''
WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = 'ltzk_platform')\gexec

\connect ltzk_platform

CREATE EXTENSION IF NOT EXISTS postgis;

CREATE TABLE IF NOT EXISTS user_permissions (
    id SERIAL PRIMARY KEY,
    user_name VARCHAR(200) NOT NULL UNIQUE,
    role VARCHAR(50) NOT NULL DEFAULT '数据操作员',
    password_hash VARCHAR(128) NOT NULL DEFAULT '',
    permissions TEXT,
    granted_by VARCHAR(200),
    granted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT chk_user_permissions_role CHECK (
        role IN ('数据操作员', '数据使用者', '数据库管理员')
    )
);

INSERT INTO user_permissions
(user_name, role, password_hash, permissions, granted_by, granted_at)
VALUES
(
  'admin',
  '数据库管理员',
  '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92',
  NULL,
  'system',
  CURRENT_TIMESTAMP
)
ON CONFLICT (user_name) DO UPDATE
SET role = EXCLUDED.role,
    password_hash = EXCLUDED.password_hash,
    granted_by = EXCLUDED.granted_by,
    granted_at = CURRENT_TIMESTAMP;
