const ExcelJS = require('exceljs');

const wb = new ExcelJS.Workbook();
wb.creator = 'MapProductManager';
wb.created = new Date();

const headerFill = { type: 'pattern', pattern: 'solid', fgColor: { argb: 'FF4472C4' } };
const headerFont = { name: 'Arial', bold: true, size: 11, color: { argb: 'FFFFFFFF' } };
const titleFont = { name: 'Arial', bold: true, size: 13, color: { argb: 'FF1F4E79' } };
const cellFont = { name: 'Arial', size: 10 };
const thinBorder = {
    top: { style: 'thin' }, left: { style: 'thin' },
    bottom: { style: 'thin' }, right: { style: 'thin' }
};

const headers = ['序号', '名称', '列名', '类型', '说明', '约束条件'];
const colWidths = [6, 22, 24, 28, 50, 36];

function addSheet(wb, sheetName, tableDesc, columns) {
    const ws = wb.addWorksheet(sheetName, {
        properties: { tabColor: { argb: 'FF4472C4' } }
    });

    // Title row
    ws.mergeCells('A1:F1');
    const titleCell = ws.getCell('A1');
    titleCell.value = `表名: ${sheetName}  —  ${tableDesc}`;
    titleCell.font = titleFont;
    titleCell.alignment = { horizontal: 'center', vertical: 'middle', wrapText: true };
    ws.getRow(1).height = 30;

    // Header row (row 3)
    const headerRow = ws.getRow(3);
    headers.forEach((h, i) => {
        const cell = headerRow.getCell(i + 1);
        cell.value = h;
        cell.font = headerFont;
        cell.fill = headerFill;
        cell.alignment = { horizontal: 'center', vertical: 'middle', wrapText: true };
        cell.border = thinBorder;
    });
    headerRow.height = 24;

    // Data rows
    columns.forEach((colData, ri) => {
        const row = ws.getRow(ri + 4);
        colData.forEach((val, ci) => {
            const cell = row.getCell(ci + 1);
            cell.value = val;
            cell.font = cellFont;
            cell.border = thinBorder;
            cell.alignment = ci === 0
                ? { horizontal: 'center', vertical: 'middle', wrapText: true }
                : { horizontal: 'left', vertical: 'middle', wrapText: true };
        });
        row.height = 22;
    });

    // Column widths
    colWidths.forEach((w, i) => {
        ws.getColumn(i + 1).width = w;
    });

    // Freeze pane
    ws.views = [{ state: 'frozen', ySplit: 3 }];
    // Auto filter
    ws.autoFilter = { from: 'A3', to: `F${3 + columns.length}` };
}

