#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
填充地图服务审批单模板并保存为 DOCX
用法: fill_archive.py <output_path> <year> <month> <day> <unit_name> <cause> <service_content> <arranged_office> <remarks>

通过表格第1列标题文本匹配定位第2列并写入内容。
模板表格共 7 行：用图单位 / 事由 / 任务内容 / 安排处室（签字）/ 地信中心（签字）/ 接收人（签字）/ 备注。
"""

import sys
import os
import shutil
import tempfile


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TEMPLATE_PATH = os.path.join(SCRIPT_DIR, 'map_service_template.docx')


def fill_and_save(output_path, year, month_str, day_str, unit_name,
                  cause, service_content, arranged_office, remarks_str):
    """用 python-docx 填充模板并保存"""
    from docx import Document

    # ---- 打开模板 ----
    doc = Document(TEMPLATE_PATH)

    # ---- 写入：编号段落 ----
    num_done = False
    for para in doc.paragraphs:
        if para.text.strip().startswith('编号'):
            para.text = '编号：' + year + '第  号'
            num_done = True
            break
    if not num_done:
        raise RuntimeError('模板中未找到"编号"段落，请检查模板格式')

    # ---- 写入：日期段落 ----
    date_done = False
    for para in doc.paragraphs:
        if para.text.strip().startswith('日期'):
            para.text = '日期：' + year + ' 年 ' + month_str + ' 月 ' + day_str + ' 日'
            date_done = True
            break
    if not date_done:
        raise RuntimeError('模板中未找到"日期"段落，请检查模板格式')

    # ---- 写入：表格内容（根据第1列标题匹配第2列） ----
    # 先去掉所有空白字符再匹配，兼容"备  注""备注"等模板变体
    table_done = 0

    for table in doc.tables:
        for row_idx, row in enumerate(table.rows):
            cells = row.cells
            if len(cells) < 2:
                continue
            header = cells[0].text
            header_compact = header.replace(' ', '').replace('\u3000', '').replace('\t', '').replace('\n', '')
            target_para = cells[1].paragraphs[0] if cells[1].paragraphs else cells[1].add_paragraph()

            if '用图单位' in header_compact:
                target_para.text = unit_name
                table_done += 1
            elif '事由' in header_compact:
                target_para.text = cause
                table_done += 1
            elif '任务内容' in header_compact:
                target_para.text = service_content
                table_done += 1
            elif '安排处室' in header_compact:
                target_para.text = arranged_office
                table_done += 1
            elif '地信中心' in header_compact:
                # 地信中心（签字）：留空供手签
                target_para.text = ''
                table_done += 1
            elif '接收人' in header_compact:
                # 接收人（签字）：留空供手签
                target_para.text = ''
                table_done += 1
            elif '备注' in header_compact:
                target_para.text = remarks_str
                table_done += 1

    if table_done < 3:
        raise RuntimeError('模板表格中未找到足够标题行，请检查模板格式')

    # ---- 保存：先写临时文件，再复制到目标路径（避免 Permission denied） ----
    tmp_fd, tmp_path = tempfile.mkstemp(suffix='.docx')
    os.close(tmp_fd)
    try:
        doc.save(tmp_path)

        # 确保目标目录存在
        out_dir = os.path.dirname(output_path)
        if out_dir:
            os.makedirs(out_dir, exist_ok=True)

        # 如果目标文件存在且被占用，删除失败就换名
        if os.path.exists(output_path):
            try:
                os.remove(output_path)
            except OSError:
                import datetime
                ts = datetime.datetime.now().strftime('%H%M%S')
                base, ext = os.path.splitext(output_path)
                output_path = base + '_' + ts + ext

        shutil.copy2(tmp_path, output_path)
        print('OK:' + output_path)
    finally:
        if os.path.exists(tmp_path):
            os.unlink(tmp_path)

    return output_path


def main():
    if len(sys.argv) < 10:
        print('ERROR: Usage: fill_archive.py <output_path> <year> <month> <day> '
              '<unit_name> <cause> <service_content> <arranged_office> <remarks>', file=sys.stderr)
        sys.exit(1)

    output_path    = sys.argv[1]
    year           = sys.argv[2]
    month          = sys.argv[3].rjust(2)
    day            = sys.argv[4].rjust(2)
    unit_name      = sys.argv[5]
    cause          = sys.argv[6]
    service_content = sys.argv[7]
    arranged_office = sys.argv[8]
    remarks        = sys.argv[9].strip() or '（无）'

    # 检查模板
    if not os.path.exists(TEMPLATE_PATH):
        print('ERROR: 模板文件不存在: ' + TEMPLATE_PATH, file=sys.stderr)
        sys.exit(1)

    try:
        fill_and_save(output_path, year, month, day,
                      unit_name, cause, service_content,
                      arranged_office, remarks)
    except ImportError:
        print('ERROR: python-docx 未安装，请执行: pip install python-docx', file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print('ERROR: ' + str(e), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
