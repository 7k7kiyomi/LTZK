# 原生数据源管理器与 GeoPDF

平台已接入当前 QGIS 3.28.15 的原生 QgsDataSourceManagerDialog。

## 使用

在地图工作区的工程数据工具栏点击“数据源管理器”，或使用 Ctrl+L。原“添加矢量数据”“添加栅格数据”分别打开原生矢量、栅格页面；WMS/WFS/WCS 入口也使用同一个窗口。

- GeoPDF 整页地图：从栅格页面选择 `.pdf` 后添加；存在多页时选择需要的页面。
- GeoPDF 可读取的矢量要素：从矢量页面选择 `.pdf`，选择驱动识别出的子图层。
- GPKG 等多图层来源：通过 QGIS 原生选择窗口多选，支持“添加到组”。
- 直接拖入 PDF：自动列出驱动识别到的栅格/矢量候选，由用户选择加载类型。
- 添加后设置活动图层，将范围转换到画布 CRS 后定位。缺少地理参考的普通 PDF 会给出中文错误，不会编造定位坐标。
- 替换选中矢量图层：先验证新来源，再更新原图层对象；图层 ID、树位置及工程引用继续有效，同几何类型下由 QGIS 保留渲染样式。存在未保存编辑时拒绝替换。

界面提供者分类、编码和打开选项来自本机 QGIS。数据库及网络图层需要相应的连接和访问权限。矢量读取保留驱动能够提取的要素与属性，整页外观通过栅格路径显示。

## 本机预览

双击 `E:\AAALTZK\tools\start_data_sources_preview.cmd`。它设置本机已有 QGIS/OSGeo4W 运行环境，然后打开 `workspace_data_sources_build/app/LTZK.exe`；不覆盖原有程序。

构建输出自带的通用 `app/start_LTZK.bat` 面向包含 QGIS/OSGeo4W 子目录的发布包，本机独立构建应使用上面的预览脚本。

## 实现文件

- `WorkspaceDataSourceLoader.h/.cpp`：提供者及子图层查询、原生选择窗口、图层创建、PDF 原始地理参考检查与错误。
- `WorkspaceDataSources.cpp`：管理器生命周期与信号、图层注册、分组、定位及安全替换。
- `LTZK.cpp/.h/.ui`、`WorkspaceLayerTools.cpp`、`LTZK.qrc`、项目及过滤器：原入口接入、拖放、资源与构建。
- 打开 QGIS 工程时保留图层树画布桥接，确保之后继续添加图层仍可显示。

## 验证

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' LTZK.vcxproj /p:Configuration=Release /p:Platform=x64 '/p:QtMsBuild=C:\Users\孟子腾\AppData\Local\QtMsBuild' '/p:IntDir=E:\AAALTZK\workspace_data_sources_build\obj\' '/p:OutDir=E:\AAALTZK\workspace_data_sources_build\app\' /m:1 /v:minimal /nologo
cmd /c tests\run_workspace_data_sources_runtime_test.cmd
cmd /c tests\run_workspace_data_source_host_test.cmd E:\AAALTZK\workspace_data_sources_build\obj
```

真实 GDAL/QGIS 运行时样例涵盖：中文路径双表 GPKG、多选/单选/取消/分组、OGR URI 与打开选项、memory 非文件 URI、GeoTIFF、带定位 GeoPDF、GeoPDF 矢量子图层、自动栅格/矢量候选、多页 PDF、未定位/损坏 PDF、密码脱敏。

完整平台集成测试涵盖：原生页面及 WMS/WFS/WCS 路由、刷新无递归、原生信号真实加载、活动图层、跨 CRS 范围转换、窗口复用/销毁后重建、GeoPDF 实际地图位置与渲染像素、替换失败保留原图层、成功替换保留原 ID 和对象、保护未保存编辑、打开工程后继续导入。

日志：`workspace_data_sources_build/runtime_test.log`、`workspace_data_sources_build/host_test/host_test.log`。截图与受控样例在 `workspace_data_sources_build/host_test/`，原始源码备份在 `workspace_data_sources_build/baseline/`。

GeoPDF 验证使用自动生成的受控样例；尚未获得用户实际 GeoPDF。真实数据库、网络服务、点云和网格数据的端到端读取未做本次样例验证，其入口按提供者 API 接入。

既有静态检查 `verify_layer_removal_uses_qgis_actions.ps1` 和 `verify_workspace_static_top_actions_survive_plugin_reload.ps1` 在原始备份中也不满足对应断言。它们要求的旧实现/精确文本与当前基线不符，本次未改写这些测试或无关逻辑。

Release 完整构建最初出现既有 `FeatureAttribute.cpp/.h` 的源编码及未使用局部变量警告；新增代码无对应警告。