// ======================== 1. product_metadata ========================
addSheet(wb, 'product_metadata', '成果元数据主表', [
    [1, '主键ID', 'id', 'SERIAL', '自增主键，唯一标识每条记录', 'PRIMARY KEY'],
    [2, '唯一标识', 'uuid', 'UUID', 'UUID全局唯一标识，用于外部引用', 'NOT NULL, DEFAULT uuid_generate_v4(), UNIQUE'],
    [3, '产品名称', 'product_name', 'VARCHAR(500)', '地图成果产品名称', 'NOT NULL'],
    [4, '产品类型', 'product_type', 'VARCHAR(100)', '产品类型：矢量/栅格/文档/图层等', 'NOT NULL'],
    [5, '文件格式', 'file_format', 'VARCHAR(100)', '文件格式：shp/tif/geojson/docx等', ''],
    [6, '文件路径', 'file_path', 'TEXT', '成果文件在系统中的存储路径', ''],
    [7, '文件大小', 'file_size', 'BIGINT', '文件大小（字节）', 'DEFAULT 0'],
    [8, '文件哈希', 'file_hash', 'VARCHAR(128)', '文件内容哈希值（SHA256），用于校验文件完整性', ''],
    [9, '文件大对象OID', 'file_oid', 'OID', 'PostgreSQL大对象OID，用于BLOB存储', ''],
    [10, '缩略图路径', 'thumbnail_path', 'TEXT', '成果预览缩略图的存储路径', ''],
    [11, '空间几何', 'geom', 'GEOMETRY(GEOMETRY, 4490)', 'PostGIS空间几何对象', 'SRID=4490'],
    [12, '最小经度', 'min_x', 'DOUBLE PRECISION', '空间范围边界框最小X坐标（经度）', ''],
    [13, '最小纬度', 'min_y', 'DOUBLE PRECISION', '空间范围边界框最小Y坐标（纬度）', ''],
    [14, '最大经度', 'max_x', 'DOUBLE PRECISION', '空间范围边界框最大X坐标（经度）', ''],
    [15, '最大纬度', 'max_y', 'DOUBLE PRECISION', '空间范围边界框最大Y坐标（纬度）', ''],
    [16, '坐标参考系', 'crs', 'VARCHAR(200)', '坐标参考系标识（如EPSG:4490）', ''],
    [17, '空间范围WKT', 'spatial_extent_wkt', 'TEXT', 'WKT格式的空间范围描述', ''],
    [18, '几何类型', 'geometry_type', 'TEXT', '几何数据类型：Point/LineString/Polygon等', ''],
    [19, '波段数', 'band_count', 'INTEGER', '栅格数据的波段数量（仅栅格数据有效）', ''],
    [20, '像素宽度', 'pixel_width', 'INTEGER', '栅格数据的像素宽度（仅栅格数据有效）', ''],
    [21, '像素高度', 'pixel_height', 'INTEGER', '栅格数据的像素高度（仅栅格数据有效）', ''],
    [22, '像素分辨率', 'pixel_resolution', 'DOUBLE PRECISION', '栅格数据的像素分辨率（仅栅格数据有效）', ''],
    [23, '比例尺', 'scale', 'TEXT', '地图比例尺（如1:10000）', ''],
    [24, '编制信息', 'compilation_info', 'TEXT', '编制单位/编制说明等补充信息', ''],
    [25, '审图号', 'approval_number', 'VARCHAR(200)', '地图审图号', ''],
    [26, '安全密级', 'security_level', 'VARCHAR(50)', '数据安全密级', "DEFAULT '非密'"],
    [27, '生产单位', 'producer', 'VARCHAR(500)', '数据生产单位名称', ''],
    [28, '生产日期', 'production_date', 'VARCHAR(50)', '数据生产日期', ''],
    [29, '描述信息', 'description', 'TEXT', '成果描述、备注等补充信息', ''],
    [30, '父目录ID', 'parent_dir_id', 'INTEGER', '所属多级目录的父目录ID，0表示根目录', 'DEFAULT 0'],
    [31, '目录路径', 'directory_path', 'TEXT', '完整目录路径（如/根目录/子目录）', ''],
    [32, '当前版本号', 'current_version', 'INTEGER', '成果当前版本号', 'DEFAULT 1'],
    [33, '版本说明', 'version_note', 'TEXT', '当前版本的变更说明', ''],
    [34, '创建人', 'created_by', 'VARCHAR(200)', '成果入库操作者', ''],
    [35, '创建时间', 'created_at', 'TIMESTAMP', '成果入库时间', 'DEFAULT CURRENT_TIMESTAMP'],
    [36, '更新人', 'updated_by', 'VARCHAR(200)', '最后更新操作者', ''],
    [37, '更新时间', 'updated_at', 'TIMESTAMP', '最后更新时间', 'DEFAULT CURRENT_TIMESTAMP'],
    [38, '图层表名', 'layer_table_name', 'VARCHAR(200)', '关联的数据图层PostgreSQL表名（迁移添加）', 'ALTER TABLE迁移添加'],
]);

