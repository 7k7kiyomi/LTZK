# Catalog tree icon design

## Goal

Give database and local catalog tree nodes distinct, recognizable icons for their meaning while retaining the current row size and selection behavior.

## Visual system

Original 24 × 24 SVG icons live together in `Resources/catalog_icons/`. Shapes use a consistent 1.6 px stroke, rounded corners, and a restrained blue, teal, amber, orange, and red palette. At tree size, each icon has one dominant silhouette. The AI badge is an orange `Ai` software cue; the PDF badge is a red document cue. No third-party logo files are copied.

## Mapping

- Database root: database cylinder.
- Top directories: distinct map sheet (制图成果), geometry nodes (制图要素), and reference stack (制图资料).
- Fixed child directories: AI, PDF, 影像, 晕渲, 要素集, 文档资料, 表格资料, and 其它 each have a semantic icon. The two 其它 nodes share a neutral category icon.
- Database product files: prefer format (`ai`, `pdf`, documents, spreadsheets), then raster/vector geometry, then a general file.
- Local directories: match the known directory names above; other directories use a custom neutral folder. Local layer items: prefer supported file suffixes, then raster/vector geometry. Other browser nodes keep their QGIS icon.
- Existing point/line/polygon/raster layer meaning remains visible through dedicated icons in this set.

## Integration

Register all SVGs in `LTZK.qrc` and resolve them via Qt resource URLs. Keep icon selection in a small catalog icon helper to avoid duplicating mappings between the database model and local browser proxy. Fall back to a neutral icon for unknown categories. No database schema, tree structure, expansion, or checkbox behavior changes.

## Verification

Check SVG XML and Qt resource references, compile the app, and inspect the resulting tree at its normal row size if a runnable authenticated session is available.
