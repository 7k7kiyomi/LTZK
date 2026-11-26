# 陇图智控平台（LTZK）

陇图智控平台的 C++ / Qt / QGIS 源码及部署配置示例，包含数据目录、工作空间、图层操作、插件集成与登录权限相关模块。

## 目录说明

- `LTZK.sln`、`LTZK.vcxproj`：平台解决方案及工程。
- `main.cpp`、`LTZK.cpp`、`LTZK.h`：程序入口与主界面。
- `DataCatalog*`、`Workspace*`：数据目录与工作空间实现。
- `auth/`、`database/`：身份验证界面及数据库权限脚本。
- `Resources/`、`LTZK.qrc`、`.ui`：界面资源。
- `map_product_manager_tools/`：原平台源码目录中附带的成果管理工具源码。
- `deployment/runtime/`：从实际 `E:\LTZK` 部署目录补充的启动脚本、配置和 OCR 服务脚本（如存在）。
- `config/`、`docs/`、`tests/`、`tools/`：配置示例、开发文档、测试及工具。

## 构建与部署

1. 准备 Visual Studio 2019 C++ v142 工具链、Qt 5、QGIS 3.28.15 及相关开发依赖；工程当前使用这些版本和本机路径。
2. 打开 `LTZK.sln`，调整 Qt、QGIS、OSGeo4W 的头文件及库路径后构建 x64 配置。
3. 参考 `deployment/` 和 `docs/` 配置运行环境。`deployment/runtime/start_LTZK.bat` 需要置于完整 LTZK 部署目录使用。
4. 将 `database.ini.example` 等示例复制为相应 `.ini` 文件，并在本地填写连接参数和密码。口令配置不提交到 Git。
5. OCR 服务还需要单独安装 Python/PaddleOCR 环境和模型；仓库不包含其依赖包或模型权重。

## 源码来源

- 平台源码：`E:\AAALTZK`（已由所有者确认）。
- 部署补充：`E:\LTZK`。
- 质检插件：[map_quality_check_tools](https://github.com/7k7kiyomi/map_quality_check_tools)。

## 版本说明

本仓库为 2026-11-26 整理的源码快照。保留现有工程结构、源码、界面资源、测试和开发文档；未包含编译产物、调试符号、历史备份、临时构建目录、数据库口令及第三方运行环境。原目录中的文件未被修改。

工程中的 Qt、QGIS、GDAL 等依赖路径来自原开发机器，首次构建时需按本机安装位置调整。此次归档进行了文件清单、敏感配置及资源引用检查，未执行完整编译或运行验证。
