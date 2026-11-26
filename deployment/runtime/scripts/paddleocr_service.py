#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
PaddleOCR 服务脚本（简化版）
  用法: python paddleocr_service.py <lang> <image_path> <output_json>
  C++ 侧已用 OpenCV 提取黑色文字通道，本脚本只负责 OCR 识别。

语言映射:
  chi_sim, ch     → 'ch' (简体中文+英文)
  chi_tra         → 'chinese_cht' (繁体中文)
  eng, en         → 'en' (英文)
"""
import sys
import json
import os

# ========== 必须在 import paddle 之前设置环境变量 ==========
os.environ['FLAGS_use_onednn'] = '0'

# ========== 加载 PaddleOCR（首次慢，后续快） ==========
def load_ocr(lang_raw, det_db_thresh=0.3, det_db_box_thresh=0.6,
             det_db_unclip_ratio=1.0, ocr_version=None):
    lang_map = {
        'chi_sim': 'ch', 'ch': 'ch',
        'chi_tra': 'chinese_cht',
        'chi_sim+eng': 'ch',
        'eng': 'en', 'en': 'en',
    }
    lang = lang_map.get(lang_raw, 'ch')
    from paddleocr import PaddleOCR
    kwargs = dict(
        lang=lang,
        use_textline_orientation=False,
        enable_mkldnn=False,
        use_doc_orientation_classify=False,
        use_doc_unwarping=False,
        text_det_limit_side_len=1500,
        text_det_thresh=det_db_thresh,
        text_det_box_thresh=det_db_box_thresh,
        text_det_unclip_ratio=det_db_unclip_ratio,
    )
    if ocr_version:
        kwargs['ocr_version'] = ocr_version
    return PaddleOCR(**kwargs)


def ocr_one(ocr, img_path):
    """对单张图执行 OCR，返回 JSON 字符串"""
    try:
        result = ocr.predict(img_path)
    except Exception as e:
        return json.dumps({'error': str(e)}, ensure_ascii=False)

    output = []
    if result:
        for item in result:
            rec_texts = item.get('rec_texts', [])
            rec_scores = item.get('rec_scores', [])
            dt_polys = item.get('dt_polys', [])
            for i, text in enumerate(rec_texts):
                if not text.strip():
                    continue
                conf = rec_scores[i] if i < len(rec_scores) else 0.0
                if dt_polys and i < len(dt_polys):
                    poly = dt_polys[i]
                    if hasattr(poly, 'tolist'):
                        poly = poly.tolist()
                    x1 = int(poly[0][0]) if len(poly) > 0 else 0
                    y1 = int(poly[0][1]) if len(poly) > 0 else 0
                    x2 = int(poly[2][0]) if len(poly) > 2 else 0
                    y2 = int(poly[2][1]) if len(poly) > 2 else 0
                else:
                    x1 = y1 = x2 = y2 = 0
                output.append({
                    'text': text,
                    'x1': x1, 'y1': y1, 'x2': x2, 'y2': y2,
                    'confidence': conf
                })
    return json.dumps(output, ensure_ascii=False)


# ========== 主入口 ==========
def main():
    if len(sys.argv) < 4:
        print('Usage: paddleocr_service.py <lang> <image_path> <output_json>')
        sys.exit(1)

    lang = sys.argv[1]
    img_path = sys.argv[2]
    out_path = sys.argv[3]

    if not os.path.isfile(img_path):
        with open(out_path, 'w', encoding='utf-8') as f:
            json.dump({'error': f'File not found: {img_path}'}, f, ensure_ascii=False)
        sys.exit(1)

    ocr = load_ocr(lang)
    result_json = ocr_one(ocr, img_path)

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(result_json)


if __name__ == '__main__':
    main()
