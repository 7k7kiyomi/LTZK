from pathlib import Path

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Inches, Pt, RGBColor


ROOT = Path(r"C:\Users\孟子腾\.codex\visualizations\2026\09\11\01a08efb-04af-7540-92c3-38435181b4d7")
OUTPUT = ROOT / "map_product_manager_tools_DLL_platform_call_handoff.docx"


def set_cell_shading(cell, fill):
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = tc_pr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tc_pr.append(shd)
    shd.set(qn("w:fill"), fill)


def set_cell_margins(cell, top=100, start=120, bottom=100, end=120):
    tc = cell._tc
    tc_pr = tc.get_or_add_tcPr()
    tc_mar = tc_pr.first_child_found_in("w:tcMar")
    if tc_mar is None:
        tc_mar = OxmlElement("w:tcMar")
        tc_pr.append(tc_mar)
    for margin, value in (("top", top), ("start", start), ("bottom", bottom), ("end", end)):
        node = tc_mar.find(qn(f"w:{margin}"))
        if node is None:
            node = OxmlElement(f"w:{margin}")
            tc_mar.append(node)
        node.set(qn("w:w"), str(value))
        node.set(qn("w:type"), "dxa")


def set_table_borders(table, color="D9D9D9", size="6"):
    tbl = table._tbl
    tbl_pr = tbl.tblPr
    borders = tbl_pr.first_child_found_in("w:tblBorders")
    if borders is None:
        borders = OxmlElement("w:tblBorders")
        tbl_pr.append(borders)
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        tag = f"w:{edge}"
        element = borders.find(qn(tag))
        if element is None:
            element = OxmlElement(tag)
            borders.append(element)
        element.set(qn("w:val"), "single")
        element.set(qn("w:sz"), size)
        element.set(qn("w:space"), "0")
        element.set(qn("w:color"), color)


def set_repeat_table_header(row):
    tr_pr = row._tr.get_or_add_trPr()
    tbl_header = OxmlElement("w:tblHeader")
    tbl_header.set(qn("w:val"), "true")
    tr_pr.append(tbl_header)


def set_widths(table, widths_cm):
    for row in table.rows:
        for cell, width in zip(row.cells, widths_cm):
            cell.width = Cm(width)


def set_run_font(run, name="Microsoft YaHei", size=10.5, bold=False, color="000000"):
    run.font.name = name
    run._element.get_or_add_rPr().rFonts.set(qn("w:ascii"), name)
    run._element.get_or_add_rPr().rFonts.set(qn("w:hAnsi"), name)
    run._element.get_or_add_rPr().rFonts.set(qn("w:eastAsia"), name)
    run.font.size = Pt(size)
    run.bold = bold
    run.font.color.rgb = RGBColor.from_string(color)


def format_paragraph(paragraph, space_before=0, space_after=6, line=1.18):
    fmt = paragraph.paragraph_format
    fmt.space_before = Pt(space_before)
    fmt.space_after = Pt(space_after)
    fmt.line_spacing = line


def add_body(doc, text, bold_lead=None):
    p = doc.add_paragraph(style="Normal")
    format_paragraph(p, space_after=6, line=1.2)
    if bold_lead and text.startswith(bold_lead):
        r1 = p.add_run(bold_lead)
        set_run_font(r1, bold=True)
        r2 = p.add_run(text[len(bold_lead):])
        set_run_font(r2)
    else:
        r = p.add_run(text)
        set_run_font(r)
    return p


def add_bullet(doc, text, level=0):
    p = doc.add_paragraph(style="List Bullet" if level == 0 else "List Bullet 2")
    format_paragraph(p, space_after=3, line=1.15)
    r = p.add_run(text)
    set_run_font(r)
    return p


