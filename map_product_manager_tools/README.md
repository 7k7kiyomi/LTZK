# 地图成果管理工具 (Map Product Manager Tools)

## 概述

基于 PostGIS + Qt 5.15.2 + QGIS 3.28.15 + GDAL 的 QGIS C++ 插件，实现地图制图成果的全生命周期管理。

## 功能模块

### 1. 成果存储与管理
- 支持矢量（Shapefile、GeoJSON、KML）、栅格（TIF、IMG、JP2）、AI/CDR/PDF 制图源文件等多类型成果的集中上传与存储
- 自动提取空间元数据（坐标范围、CRS、几何类型、分辨率等）
- 版本化文件存储，SHA256 完整性校验
- 缩略图自动生成

### 2. 元数据管理
- GDAL 自动提取空间元数据信息
- 人工补录：比例尺、编制信息、审图号、密级、生产单位等
- 多级目录分类管理
- 标签系统，支持多标签分类

### 3. 成果检索与预览
- 多条件组合检索（关键词、类型、比例尺、密级、日期范围、空间范围等）
- 缩略图在线预览
- 详细元数据展示
- 查询结果 CSV 导出（支持 Excel）

### 4. 版本回溯与差异比对
- 全版本修改记录查看
- 版本详情展示（文件哈希、大小、变更人、时间）
- 任意两个版本差异比对报告
- 版本回退（生成新版本，保留完整历史）

### 5. 权限管理
- 四级访问权限：查看者、编辑者、审核者、管理员
- 按岗位职责划分权限
- 审计日志记录

## 项目结构

```
map_product_manager_tools/
├── map_product_manager_tools.h         # 主插件头文件
├── map_product_manager_tools.cpp       # 主插件实现（QGIS入口、菜单注册）
├── map_product_manager_tools.qrc       # Qt资源文件
├── map_product_manager_tools.vcxproj   # VS2019项目文件
├── map_product_manager_tools.vcxproj.filters
├── README.md
├── sql/
│   └── init_database.sql              # 数据库初始化脚本
├── database/
│   ├── postgis_connector.h/cpp        # PostGIS连接器（libpq）
│   ├── product_metadata.h/cpp         # 数据模型定义
│   ├── product_dao.h/cpp              # 数据访问层（CRUD）
│   └── schema_manager.h/cpp           # Schema管理
├── core/
│   ├── metadata_extractor.h/cpp       # GDAL元数据自动提取
│   └── file_storage_manager.h/cpp     # 文件存储管理
├── ui_class/
│   ├── product_storage_dialog.h/cpp   # 成果存储与管理UI
│   ├── metadata_manager_dialog.h/cpp  # 元数据管理UI
│   ├── product_search_dialog.h/cpp    # 成果检索与预览UI
│   ├── version_diff_dialog.h/cpp      # 版本回溯与差异比对UI
│   ├── access_control_dialog.h/cpp    # 权限管理UI
│   └── db_config_dialog.h/cpp         # 数据库配置UI
└── images/                            # 图标资源
```

## 技术栈

| 组件 | 版本 | 用途 |
|------|------|------|
| Qt | 5.15.2 (MSVC 2019 x64) | GUI框架 |
| QGIS | 3.28.15 | GIS平台集成 |
| GDAL | OSGeo4W | 空间数据元数据提取 |
| PostgreSQL | 14+ | 数据库 |
| PostGIS | 3.x | 空间数据库扩展 |
| libpq | PostgreSQL自带 | C语言数据库连接 |
| MSVC | 2019 (v142) | 编译器 |

## 构建

### 环境要求

- Windows 10/11 x64
- Visual Studio 2019
- Qt 5.15.2 MSVC 2019 x64 (路径: `D:\Qt\5.15.2\msvc2019_64`)
- OSGeo4W (含 QGIS 3.28.15 + GDAL)
- PostgreSQL 14+ with PostGIS

### 环境变量

- `GARMAP_OSGEO4W_34012`: OSGeo4W 安装路径
- `QTDIR_OSGEO4W`: OSGeo4W 内 Qt 路径（用于 MOC 工具）

### 编译步骤

1. 打开 `map_product_manager_tools.vcxproj`（Visual Studio 2019）
2. 选择 `Release | x64` 配置
3. 生成 -> 生成解决方案
4. 输出 DLL: `D:\GarMap\garmap_release\winx64_vc16\obj\plugins\map_product_manager_tools.dll`

## 数据库初始化

### 方式一：通过插件UI

启动 QGIS，加载插件后会自动弹出数据库配置对话框，填写连接参数后自动初始化 Schema。

### 方式二：手动执行 SQL

```bash
psql -U YOUR_PASSWORD -d map_products -f sql/init_database.sql
```

## 数据库表结构

| 表名 | 说明 |
|------|------|
| `product_metadata` | 成果元数据主表（含空间几何字段） |
| `version_records` | 版本记录表 |
| `product_directory` | 多级目录表 |
| `product_tags` | 标签表 |
| `product_tag_mapping` | 产品-标签关联表 |
| `user_permissions` | 用户权限表 |
| `audit_log` | 审计日志表 |

## 使用流程

1. **首次使用**：配置数据库连接 → 自动初始化 Schema
2. **上传成果**：成果存储与管理 → 选择文件 → 自动提取元数据 → 上传
3. **完善元数据**：元数据管理 → 选择产品 → 补录比例尺/审图号/密级等信息
4. **检索成果**：成果检索与预览 → 输入条件 → 查看结果 → 预览缩略图
5. **版本管理**：版本回溯与差异比对 → 选择产品 → 查看历史 → 比对差异
6. **权限管理**：权限管理 → 添加用户 → 分配角色权限
