# Workspace attribute tables, map status, and GDB icons

User approved the design on 2026-10-26, selecting one attribute-table window per layer. Different layers may have simultaneous windows.

## Behavior

- Reopening a layer's attribute table restores and activates its existing dialog, preserving edits, selection, and filters. A nonempty filter supplied through the plugin interface updates that dialog's expression filter. Closing or deleting a layer releases the dialog, and reopening works normally.
- Add a compact status row inside the map panel, beneath the canvas. Show mouse X/Y in the canvas destination CRS (six decimal places for geographic coordinates, two for projected coordinates), the CRS identifier, and an editable QGIS scale widget. Zoom updates the widget; choosing a scale zooms the canvas. Unavailable coordinates show placeholders. The existing application longitude/latitude display must transform from the actual canvas CRS.
- GDB containers retain their collection icon. Data leaves, including node_type=1 directory records, use point, line, polygon, raster, or table icons based on metadata. File paths containing .gdb are provenance, not grounds to override a leaf's geometry icon. Unknown leaves use a generic data/file icon. Ordinary ancestor directories do not acquire GDB icons solely because descendants originate in a GDB.

## Implementation boundaries

Use LTZK's guarded ownership patterns for dialog tracking. Add status UI alongside the existing map panel layout and QgsMapCanvas signals. Keep icon decisions in DataCatalogWidget's existing helpers and database-tree builder; do not alter database records or loading behavior.

## Verification

Run a real Qt/QGIS host test for same-layer reuse, separate-layer dialogs, minimize/restore, close/reopen, layer deletion, and plugin filters. Verify map coordinates under EPSG:4326 and EPSG:3857 and scale changes in both directions. Compare rendered icon pixels for GDB point/line/polygon/raster/table leaves and GDB containers, including layer-directory records. Compile the Release application and rerun relevant existing checks.

The workspace has no .git repository, so changes are applied in place with original source backups under the task build directory; no commits are possible.