def add_code(doc, code):
    for line in code.strip("\n").splitlines():
        p = doc.add_paragraph(style="Normal")
        format_paragraph(p, space_after=0, line=1.0)
        p.paragraph_format.left_indent = Cm(0.35)
        p.paragraph_format.right_indent = Cm(0.35)
        p_pr = p._p.get_or_add_pPr()
        shd = OxmlElement("w:shd")
        shd.set(qn("w:fill"), "F2F4F7")
        p_pr.append(shd)
        r = p.add_run(line)
        set_run_font(r, name="Consolas", size=9, color="1F2937")


def add_table(doc, headers, rows, widths_cm):
    table = doc.add_table(rows=1, cols=len(headers))
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = False
    set_table_borders(table)
    set_widths(table, widths_cm)
    header = table.rows[0]
    set_repeat_table_header(header)
    for cell, text in zip(header.cells, headers):
        set_cell_shading(cell, "1F4E78")
        set_cell_margins(cell, top=120, start=130, bottom=120, end=130)
        cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
        p = cell.paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        format_paragraph(p, space_after=0, line=1.0)
        r = p.add_run(text)
        set_run_font(r, bold=True, color="FFFFFF", size=9.5)
    for index, row_data in enumerate(rows):
        row = table.add_row()
        for cell, text in zip(row.cells, row_data):
            set_cell_margins(cell)
            cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
            if index % 2 == 1:
                set_cell_shading(cell, "F5F8FB")
            p = cell.paragraphs[0]
            format_paragraph(p, space_after=0, line=1.08)
            r = p.add_run(str(text))
            set_run_font(r, size=9.2)
    set_widths(table, widths_cm)
    doc.add_paragraph().paragraph_format.space_after = Pt(1)
    return table


def add_heading(doc, text, level=1):
    p = doc.add_paragraph(style=f"Heading {level}")
    format_paragraph(p, space_before=10 if level == 1 else 6, space_after=5, line=1.0)
    r = p.add_run(text)
    set_run_font(r, name="Microsoft YaHei", size=14 if level == 1 else 11.5, bold=True)
    return p


def configure_styles(doc):
    styles = doc.styles
    normal = styles["Normal"]
    normal.font.name = "Microsoft YaHei"
    normal._element.rPr.rFonts.set(qn("w:ascii"), "Microsoft YaHei")
    normal._element.rPr.rFonts.set(qn("w:hAnsi"), "Microsoft YaHei")
    normal._element.rPr.rFonts.set(qn("w:eastAsia"), "Microsoft YaHei")
    normal.font.size = Pt(10.5)
    for name, size in (("Heading 1", 14), ("Heading 2", 11.5), ("Heading 3", 10.5)):
        style = styles[name]
        style.font.name = "Microsoft YaHei"
        style._element.rPr.rFonts.set(qn("w:ascii"), "Microsoft YaHei")
        style._element.rPr.rFonts.set(qn("w:hAnsi"), "Microsoft YaHei")
        style._element.rPr.rFonts.set(qn("w:eastAsia"), "Microsoft YaHei")
        style.font.size = Pt(size)
        style.font.bold = True
        style.font.color.rgb = RGBColor(0, 0, 0)


