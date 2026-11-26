#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成地图服务审批单模板 (map_service_template.docx)
"""
import sys, os
from docx import Document
from docx.shared import Pt, Cm, Inches, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn, nsdecls
from docx.oxml import parse_xml

OUTPUT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'map_service_template.docx')

def set_cell_border(cell, **kwargs):
    """设置单元格边框"""
    tc = cell._tc
    tcPr = tc.get_or_add_tcPr()
    tcBorders = parse_xml(
        '<w:tcBorders %s>'
        '  <w:top w:val="{val}" w:sz="4" w:space="0" w:color="000000"/>'
        '  <w:left w:val="{val}" w:sz="4" w:space="0" w:color="000000"/>'
        '  <w:bottom w:val="{val}" w:sz="4" w:space="0" w:color="000000"/>'
        '  <w:right w:val="{val}" w:sz="4" w:space="0" w:color="000000"/>'
        '</w:tcBorders>' % nsdecls('w')
    )
    tcPr.append(tcBorders)

def add_run(paragraph, text, font_name='宋体', size=Pt(12), bold=False):
    """添加格式化文本"""
    run = paragraph.add_run(text)
    run.font.name = font_name
    run._element.rPr.rFonts.set(qn('w:eastAsia'), font_name)
    run.font.size = size
    run.bold = bold
    return run

def main():
    doc = Document()

    # 设置默认字体
    style = doc.styles['Normal']
    font = style.font
    font.name = '宋体'
    font.size = Pt(12)
    style.element.rPr.rFonts.set(qn('w:eastAsia'), '宋体')

    # 设置页边距
    for section in doc.sections:
        section.top_margin = Cm(2.54)
        section.bottom_margin = Cm(2.54)
        section.left_margin = Cm(3.18)
        section.right_margin = Cm(3.18)

    # 标题 - 居中
    title_para = doc.add_paragraph()
    title_para.alignment = WD_ALIGN_PARAGRAPH.CENTER
    add_run(title_para, '地图服务审批单', font_name='黑体', size=Pt(18), bold=True)

    # 空行
    doc.add_paragraph()

    # 编号行：编号：{{year}}第  号
    num_para = doc.add_paragraph()
    add_run(num_para, '编号：', size=Pt(14))
    add_run(num_para, '{{year}}', size=Pt(14))
    add_run(num_para, '第', size=Pt(14))
    add_run(num_para, '  号', size=Pt(14))

    # 日期行
    date_para = doc.add_paragraph()
    add_run(date_para, '日期：', size=Pt(14))
    add_run(date_para, '{{year_num}}', size=Pt(14))
    add_run(date_para, ' 年 ', size=Pt(14))
    add_run(date_para, '{{month}}', size=Pt(14))
    add_run(date_para, ' 月 ', size=Pt(14))
    add_run(date_para, '{{day}}', size=Pt(14))
    add_run(date_para, ' 日', size=Pt(14))

    # 空行
    doc.add_paragraph()

    # 表格
    table = doc.add_table(rows=3, cols=2, style='Table Grid')
    table.alignment = WD_TABLE_ALIGNMENT.CENTER

    # 设置列宽
    for row in table.rows:
        row.cells[0].width = Cm(3)
        row.cells[1].width = Cm(11)

    # 第1行：用图单位
    cell_0_0 = table.cell(0, 0)
    p = cell_0_0.paragraphs[0]
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    add_run(p, '用图单位', size=Pt(12), bold=True)

    cell_0_1 = table.cell(0, 1)
    p = cell_0_1.paragraphs[0]
    add_run(p, '{{unit_name}}', size=Pt(12))

    # 第2行：任务内容
    cell_1_0 = table.cell(1, 0)
    p = cell_1_0.paragraphs[0]
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    add_run(p, '任务内容', size=Pt(12), bold=True)

    cell_1_1 = table.cell(1, 1)
    p = cell_1_1.paragraphs[0]
    add_run(p, '{{service_content}}', size=Pt(12))

    # 第3行：备注
    cell_2_0 = table.cell(2, 0)
    p = cell_2_0.paragraphs[0]
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    add_run(p, '备  注', size=Pt(12), bold=True)

    cell_2_1 = table.cell(2, 1)
    p = cell_2_1.paragraphs[0]
    add_run(p, '{{remarks}}', size=Pt(12))

    # 设置所有行高
    for row in table.rows:
        for cell in row.cells:
            tcPr = cell._tc.get_or_add_tcPr()
            tcHeight = parse_xml(
                '<w:tcHeight %s w:val="800"/>' % nsdecls('w')
            )
            tcPr.append(tcHeight)

    doc.save(OUTPUT_PATH)
    print(f'Template created: {OUTPUT_PATH}')

if __name__ == '__main__':
    main()
