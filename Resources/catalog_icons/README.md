# 目录树图标

此文件夹保存数据库数据与本地数据目录树共用的 19 个原创 SVG 图标，另有 `preview.png` 总览。图标源尺寸为 24 × 24，Qt 会在树节点内按现有尺寸缩放。

| 文件 | 用途 |
| --- | --- |
| `database.svg` | 数据库根节点 |
| `results.svg`、`elements.svg`、`materials.svg` | 制图成果、制图要素、制图资料 |
| `ai.svg`、`pdf.svg` | AI 与 PDF 目录或文件 |
| `imagery.svg`、`hillshade.svg`、`feature_set.svg` | 影像、晕渲、要素集 |
| `document.svg`、`spreadsheet.svg`、`other.svg` | 文档资料、表格资料、其它 |
| `point.svg`、`line.svg`、`polygon.svg`、`raster.svg` | 地理图层类型 |
| `table.svg`、`folder.svg`、`file.svg` | 数据表与通用回退 |

SVG 由 `tools/generate_catalog_icons.py` 生成；资源路径登记在根目录的 `LTZK.qrc` 中。
