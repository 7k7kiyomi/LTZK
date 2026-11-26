# LTZK PostGIS 登录权限配置

数据库连接配置使用 LTZK 安装目录下的 `config\database.ini`。运行包内应放在：

`LTZK\config\database.ini`

开发调试时也可以使用当前源码目录或编译输出目录下的 `config\database.ini`。

打包运行时，程序会读取 `bin\LTZK.exe` 上一级安装目录下的 `config\database.ini`，即 `LTZK\config\database.ini`。不再依赖 `QSettings("GarMap", "LTZK")` 或 `QSettings("GarMap", "MapProductManager")` 作为连接来源。

## 1. 创建 PostGIS 库

在已安装 PostgreSQL + PostGIS 的机器上执行：

```powershell
psql -U YOUR_PASSWORD -d YOUR_PASSWORD -f .\database\postgis_user_permissions_setup.sql
```

这个脚本会创建 `ltzk_platform` 数据库，连接到该库，启用 PostGIS，并创建 `user_permissions` 表。

## 2. 配置连接参数

编辑：

`LTZK\config\database.ini`

默认内容：

```ini
[db]
host=localhost
port=5432
database=ltzk_platform
schema=public
user=YOUR_PASSWORD
password=
```

如果 PostgreSQL 的 `YOUR_PASSWORD` 用户有密码，把密码写到 `password=` 后面。

## 3. 默认管理员

初始化账号：

- 用户名：admin
- 密码：123456
- 角色：数据库管理员

程序登录时也会自动检查 `user_permissions` 表。如果库已经存在但表不存在，会自动建表；如果 admin 不存在，会自动补 `admin / 123456`。

## 4. 角色规则

- 数据库管理员：四大模块 + 权限管理，最高权限。
- 数据操作员：可进入地图成果管理，支持上传、删除、回溯等数据操作。
- 数据使用者：可登录访问智能制图、地图成果质检、地图服务管理，只读访问和下载数据。


