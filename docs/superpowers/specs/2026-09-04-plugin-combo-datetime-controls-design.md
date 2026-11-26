# 插件下拉框与日期时间控件修复设计

## 目标

修复插件窗口中的普通 `QComboBox` 被宿主程序误设为只读的问题，并恢复 `QDateEdit`/`QDateTimeEdit` 在平台主题下的下拉箭头显示。

## 根因

`polishDrawingLayoutTreeEmbeddedCombos()` 原本用于图层关系树中的嵌入式下拉框，但当前通过 `widget->findChildren<QComboBox*>()` 处理整个插件窗口，导致服务单中的普通下拉框也执行了 `setEditable(true)` 和内部 `QLineEdit::setReadOnly(true)`。

平台 QSS 只定义了日期时间控件的 `::drop-down`，没有定义对应的 `::down-arrow`。自定义下拉区域后，Qt 基础样式的日期箭头不会可靠显示，因此出现只有右侧分隔区域、没有箭头的现象。

## 设计

1. 保留图层关系树嵌入式下拉框现有的字体、颜色、文字抑制和自绘箭头处理。
2. 通过祖先 `QTreeWidget` 的对象名 `treeWidget_csv` 识别目标下拉框；普通插件下拉框不进入该特殊处理流程。
3. 在五套平台主题中为 `QDateEdit` 和 `QDateTimeEdit` 补充 `::down-arrow`，深色主题使用 `combo_arrow_dark.svg`，浅色主题使用 `combo_arrow_light.svg`。
4. 不修改插件业务数据、下拉框选项、日期时间值或数据库流程。

## 验收标准

- 服务单位等普通插件 `QComboBox` 保留插件自身的手动编辑能力，并仍可下拉选择。
- 图层关系树中的嵌入式 `QComboBox` 继续显示主题箭头和当前文字。
- 服务接收时间、服务提交时间的下拉箭头在五套主题中均可见。
- 回归检查能够阻止特殊处理重新扩散到普通插件下拉框，并检查五套主题的日期箭头规则。
