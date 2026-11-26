# 陇图智控 · 顶部插件功能图标新方案

这是一套**独立的设计稿**。图标名称来自 `E:\LTZK\config\plugin_bindings.json` 中 3 个插件的全部 21 个 `featureName`。其中“成果目录操作”在原配置中 `showInMenu: false`，这里仍提供完整设计。

## 查看与使用

- [浅色总览](preview_light.png) · [深色总览](preview_dark.png)
- `svg/`：64 × 64 可编辑矢量图标，各功能一件。
- `png64/`：64 × 64 透明背景 PNG，适合审阅及展示。
- `png34/`：34 × 34 透明背景 PNG，按当前顶部插件按钮常用图标尺寸预览。
- `manifest.json`：功能名称、插件 ID、显示状态、原图标路径与新文件名的逐项对应。
- `generate.js`：本套设计的可重复生成脚本，需 Node.js 与 `sharp`。

## 视觉语言

深海蓝徽章代表平台控制面板；等高线、地图网格、定位刻度和测绘节点作共同细节。三组功能分别使用**青蓝（成果质检）**、**青绿与金色（成果管理）**、**紫金（智能综合）**。每个图标在徽章中心使用独立图形说明操作，细节在 64 像素下丰富，主体在 34 像素下仍可辨认。

## 功能索引

| # | 功能名称 | SVG |
| --- | --- | --- |
| 01 | 地图数据下载 | [01_map_data_download.svg](svg/01_map_data_download.svg) |
| 02 | 地图数据预处理 | [02_map_data_preprocess.svg](svg/02_map_data_preprocess.svg) |
| 03 | 质检方案配置 | [03_quality_scheme_config.svg](svg/03_quality_scheme_config.svg) |
| 04 | 质检任务管理 | [04_quality_task_manager.svg](svg/04_quality_task_manager.svg) |
| 05 | 图片地图质检 | [05_picture_map_quality.svg](svg/05_picture_map_quality.svg) |
| 06 | 图幅接边检查 | [06_sheet_edge_check.svg](svg/06_sheet_edge_check.svg) |
| 07 | 跨比例尺检查 | [07_cross_scale_check.svg](svg/07_cross_scale_check.svg) |
| 08 | 质检报告输出 | [08_quality_report_export.svg](svg/08_quality_report_export.svg) |
| 09 | 数据库连接 | [09_database_connection.svg](svg/09_database_connection.svg) |
| 10 | 数据导入 | [10_data_import.svg](svg/10_data_import.svg) |
| 11 | 成果目录操作（配置预留） | [11_product_catalog_operation.svg](svg/11_product_catalog_operation.svg) |
| 12 | 元数据管理 | [12_metadata_management.svg](svg/12_metadata_management.svg) |
| 13 | 检索与预览 | [13_search_preview.svg](svg/13_search_preview.svg) |
| 14 | 版本回溯 | [14_version_rollback.svg](svg/14_version_rollback.svg) |
| 15 | 主题管理 | [15_theme_management.svg](svg/15_theme_management.svg) |
| 16 | 地图服务管理 | [16_map_service_management.svg](svg/16_map_service_management.svg) |
| 17 | 参数配置 | [17_parameter_config.svg](svg/17_parameter_config.svg) |
| 18 | 地图综合 | [18_map_generalization.svg](svg/18_map_generalization.svg) |
| 19 | 综合成果质检 | [19_generalized_result_quality.svg](svg/19_generalized_result_quality.svg) |
| 20 | 格式转换 | [20_format_conversion.svg](svg/20_format_conversion.svg) |
| 21 | 数据备份恢复 | [21_data_backup_restore.svg](svg/21_data_backup_restore.svg) |

这套设计稿未登记到 Qt 资源文件，也未修改现有源码、配置或原图标。