def build_document():
    doc = Document()
    configure_styles(doc)
    section = doc.sections[0]
    section.top_margin = Cm(1.8)
    section.bottom_margin = Cm(1.7)
    section.left_margin = Cm(2.0)
    section.right_margin = Cm(2.0)

    title = doc.add_paragraph(style="Title")
    title.alignment = WD_ALIGN_PARAGRAPH.CENTER
    format_paragraph(title, space_after=8, line=1.0)
    title_run = title.add_run("平台调用地图成果管理插件 DLL 开发交接说明")
    set_run_font(title_run, name="Microsoft YaHei", size=20, bold=True)

    subtitle = doc.add_paragraph()
    subtitle.alignment = WD_ALIGN_PARAGRAPH.CENTER
    format_paragraph(subtitle, space_after=16, line=1.0)
    subtitle_run = subtitle.add_run("面向 map_product_manager_tools 开发人员")
    set_run_font(subtitle_run, size=10.5, color="555555")

    add_body(
        doc,
        "本文用于说明平台为了直接调用 map_product_manager_tools.dll 内部的成果目录功能，已经形成的调用契约，以及 DLL 侧需要新增或核对的代码。重点是让平台右键菜单能够稳定打开现有的“按范围导出”和“按主区裁切导出”对话框。平台只负责传递操作类型、目录和成果 ID、当前数据库连接参数；导出业务仍由 DLL 现有成果管理代码执行。",
    )
    add_body(
        doc,
        "本文中的“需要修改”指 DLL 开发人员应在 DLL 工程中实现或确认的适配内容。平台侧最终勾选功能修复只修改了平台代码，未要求重新实现 DLL 内的导出算法。",
        bold_lead="范围说明：",
    )

    add_heading(doc, "一 交接结论")
    add_body(
        doc,
        "DLL 不能只依赖平台通过菜单文字或 QAction 索引查找功能。平台需要一个稳定的 C ABI 导出函数，DLL 收到请求后再转入自身已有的成果管理对话框。DLL 开发人员需要保证导出符号、请求结构、操作字符串、线程调度和权限检查全部与平台约定一致。",
    )
    add_bullet(doc, "增加或保留导出函数 ltzk_product_catalog_open_operation。")
    add_bullet(doc, "使用共享头文件 product_catalog_plugin_api.h，避免跨 DLL 传递 QString、QVariant、QObject 或 DLL 内部类。")
    add_bullet(doc, "将 range 映射到已有的按范围导出对话框，将 main-area 映射到已有的按主区裁切导出对话框。")
    add_bullet(doc, "保留 DLL 原有用户登录和权限校验；数据库连接参数由平台在本次操作中传入。")

    add_heading(doc, "二 原有问题和修改原因")
    add_body(doc, "之前平台通过插件菜单动作查找 DLL 功能，存在以下不稳定情况：")
    add_bullet(doc, "数据库列表的右键动作是动态创建的，不一定出现在插件固定 QAction 列表中。")
    add_bullet(doc, "动作索引可能定位到用户登录或其他菜单动作，导致点击导出时弹出登录窗口。")
    add_bullet(doc, "插件已加载并不等于平台能够找到具体业务动作，因此会出现“插件动作未找到”。")
    add_bullet(doc, "平台无法直接传递当前数据库列表使用的手动连接，DLL 可能继续使用自己的旧连接。")
    add_body(
        doc,
        "因此，本次适配的核心不是复制一份导出代码，而是在 DLL 上增加稳定的请求入口，让平台能够直接调用已有业务实现。",
        bold_lead="修改原因：",
    )

    add_heading(doc, "三 DLL 需要修改或确认的代码位置")
    add_table(
        doc,
        ["代码位置", "需要实现或确认的内容", "目的"],
        [
            ["product_catalog_plugin_api.h", "定义 API 版本、请求结构、返回码和函数指针类型。", "平台和 DLL 使用同一套稳定 ABI。"],
            ["product_catalog_plugin_api.cpp", "实现 extern C 导出函数，校验请求并调度到插件对象。", "让平台通过 QLibrary::resolve 直接调用 DLL。"],
            ["map_product_manager_tools.h", "增加成果目录操作入口和请求执行函数声明；保存隐藏桥接 QAction。", "兼容平台配置动作和已有插件菜单体系。"],
            ["map_product_manager_tools.cpp", "创建隐藏的“成果目录操作”动作，读取平台请求并执行权限、连接和业务分发。", "把平台请求转入 DLL 现有业务窗口。"],
            ["product_storage_dialog.cpp", "确认 executeExternalDirectoryOperation 能处理各个 operation。", "复用已有导出、裁切、主题和地图逻辑。"],
            ["工程文件或 qmake 配置", "把新增 API 源文件加入编译，并确保导出符号进入 DLL。", "避免源码存在但 DLL 中没有导出函数。"],
        ],
        [4.0, 9.0, 4.0],
    )

    add_heading(doc, "四 稳定 DLL 接口定义")
    add_body(doc, "平台使用以下头文件约定。DLL 工程编译时定义 LTZK_PRODUCT_CATALOG_PLUGIN_BUILD，使函数导出；平台工程只包含同一头文件，不链接 DLL import library。")
    add_code(
        doc,
        r'''#define LTZK_PRODUCT_CATALOG_API_VERSION 1u

typedef struct LTZKProductCatalogOperationRequest
{
    uint32_t apiVersion;
    const char* operationUtf8;
    int32_t directoryId;
    int32_t productId;
    const int32_t* productIds;
    uint32_t productIdCount;
    const char* connectionJsonUtf8;
} LTZKProductCatalogOperationRequest;

extern "C" Q_DECL_EXPORT int ltzk_product_catalog_open_operation(
    const LTZKProductCatalogOperationRequest* request);''',
    )
    add_body(doc, "DLL 实现必须校验 apiVersion、operationUtf8、目录 ID、成果 ID、成果 ID 数组和连接 JSON。无效请求不能继续调用业务窗口，应返回明确的错误码。")
    add_table(
        doc,
        ["返回码", "含义", "平台处理"],
        [
            ["0", "已接受并执行", "清理平台暂存请求。"],
            ["1", "已排队执行", "清理平台暂存请求，等待 DLL 的 Qt 事件循环处理。"],
            ["-1", "请求参数错误", "提示成果目录操作失败。"],
            ["-2", "插件实例未初始化", "提示 DLL 当前不可用。"],
            ["-3", "不支持的操作", "提示 DLL 版本不支持该功能。"],
            ["-4", "Qt 调度失败", "提示 DLL 内部调度失败。"],
        ],
        [2.0, 8.0, 7.0],
    )

    add_heading(doc, "五 请求参数和操作映射")
    add_table(
        doc,
        ["operation", "业务功能", "DLL 内部处理"],
        [
            ["export", "导出所选数据", "调用 ProductStorageDialog 的已有导出逻辑。"],
            ["range", "按范围导出", "调用 onExportByRange，弹出原有按范围导出对话框。"],
            ["main-area", "按主区裁切导出", "调用 onExportByMainAreaClip，弹出原有按主区裁切对话框。"],
            ["import", "导入数据", "调用已有 DataImport 或 DataImportWizard 入口。"],
            ["map", "添加到地图", "调用已有目录选择添加到地图逻辑。"],
            ["theme", "添加到主题", "调用已有主题挂接逻辑。"],
            ["create-directory", "新建目录", "调用已有目录创建逻辑。"],
            ["delete-directory", "删除目录", "调用已有目录删除逻辑。"],
            ["rename-directory", "重命名目录", "调用已有目录重命名逻辑。"],
        ],
        [4.0, 5.2, 7.8],
    )
    add_body(doc, "directoryId 表示当前目录节点，productId 表示当前右键节点对应的单个成果，productIds 表示当前操作涉及的成果集合。按范围导出和按主区裁切导出必须使用 productIds，不能只读取单个 productId。")

    add_heading(doc, "六 DLL 导出函数的推荐实现流程")
    add_body(doc, "导出函数收到请求后，应按以下顺序处理：")
    add_table(
        doc,
        ["步骤", "处理内容", "注意事项"],
        [
            ["1", "检查插件当前实例是否存在。", "实例不存在返回 NOT_INITIALIZED。"],
            ["2", "校验 API 版本和操作名称。", "不支持的操作返回 UNSUPPORTED_OPERATION。"],
            ["3", "把 C ABI 请求转换为 QVariantMap 或插件内部请求对象。", "转换阶段不要把内部 Qt 对象指针暴露给平台。"],
            ["4", "如果请求来自其他线程，通过 Qt 元对象排队到插件线程。", "不要把 DLL 内 lambda 或临时对象直接留在事件队列中。"],
            ["5", "执行用户登录和权限检查。", "未登录或无权限时使用 DLL 原有提示。"],
            ["6", "同步平台传入的数据库连接。", "只同步当前操作需要的数据库连接，不改变平台登录状态。"],
            ["7", "创建或调用 ProductStorageDialog。", "复用现有对话框和业务函数。"],
            ["8", "返回 ACCEPTED 或 QUEUED。", "返回后平台会清理暂存请求。"],
        ],
        [1.3, 8.4, 7.3],
    )

    add_heading(doc, "七 线程和对象生命周期要求")
    add_body(doc, "DLL 导出函数可能由平台主线程调用，也可能由插件管理器在其他线程发起。建议实现为：当前线程就是插件线程时直接执行；否则使用 QMetaObject::invokeMethod 对插件对象执行 QueuedConnection。")
    add_code(
        doc,
        r'''if (QThread::currentThread() == plugin->thread())
{
    plugin->executeProductCatalogRequest(requestMap);
    return LTZK_PRODUCT_CATALOG_ACCEPTED;
}

if (!QMetaObject::invokeMethod(
        plugin,
        "executeProductCatalogRequest",
        Qt::QueuedConnection,
        Q_ARG(QVariantMap, requestMap)))
{
    return LTZK_PRODUCT_CATALOG_DISPATCH_FAILED;
}
return LTZK_PRODUCT_CATALOG_QUEUED;''',
    )
    add_body(doc, "不要把 DLL 内部 lambda、临时 QString、QVariant 引用或 QWidget 指针直接作为跨线程事件参数。插件卸载时，排队调用必须能安全结束，避免 DLL 已卸载后仍执行回调。")

    add_heading(doc, "八 数据库连接和用户认证边界")
    add_body(doc, "平台数据库列表使用用户手动输入的数据库连接。用户选择记住连接信息时，只用于下次自动填充，不自动连接。平台调用 DLL 时，会把当前已经打开的数据库连接参数放入 connectionJsonUtf8。")
    add_table(
        doc,
        ["内容", "处理方", "DLL 开发要求"],
        [
            ["用户登录", "平台和 DLL 的既有登录会话", "继续保留 gCurrentUserSession 登录和权限检查，不要把数据库密码当作用户登录凭据。"],
            ["数据库数据连接", "平台数据列表手动连接", "从 connectionJsonUtf8 同步 host、port、database、schema、user、password。"],
            ["记住连接信息", "平台连接对话框", "只影响下次填充，不应触发 DLL 自动登录或平台自动连接。"],
            ["操作权限", "DLL 业务入口", "按原有 DataOperator 或 DatabaseAdmin 权限判断。"],
        ],
        [4.0, 5.0, 8.0],
    )
    add_body(doc, "connectionJsonUtf8 可能包含数据库密码，DLL 不得把完整 JSON 写入普通日志，也不能在错误提示中回显密码。")

    add_heading(doc, "九 隐藏 QAction 和配置兼容")
    add_body(doc, "平台配置中保留一个不显示在用户菜单中的“成果目录操作”绑定，作为旧版本兼容桥接。DLL 侧应创建一个稳定的隐藏 QAction，并设置固定 objectName，例如 mActionDataCatalogOperation。该动作只负责触发 DataCatalogOperation，不负责重新实现导出功能。")
    add_code(
        doc,
        r'''mActionDataCatalogOperation = new QAction(
    QStringLiteral("成果目录操作"), this);
mActionDataCatalogOperation->setObjectName(
    QStringLiteral("mActionDataCatalogOperation"));
mActionDataCatalogOperation->setVisible(false);
connect(mActionDataCatalogOperation, &QAction::triggered,
    this, &CMapProductManagerPlugin::DataCatalogOperation);''',
    )
    add_body(doc, "新版本平台优先调用 DLL 导出 API。只有在 DLL 尚未提供该导出函数时，平台才尝试 QAction 回退。因此隐藏动作不能替代导出 API，而是用于兼容旧 DLL 或调试。")

    add_heading(doc, "十 工程和部署检查")
    add_table(
        doc,
        ["检查项", "验收标准"],
        [
            ["源文件加入工程", "product_catalog_plugin_api.cpp 和对应头文件参与 Release x64 编译。"],
            ["导出符号", "使用 dumpbin /exports 或等效工具能够看到 ltzk_product_catalog_open_operation。"],
            ["DLL 加载", "平台日志显示 DLL 已加载，且没有“插件动作未找到”。"],
            ["按范围导出", "平台右键操作能打开已有按范围导出对话框。"],
            ["按主区裁切导出", "平台右键操作能打开已有按主区裁切对话框。"],
            ["参数传递", "目录 ID、单个成果 ID、成果 ID 集合和数据库连接均能在 DLL 端收到。"],
            ["认证提示", "已登录且有权限时不重复弹出登录框；未登录时仍按 DLL 原有逻辑提示。"],
            ["线程安全", "跨线程请求返回 QUEUED，插件卸载时无悬挂调用。"],
            ["部署文件", "DLL、平台程序和 plugin_bindings.json 使用同一版本的接口约定。"],
        ],
        [5.0, 12.0],
    )

    add_heading(doc, "十一 平台侧已完成的配套内容")
    add_body(doc, "DLL 开发人员可以按以下平台代码确认接口使用方式：")
    add_table(
        doc,
        ["平台文件", "已完成内容"],
        [
            ["product_catalog_plugin_api.h", "定义跨模块 C ABI 请求结构和返回码。"],
            ["LTZKPluginManager.h 和 LTZKPluginManager.cpp", "加载 DLL 后解析 ltzk_product_catalog_open_operation 并发起调用。"],
            ["LTZK.cpp", "把平台右键操作转换成 operation、目录 ID、成果 ID、成果 ID 集合和连接 JSON。"],
            ["DataCatalogWidget.cpp", "从数据库目录节点收集成果 ID，分发 range、main-area 等操作。"],
            ["config/plugin_bindings.json", "增加隐藏的成果目录操作绑定，用于 API 调用和旧版本回退。"],
            ["DataCatalogWidget.cpp", "修复勾选和取消勾选本节点全部成果时的界面状态与 ID 缓存同步。"],
        ],
        [6.0, 11.0],
    )

    add_heading(doc, "十二 交付给 DLL 开发人员的最小任务清单")
    add_bullet(doc, "使用 product_catalog_plugin_api.h 编译 DLL，并导出 ltzk_product_catalog_open_operation。")
    add_bullet(doc, "确认 export、range、main-area、import、map、theme 及目录操作字符串均能被识别。")
    add_bullet(doc, "确认 range 和 main-area 分别进入现有按范围导出、按主区裁切导出对话框。")
    add_bullet(doc, "确认请求中的 productIds 用于批量导出，不只读取 productId。")
    add_bullet(doc, "确认 DLL 端先执行用户登录和权限检查，再同步平台数据库连接。")
    add_bullet(doc, "确认导出函数支持 Qt 主线程直接执行和非主线程排队执行。")
    add_bullet(doc, "重新编译并部署 DLL 后，用平台 Release x64 程序进行实际点击验证。")

    add_heading(doc, "十三 验证记录")
    add_body(doc, "平台侧当前已完成以下验证：")
    add_bullet(doc, "数据目录勾选操作契约检查通过。")
    add_bullet(doc, "数据库成果目录结构检查通过。")
    add_bullet(doc, "平台 Release x64 隔离输出编译成功，产物为 package_build\\out\\LTZK.exe。")
    add_body(doc, "DLL 开发人员完成上述导出接口后，应重新编译 DLL，并验证平台日志、导出对话框、用户权限和数据库连接四部分行为。")

    footer = section.footer.paragraphs[0]
    footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
    format_paragraph(footer, space_after=0, line=1.0)
    footer_run = footer.add_run("平台调用地图成果管理插件 DLL 开发交接说明")
    set_run_font(footer_run, size=8.5, color="777777")

    doc.core_properties.title = "平台调用地图成果管理插件 DLL 开发交接说明"
    doc.core_properties.subject = "说明 DLL 侧为支持平台直接调用成果目录功能需要实现的接口和代码适配"
    doc.core_properties.author = "LTZK 平台开发"
    doc.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    build_document()
