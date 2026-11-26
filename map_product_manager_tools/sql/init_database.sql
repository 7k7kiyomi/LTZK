-- ============================================================================
-- 地图服务管理系统 - 数据库初始化脚本 (UNV2)
-- 支持 PostgreSQL
-- 变更摘要：
--   - 去掉状态/经费栏（UI不再显示，数据库保留兼容）
--   - arranged_leader → arranged_office（安排处室）
--   - 新增 service_level, other_service_form, map_name, mapping_region,
--     size, material, compilation_details
--   - 图件种类用 compilation_details JSON 存储
-- ============================================================================

-- ----------------------------------------------------------------------------
-- 1. 服务单位表（简化版，仅存储单位名称，其他信息在服务单中体现）
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS service_units (
    id          SERIAL PRIMARY KEY,
    name        VARCHAR(255) NOT NULL UNIQUE,
    created_at  TIMESTAMP DEFAULT NOW()
);

-- ----------------------------------------------------------------------------
-- 2. 服务记录表 (UNV2)
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS service_records (
    id                      SERIAL PRIMARY KEY,

    -- 服务单号
    service_number          VARCHAR(64) NOT NULL,

    -- 基本信息
    record_date             VARCHAR(32),                -- 日期 (yyyy-MM-dd)
    application_scenario    VARCHAR(64),                 -- 应用场景（日常政务服务/政务调研用图/应急地图服务）
    service_content         TEXT,                       -- 服务内容，支持多行
    coverage_area           VARCHAR(512),               -- 覆盖区域

    -- 分类与级别
    service_level           VARCHAR(64),                -- 服务等级（国家级/省级/厅级/其他政府单位）
    arranged_office         VARCHAR(256),               -- 安排处室（处室+安排领导）
    category                VARCHAR(128),               -- 服务类别（110：省委秘书一处等）
    service_form            VARCHAR(256),               -- 服务形式（6种）
    other_service_form      VARCHAR(256),               -- 自定义服务形式（选"其他"时填写）

    -- 地图编制详细字段
    map_name                VARCHAR(512),               -- 图名
    mapping_region          VARCHAR(512),               -- 制图区域
    size                    VARCHAR(128),               -- 尺寸
    material                VARCHAR(128),               -- 材质
    compilation_details     TEXT,                       -- 图件种类+数量 JSON
    -- UNV3：各服务形式Tab详情（JSON存储）
    print_details           TEXT,                       -- 地图打印Tab详情
    mounting_details        TEXT,                       -- 地图装裱Tab详情
    product_details         TEXT,                       -- 地图成品Tab详情
    data_details            TEXT,                       -- 数据发送Tab详情
    other_details           TEXT,                       -- 其他Tab详情

    -- 服务数量
    sheet_count             INTEGER DEFAULT 0,          -- 服务数量

    -- 服务单位
    unit_id                 INTEGER DEFAULT 0,
    unit_name               VARCHAR(255),               -- 服务单位

    -- 时间与接收
    submit_time             VARCHAR(32),                 -- 服务提交时间
    receiver                VARCHAR(256),               -- 接收人姓名及电话

    -- 关联地图成果
    product_ids             TEXT,                       -- 关联成果ID（逗号分隔）
    product_names           TEXT,                       -- 关联成果名称（逗号分隔）

    -- 状态（保留兼容，UI不再显示）
    status                  INTEGER DEFAULT 0,          -- 0:处理中 1:已完成 2:已取消 3:已归档
    arranged_leader         VARCHAR(128),               -- [废弃] 安排领导
    funding                 NUMERIC(12, 2) DEFAULT 0,   -- [废弃] 经费（元）

    -- 备注与归档
    remarks                 TEXT,                       -- 备注
    archive_path            VARCHAR(1024),              -- 电子版归档路径
    archive_format          VARCHAR(64),                -- 归档格式
    approval_file_path      VARCHAR(1024),              -- 电子审批单文件路径

    -- 审计信息
    created_by              VARCHAR(128) DEFAULT 'system',
    created_at              TIMESTAMP DEFAULT NOW(),
    updated_by              VARCHAR(128),
    updated_at              TIMESTAMP
);

-- ----------------------------------------------------------------------------
-- 3. 索引
-- ----------------------------------------------------------------------------
CREATE INDEX IF NOT EXISTS idx_service_records_number
    ON service_records(service_number);
CREATE INDEX IF NOT EXISTS idx_service_records_date
    ON service_records(record_date);
CREATE INDEX IF NOT EXISTS idx_service_records_unit
    ON service_records(unit_name);
CREATE INDEX IF NOT EXISTS idx_service_records_category
    ON service_records(category);
CREATE INDEX IF NOT EXISTS idx_service_records_service_level
    ON service_records(service_level);
CREATE INDEX IF NOT EXISTS idx_service_records_status
    ON service_records(status);
CREATE INDEX IF NOT EXISTS idx_service_records_created_at
    ON service_records(created_at);

-- ----------------------------------------------------------------------------
-- 4. UNV2 新字段迁移（已有数据库执行，新增数据库自动创建）
-- ----------------------------------------------------------------------------
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS service_level VARCHAR(64);
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS arranged_office VARCHAR(256);
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS other_service_form VARCHAR(256);
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS map_name VARCHAR(512);
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS mapping_region VARCHAR(512);
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS size VARCHAR(128);
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS material VARCHAR(128);
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS compilation_details TEXT;
-- UNV3 新增列
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS print_details TEXT;
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS mounting_details TEXT;
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS product_details TEXT;
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS data_details TEXT;
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS other_details TEXT;
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS product_ids TEXT;
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS product_names TEXT;
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS approval_file_path VARCHAR(1024);
ALTER TABLE service_records ADD COLUMN IF NOT EXISTS approval_file_oid INTEGER DEFAULT 0;
ALTER TABLE service_records ALTER COLUMN service_form TYPE VARCHAR(256);
