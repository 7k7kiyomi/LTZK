#ifndef PRICE_CONFIG_H
#define PRICE_CONFIG_H

#include <QString>
#include <QVector>

/**
 * @brief 装裱单价配置（读取 scripts/prices.json）
 *
 * 由图框规格（米）和材质唯一确定单价（元）。
 * 规格匹配前会做规范化（统一 ×/x/* 分隔符、去空格、数值统一格式），
 * 因此用户输入 "0.8*1.2"、"0.8x1.2"、"0.80×1.20" 均能匹配配置项 "0.8×1.2"。
 */
class PriceConfig
{
public:
	struct Item
	{
		QString frameSpec;   // 规范化后的图框规格（米）
		QString material;    // 材质
		double unitPrice = 0.0; // 单价（元）
	};

	// 从 JSON 文件加载价格表，成功返回 true
	bool load(const QString& filePath);

	// 根据图框规格、材质查询单价（元）；未匹配返回 -1.0
	double lookup(const QString& frameSpec, const QString& material) const;

	// 是否成功加载
	bool isValid() const { return !m_items.isEmpty(); }

	// 规格规范化：统一分隔符（×/*/x/X）为 x，去空格，数值统一格式
	static QString normalizeSpec(const QString& spec);

	const QVector<Item>& items() const { return m_items; }

private:
	QVector<Item> m_items;
};

#endif // PRICE_CONFIG_H
