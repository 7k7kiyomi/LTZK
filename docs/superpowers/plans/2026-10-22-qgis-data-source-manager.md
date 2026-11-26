# QGIS 原生数据源管理器实现计划

**目标：** 接入当前 QGIS 3.28.15 原生数据源管理器，实现容器子图层选择、GeoPDF 加载和平台图层注册。

**架构：** WorkspaceDataSourceLoader 负责提供者查询、原生子图层选择及图层创建；WorkspaceDataSources.cpp 负责管理器信号、项目注册、图层替换和画布定位。原有按钮、浏览器及拖放复用同一加载路径。

**技术：** Qt 5 / C++17 / QGIS 3.28.15 / GDAL / MSVC。工作目录没有 Git；原始源码备份在 workspace_data_sources_build/baseline/。

## 任务 1：来源加载

- [x] 新建 WorkspaceDataSourceLoader.h/.cpp，按 provider key、URI、图层类型、编码及变换上下文加载，不以本地文件存在性过滤提供者 URI。
- [x] 编写运行时用例，生成多图层 GPKG、GeoTIFF、带地理参考和无地理参考 PDF，验证选择、取消、CRS、范围与属性。
- [x] 使用 QGIS 提供者子图层查询和原生 QgsSublayersDialog，保留多个候选 URI、打开选项及分组行为。
- [x] 使用 GDAL 检查 PDF 地理参考；未定位 PDF 给出明确错误，损坏来源和无法查询子图层的提供者走可诊断加载路径。
- [x] 运行测试并生成可复用脚本及日志。

## 任务 2：原生管理器和平台集成

- [x] 备份入口源码、UI、工具栏文件及项目过滤器。
- [x] 新建 WorkspaceDataSources.cpp，并在 LTZK.h 中添加管理器、加载、注册及错误处理接口和 QPointer 生命周期管理。
- [x] 加入“数据源管理器”与 Ctrl+L 入口；原矢量、栅格、WMS、WFS、WCS 按钮路由到各自原生页面。
- [x] 接入矢量/栅格、数据库、网格、点云、矢量瓦片、浏览器 URI、文件打开、矢量替换、状态及连接刷新信号。
- [x] 验证后注册图层、设置活动图层并转换范围至画布 CRS；非空间表不触动画布范围。
- [x] 让 PDF 与支持格式的拖放、目录扫描及浏览器来源走同一提供者查询路径。
- [x] 替换前验证新来源并保护未保存编辑；用 setDataSource 更新原图层对象，保留图层 ID、树位置及引用。
- [x] 修复打开工程后画布桥接丢失；Release 完整构建通过。

## 任务 3：回归与交付

- [x] 运行真实 GDAL/QGIS 加载用例，覆盖中文路径 GPKG、GeoTIFF、GeoPDF、多页 PDF、取消、损坏及未定位 PDF。
- [x] 运行平台宿主集成用例，验证原生页面、加载信号、窗口复用、GeoPDF 定位及渲染、图层替换和未保存编辑保护。
- [x] 运行八项相关既有回归检查；记录两项原始备份中同样失败的旧静态断言。
- [x] 完成代码自检，修复刷新递归、画布桥接和图层替换对象重建问题。
- [x] 更新设计状态、使用说明、验证记录和本计划。GeoPDF 使用受控样例，尚未用用户实际文件验证。

## 验证命令

运行加载测试：cmd /c tests\run_workspace_data_sources_runtime_test.cmd
运行平台集成测试：cmd /c tests\run_workspace_data_source_host_test.cmd E:\AAALTZK\workspace_data_sources_build\obj
Release 编译：MSBuild LTZK.vcxproj /p:Configuration=Release /p:Platform=x64，QtMsBuild、IntDir 和 OutDir 参数使用 docs/qgis_data_source_manager.md 中记录的本机路径。

完整平台测试输出保存在 workspace_data_sources_build/host_test/host_test.log；加载测试输出保存在 workspace_data_sources_build/runtime_test.log。