// ======================== 2. version_records ========================
addSheet(wb, 'version_records', '版本记录表', [
    [1, '主键ID', 'id', 'SERIAL', '自增主键', 'PRIMARY KEY'],
    [2, '产品ID', 'product_id', 'INTEGER', '关联的产品元数据ID', 'NOT NULL, FK→product_metadata(id) ON DELETE CASCADE'],
    [3, '版本号', 'version_number', 'INTEGER', '版本序号（从1开始递增）', 'NOT NULL, UNIQUE(product_id,version_number)'],
    [4, '文件路径', 'file_path', 'TEXT', '该版本文件的存储路径', ''],
    [5, '文件哈希', 'file_hash', 'VARCHAR(128)', '该版本文件的哈希值', ''],
    [6, '文件大小', 'file_size', 'BIGINT', '该版本文件大小（字节）', 'DEFAULT 0'],
    [7, '文件OID', 'file_oid', 'OID', '该版本文件的PostgreSQL大对象OID', ''],
    [8, '文件格式', 'file_format', 'VARCHAR(50)', '该版本文件格式（迁移添加）', 'ALTER TABLE迁移添加'],
    [9, '图层表名', 'layer_table_name', 'VARCHAR(200)', '该版本关联的图层表名（迁移添加）', 'ALTER TABLE迁移添加'],
    [10, '变更说明', 'change_note', 'TEXT', '本次版本变更的说明', ''],
    [11, '变更人', 'changed_by', 'VARCHAR(200)', '变更操作者姓名', ''],
    [12, '变更时间', 'changed_at', 'TIMESTAMP', '变更操作时间', 'DEFAULT CURRENT_TIMESTAMP'],
    [13, '父版本号', 'parent_version', 'INTEGER', '该版本的父版本号（版本链追溯）', 'DEFAULT 0'],
    [14, '差异信息', 'diff_info', 'TEXT', '与前一个版本的差异比对信息（JSON格式）', ''],
]);

// ======================== 3. product_directory ========================
addSheet(wb, 'product_directory', '多级目录表', [
    [1, '主键ID', 'id', 'SERIAL', '自增主键', 'PRIMARY KEY'],
    [2, '父目录ID', 'parent_id', 'INTEGER', '父目录ID，0表示根目录', 'DEFAULT 0'],
    [3, '目录名称', 'name', 'VARCHAR(300)', '目录显示名称', 'NOT NULL'],
    [4, '目录描述', 'description', 'TEXT', '目录功能描述', ''],
    [5, '排序序号', 'sort_order', 'INTEGER', '同级目录排序顺序', 'DEFAULT 0'],
    [6, '创建人', 'created_by', 'VARCHAR(200)', '目录创建者', ''],
    [7, '创建时间', 'created_at', 'TIMESTAMP', '目录创建时间', 'DEFAULT CURRENT_TIMESTAMP'],
]);

// ======================== 4. product_tags ========================
addSheet(wb, 'product_tags', '标签表', [
    [1, '主键ID', 'id', 'SERIAL', '自增主键', 'PRIMARY KEY'],
    [2, '标签名称', 'name', 'VARCHAR(200)', '标签名称', 'NOT NULL, UNIQUE'],
    [3, '标签颜色', 'color', 'VARCHAR(20)', '标签显示颜色（如#FF0000）', ''],
    [4, '标签描述', 'description', 'TEXT', '标签功能描述', ''],
    [5, '创建时间', 'created_at', 'TIMESTAMP', '标签创建时间', 'DEFAULT CURRENT_TIMESTAMP'],
]);

// ======================== 5. product_tag_mapping ========================
addSheet(wb, 'product_tag_mapping', '产品-标签关联表（联合主键：product_id+tag_id）', [
    [1, '产品ID', 'product_id', 'INTEGER', '关联的产品元数据ID', 'NOT NULL, FK→product_metadata(id) ON DELETE CASCADE'],
    [2, '标签ID', 'tag_id', 'INTEGER', '关联的标签ID', 'NOT NULL, FK→product_tags(id) ON DELETE CASCADE'],
]);

