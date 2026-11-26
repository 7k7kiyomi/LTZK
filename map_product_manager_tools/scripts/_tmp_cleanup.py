# -*- coding: utf-8 -*-
import os, shutil, sys

FILES = [
    r"d:\GarMap\qgis_plugins\cplusplus\map_product_manager_tools\scripts\_tmp_extract_docx.py",
    r"d:\GarMap\qgis_plugins\cplusplus\map_product_manager_tools\scripts\_tmp_doc_content.txt",
    r"d:\GarMap\qgis_plugins\cplusplus\map_product_manager_tools\scripts\_tmp_validate_docx.py",
    r"d:\GarMap\qgis_plugins\cplusplus\map_product_manager_tools\scripts\_gen_map_product_summary.js",
]
DIRS = [
    r"E:\00-GARMAP\1_实施项目\3-LTZK\3-文档编写\验收相关文档\unpacked_7-9",
]

def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    for f in FILES:
        if os.path.exists(f):
            os.remove(f)
            print("removed:", f)
    for d in DIRS:
        if os.path.isdir(d):
            shutil.rmtree(d, ignore_errors=True)
            print("removed-dir:", d)

if __name__ == "__main__":
    main()
