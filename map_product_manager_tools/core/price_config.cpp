#include "price_config.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QRegularExpression>

// 去掉小数末尾多余的 0（0.80 → 0.8，1.00 → 1）
static QString trimNumber(QString num)
{
	if (num.contains(QLatin1Char('.')))
	{
		while (num.endsWith(QLatin1Char('0')))
			num.chop(1);
		if (num.endsWith(QLatin1Char('.')))
			num.chop(1);
	}
	return num;
}

// 把单个尺寸分量解析为"米"数值。
// 支持写法：0.8 / 1.2米 / 1.2m / 86厘米 / 86cm / 86公分；无单位按米处理。
static double partToMeters(const QString& part, bool* ok)
{
	static const QRegularExpression re(
		QStringLiteral("^\\s*([0-9]+(?:\\.[0-9]+)?)\\s*(米|m|厘米|公分|cm)?\\s*$"),
		QRegularExpression::CaseInsensitiveOption);
	const QRegularExpressionMatch m = re.match(part);
	if (!m.hasMatch())
	{
		*ok = false;
		return 0.0;
	}
	bool numOk = false;
	double val = m.captured(1).toDouble(&numOk);
	if (!numOk)
	{
		*ok = false;
		return 0.0;
	}
	const QString unit = m.captured(2).toLower();
	if (unit == QStringLiteral("厘米") || unit == QStringLiteral("公分") || unit == QStringLiteral("cm"))
		val = val / 100.0; // 厘米 → 米
	*ok = true;
	return val;
}

// 规范化图框规格：
//  - 统一分隔符（全角乘号 ×、半角 x/X、星号 *）为 "x"
//  - 去除空格
//  - 单位统一为米：86厘米×355厘米 与 0.86×3.55 规范化为同一个 key "0.86x3.55"
//  - 两边的数字统一格式（去掉多余的 0，如 0.80 → 0.8）
QString PriceConfig::normalizeSpec(const QString& spec)
{
	QString s = spec.trimmed();
	s.replace(QStringLiteral("×"), QStringLiteral("x"));
	s.replace(QStringLiteral("＊"), QStringLiteral("x"));
	s.replace(QLatin1Char('X'), QLatin1Char('x'));
	s.replace(QLatin1Char('*'), QLatin1Char('x'));
	s.remove(QLatin1Char(' '));

	// 拆出两个尺寸并统一换算为米（0.8×1.2 / 0.80x1.20 / 86厘米×355厘米 等）
	QStringList parts = s.split(QLatin1Char('x'), Qt::SkipEmptyParts);
	if (parts.size() == 2)
	{
		bool ok1 = false, ok2 = false;
		const double a = partToMeters(parts[0], &ok1);
		const double b = partToMeters(parts[1], &ok2);
		if (ok1 && ok2)
		{
			// 使用紧凑格式：整数不显示小数点，小数去掉尾部 0
			const QString sa = QString::number(a, 'f', 4);
			const QString sb = QString::number(b, 'f', 4);
			return QStringLiteral("%1x%2")
				.arg(trimNumber(sa), trimNumber(sb));
		}
	}
	return s;
}

bool PriceConfig::load(const QString& filePath)
{
	m_items.clear();

	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
	file.close();

	if (parseError.error != QJsonParseError::NoError || !doc.isObject())
		return false;

	const QJsonObject root = doc.object();
	const QJsonArray table = root.value(QStringLiteral("价格表")).toArray();
	for (const QJsonValue& v : table)
	{
		if (!v.isObject()) continue;
		const QJsonObject o = v.toObject();

		Item item;
		item.frameSpec = normalizeSpec(o.value(QStringLiteral("图框规格")).toString());
		item.material  = o.value(QStringLiteral("材质")).toString().trimmed();
		item.unitPrice = o.value(QStringLiteral("单价")).toDouble();

		if (!item.frameSpec.isEmpty() && !item.material.isEmpty())
			m_items.append(item);
	}
	return !m_items.isEmpty();
}

double PriceConfig::lookup(const QString& frameSpec, const QString& material) const
{
	const QString normSpec = normalizeSpec(frameSpec);
	const QString mat = material.trimmed();

	for (const Item& item : m_items)
	{
		if (item.frameSpec == normSpec && item.material == mat)
			return item.unitPrice;
	}
	return -1.0;
}