// ======================== 6. user_permissions ========================
addSheet(wb, 'user_permissions', '用户权限表', [
    [1, '主键ID', 'id', 'SERIAL', '自增主键', 'PRIMARY KEY'],
    [2, '用户名', 'user_name', 'VARCHAR(200)', '登录用户名', 'NOT NULL, UNIQUE'],
    [3, '角色', 'role', 'VARCHAR(50)', '用户角色：数据入库员/数据审核员/数据库管理员', "NOT NULL, DEFAULT '数据入库员'"],
    [4, '密码哈希', 'password_hash', 'VARCHAR(128)', '用户密码的哈希值（SHA256）', "NOT NULL, DEFAULT ''"],
    [5, '权限列表', 'permissions', 'TEXT', '扩展权限列表（JSON格式）', ''],
    [6, '授权人', 'granted_by', 'VARCHAR(200)', '权限授予者', ''],
    [7, '授权时间', 'granted_at', 'TIMESTAMP', '权限授予时间', 'DEFAULT CURRENT_TIMESTAMP'],
]);

// ======================== 7. audit_log ========================
addSheet(wb, 'audit_log', '审计日志表', [
    [1, '主键ID', 'id', 'SERIAL', '自增主键', 'PRIMARY KEY'],
    [2, '产品ID', 'product_id', 'INTEGER', '关联的产品元数据ID（可为空）', 'FK→product_metadata(id) ON DELETE SET NULL'],
    [3, '操作动作', 'action', 'VARCHAR(100)', '操作类型：上传/更新/删除/下载/回退等', 'NOT NULL'],
    [4, '操作详情', 'detail', 'TEXT', '操作的详细描述信息', ''],
    [5, '操作人', 'operator_name', 'VARCHAR(200)', '执行操作的用户名', ''],
    [6, '操作IP', 'operator_ip', 'VARCHAR(50)', '操作来源IP地址', ''],
    [7, '操作时间', 'created_at', 'TIMESTAMP', '操作发生时间', 'DEFAULT CURRENT_TIMESTAMP'],
]);

// ======================== 8. layer_registry ========================
addSheet(wb, 'layer_registry', '图层注册表', [
    [1, '主键ID', 'id', 'SERIAL', '自增主键', 'PRIMARY KEY'],
    [2, '产品ID', 'product_id', 'INTEGER', '关联的产品元数据ID', 'NOT NULL, FK→product_metadata(id) ON DELETE CASCADE'],
    [3, '表名', 'table_name', 'VARCHAR(200)', 'PostgreSQL数据图层表名', 'NOT NULL, UNIQUE'],
    [4, '几何类型', 'geometry_type', 'VARCHAR(100)', '图层几何类型：POINT/LINESTRING/POLYGON等', ''],
    [5, '几何SRID', 'geometry_srid', 'INTEGER', '空间参考系SRID', 'DEFAULT 4490'],
    [6, '要素数量', 'feature_count', 'INTEGER', '图层中的要素数量', 'DEFAULT 0'],
    [7, '创建时间', 'created_at', 'TIMESTAMP', '图层注册时间', 'DEFAULT CURRENT_TIMESTAMP'],
]);

// ======================== 9. service_units ========================
addSheet(wb, 'service_units', '服务单位表', [
    [1, '主键ID', 'id', 'SERIAL', '自增主键', 'PRIMARY KEY'],
    [2, '单位名称', 'name', 'VARCHAR(255)', '服务单位名称', 'NOT NULL, UNIQUE'],
    [3, '创建时间', 'created_at', 'TIMESTAMP', '记录创建时间', 'DEFAULT CURRENT_TIMESTAMP'],
]);

