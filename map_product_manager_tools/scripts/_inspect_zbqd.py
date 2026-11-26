# -*- coding: utf-8 -*-
"""临时脚本：解析 zbqd.xlsx 模板的 sheet 结构（仅用于开发调试，任务结束后删除）"""
import zipfile, re, sys

p = r'D:/GarMap/garmap_release/starmap/bin/scripts/zbqd.xlsx'
z = zipfile.ZipFile(p)
names = z.namelist()
print('ENTRIES:')
for n in names:
    print('  ', n)

sheet = [n for n in names if n.startswith('xl/worksheets/') and n.endswith('.xml')]
print('SHEETS:', sheet)
if sheet:
    xml = z.read(sheet[0]).decode('utf-8', errors='replace')
    # 读共享字符串
    sst = []
    try:
        sx = z.read('xl/sharedStrings.xml').decode('utf-8', errors='replace')
        sst = re.findall(r'<t[^>]*>(.*?)</t>', sx, re.S)
    except KeyError:
        pass

    # 行解析
    rowre = re.compile(r'<row[^>]*>.*?</row>', re.S)
    cre = re.compile(r'<c[^>]*r="([A-Z]+)(\d+)"[^>]*>(.*?)</c>', re.S)
    for rm in rowre.finditer(xml):
        rbody = rm.group(0)
        m = re.search(r'r="(\d+)"', rbody)
        rn = m.group(1) if m else '?'
        out = []
        for cm in cre.finditer(rbody):
            col, row, cbody = cm.group(1), cm.group(2), cm.group(3)
            v = ''
            tv = re.search(r'<v>(.*?)</v>', cbody, re.S)
            ism = re.search(r'<is>.*?<t[^>]*>(.*?)</t>.*?</is>', cbody, re.S)
            fm = re.search(r'<f[^>]*>(.*?)</f>', cbody, re.S)
            tattr = re.search(r't="([^"]+)"', cbody)
            if tv:
                if tattr and tattr.group(1) == 's':
                    try:
                        v = sst[int(tv.group(1))]
                    except Exception:
                        v = tv.group(1)
                else:
                    v = tv.group(1)
            elif ism:
                v = ism.group(1)
            txt = col + str(row) + '=' + v
            if fm:
                txt += ' [f:' + fm.group(1) + ']'
            out.append(txt)
        print('ROW %s: %s' % (rn, ' | '.join(out)))
