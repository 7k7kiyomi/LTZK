# 质检结果全量扫描与路径归组设计

## 目标

修复全量质检结果较多时图层管理只显示平铺图层的问题：取消单次扫描 50 个 SHP 的限制，并在质检任务完成后的补扫中，按每个 SHP 的真实源路径重新归入对应的目录层级。

## 设计

1. 保留现有递归扫描、稳定性判断和 `kMaxQualityResultScanFilesPerRoot` 文件遍历保护，只移除 `kMaxQualityResultLayersPerScan` 及其 `addedCount` 截断条件，确保一次任务的所有结果 SHP 都有机会被加载。
2. 新增一个只处理已加载图层的归组步骤，遍历项目中的所有 `QgsVectorLayer`，过滤出选定质检输出目录下的 SHP，使用 `localPathFromLayerSource()` 获取真实路径，并复用 `moveQualityResultLayerToGroupSafely()` 挂入路径对应的目录组。
3. 将归组步骤放在任务完成后启动的补扫定时器中，并在发现归组变化或新图层时刷新结果面板。扫描新发现的 SHP 仍通过现有 `addQualityResultLayerToTree()` 直接按真实路径归组。

## 验收标准

- 源码不再包含单次 50 层的质量结果扫描限制或对应提前退出条件。
- 任务完成后的补扫能处理已经加载但当前平铺的所有结果 SHP。
- 每个结果图层的目标分组由其真实源路径决定，而不是由发现顺序决定。
- 现有质量结果加载、面板刷新和稳定性扫描回归检查继续通过。