// ======================== 10. service_records ========================
addSheet(wb, 'service_records', '服务记录表', [
    [1, '主键ID', 'id', 'SERIAL', '自增主键', 'PRIMARY KEY'],
    [2, '服务单号', 'service_number', 'VARCHAR(64)', '服务记录单号', 'NOT NULL'],
    [3, '日期', 'record_date', 'VARCHAR(32)', '服务日期（yyyy-MM-dd）', ''],
    [4, '应用场景', 'application_scenario', 'VARCHAR(64)', '应用场景分类', ''],
    [5, '服务内容', 'service_content', 'TEXT', '服务内容描述（支持多行）', ''],
    [6, '覆盖区域', 'coverage_area', 'VARCHAR(512)', '服务覆盖的地理区域', ''],
    [7, '开本', 'format', 'VARCHAR(64)', '地图开本规格', ''],
    [8, '图幅数', 'sheet_count', 'INTEGER', '地图图幅数量', 'DEFAULT 0'],
    [9, '编制图幅数', 'compiled_sheet_count', 'INTEGER', '编制图幅数量', 'DEFAULT 0'],
    [10, '装裱数', 'mounted_count', 'INTEGER', '装裱数量', 'DEFAULT 0'],
    [11, '政务图包数', 'gov_map_pack_count', 'INTEGER', '政务地图包数量', 'DEFAULT 0'],
    [12, '甘肃省工作用图', 'gansu_work_map', 'INTEGER', '甘肃省工作用图数量', 'DEFAULT 0'],
    [13, '图集图册', 'atlas', 'INTEGER', '图集图册数量', 'DEFAULT 0'],
    [14, '台历/文具地图', 'calendar_stationery_map', 'INTEGER', '台历/文具地图数量', 'DEFAULT 0'],
    [15, '数据', 'data_count', 'INTEGER', '数据数量', 'DEFAULT 0'],
    [16, '服务单位ID', 'unit_id', 'INTEGER', '服务单位ID', 'DEFAULT 0'],
    [17, '服务单位名称', 'unit_name', 'VARCHAR(255)', '服务单位名称', ''],
    [18, '服务类别', 'category', 'VARCHAR(128)', '服务类别编号（如110：省委秘书一处）', ''],
    [19, '服务形式', 'service_form', 'VARCHAR(256)', '服务形式（多选，分号分隔）', ''],
    [20, '图集图册名称', 'atlas_name', 'VARCHAR(256)', '图集图册具体名称', ''],
    [21, '服务等级', 'service_target', 'VARCHAR(256)', '服务等级：国家级/省级/厅级/其他政府单位/事业单位', ''],
    [22, '安排领导', 'arranged_leader', 'VARCHAR(128)', '安排服务的领导姓名', ''],
    [23, '经费', 'funding', 'NUMERIC(12,2)', '服务经费（元）', 'DEFAULT 0'],
    [24, '图件种类', 'map_kind', 'VARCHAR(64)', '图件种类：图集图册/单张图', ''],
    [25, '接收时间', 'receive_time', 'VARCHAR(32)', '服务接收时间', ''],
    [26, '提交时间', 'submit_time', 'VARCHAR(32)', '服务提交时间', ''],
    [27, '接收人', 'receiver', 'VARCHAR(256)', '接收人姓名及电话', ''],
    [28, '状态', 'status', 'INTEGER', '服务状态：0-处理中 1-已完成 2-已取消 3-已归档', 'DEFAULT 0'],
    [29, '备注', 'remarks', 'TEXT', '备注信息', ''],
    [30, '归档路径', 'archive_path', 'VARCHAR(1024)', '电子版归档存储路径', ''],
    [31, '归档格式', 'archive_format', 'VARCHAR(64)', '归档文件格式', ''],
    [32, '关联产品ID列表', 'product_ids', 'TEXT', '关联的地图成果产品ID列表（JSON格式）', ''],
    [33, '关联产品名称列表', 'product_names', 'TEXT', '关联的地图成果产品名称列表（JSON格式）', ''],
    [34, '审批文件路径', 'approval_file_path', 'VARCHAR(1024)', '审批文件存储路径', ''],
    [35, '创建人', 'created_by', 'VARCHAR(128)', '记录创建人', "DEFAULT 'system'"],
    [36, '创建时间', 'created_at', 'TIMESTAMP', '记录创建时间', 'DEFAULT CURRENT_TIMESTAMP'],
    [37, '更新人', 'updated_by', 'VARCHAR(128)', '记录最后更新人', ''],
    [38, '更新时间', 'updated_at', 'TIMESTAMP', '记录最后更新时间', ''],
]);

const outputPath = 'd:\\GarMap\\qgis_plugins\\cplusplus\\map_product_manager_tools\\地图成果仓数据库表结构.xlsx';
wb.xlsx.writeFile(outputPath)
    .then(() => console.log('Excel saved to:', outputPath))
    .catch(err => console.error('Error:', err));
