#include "chart_widget.h"
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QToolTip>
#include <QtMath>
#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>

// ============ 构造 ============

ChartWidget::ChartWidget(QWidget* parent)
	: QWidget(parent)
	, mChartType(Bar2D)
{
	setMinimumSize(400, 300);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setMouseTracking(true);
}

// ============ 公开接口 ============

void ChartWidget::setChartType(ChartType type)
{
	if (mChartType != type)
	{
		mChartType = type;
		update();
	}
}

void ChartWidget::setData(const QList<DynamicStatsRow>& rows,
	const QStringList& metricCols,
	const QMap<QString, QString>& keyToName)
{
	mRows       = rows;
	mMetricCols = metricCols;
	mKeyToName  = keyToName;
	update();
}

void ChartWidget::clearData()
{
	mRows.clear();
	mMetricCols.clear();
	mKeyToName.clear();
	mPieSlices.clear();
	mPieRadius = 0;
	mPieRadiusY = 0;
	update();
}

// ============ 事件 ============

void ChartWidget::paintEvent(QPaintEvent* /*event*/)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	QRect area = rect().adjusted(80, 60, -80, -60);
	if (area.width() <= 0 || area.height() <= 0) return;

	if (mRows.isEmpty() || mMetricCols.isEmpty())
	{
		p.setPen(QColor(150, 150, 150));
		QFont f = p.font();
		f.setPixelSize(60);
		p.setFont(f);
		p.drawText(area, Qt::AlignCenter, QString::fromUtf8("暂无数据，请点击刷新统计"));
		return;
	}

	switch (mChartType)
	{
	case Bar2D:  drawBarChart2D(p, area);  break;
	case Pie2D:  drawPieChart2D(p, area);  break;
	case Bar3D:  drawBarChart3D(p, area);  break;
	case Pie3D:  drawPieChart3D(p, area);  break;
	}
}

void ChartWidget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	update();  // 尺寸变化时重绘
}

void ChartWidget::mouseMoveEvent(QMouseEvent* event)
{
	if (mPieSlices.isEmpty() || mPieRadius <= 0 || mPieRadiusY <= 0)
	{
		QToolTip::hideText();
		QWidget::mouseMoveEvent(event);
		return;
	}

	QPoint pos = event->pos();
	const double dx = pos.x() - mPieCenterX;
	const double dy = pos.y() - mPieCenterY;
	// 顶面为椭圆，按归一化半径做命中判定
	const double nx = dx / (mPieRadius + 2.0);
	const double ny = dy / (mPieRadiusY + 2.0);

	if (nx * nx + ny * ny > 1.0)
	{
		QToolTip::hideText();
		QWidget::mouseMoveEvent(event);
		return;
	}

	// 将屏幕坐标转换为角度：atan2(y 向下为负) → 度数（0=3点钟，逆时针）
	double angle = std::atan2(static_cast<double>(-dy), static_cast<double>(dx)) * 180.0 / M_PI;
	// 归一化到 [0, 360)
	if (angle < 0) angle += 360.0;
	if (angle >= 360.0) angle -= 360.0;

	// Qt drawPie: startAngle=90 表示 12 点钟方向 → 需要把 atan2 角度对齐
	// atan2 0°=3点钟, 90°=12点钟 → 与 Qt 一致，直接使用
	for (const auto& slice : mPieSlices)
	{
		double s = slice.startAngleDeg;
		double e = s + slice.spanAngleDeg;
		// 归一化
		while (s < 0) s += 360.0;
		while (s >= 360.0) s -= 360.0;
		while (e >= 360.0) e -= 360.0;

		bool inside = false;
		if (slice.spanAngleDeg >= 360.0)
		{
			inside = true;
		}
		else if (e > s)
		{
			inside = (angle >= s && angle <= e);
		}
		else
		{
			// 跨 360° 边界
			inside = (angle >= s || angle <= e);
		}

		if (inside)
		{
			QString tip = QString("%1\n%2 (%3%)")
				.arg(slice.name)
				.arg(slice.value)
				.arg(slice.percentage, 0, 'f', 1);
			QToolTip::showText(event->globalPos(), tip, this);
			QWidget::mouseMoveEvent(event);
			return;
		}
	}

	QToolTip::hideText();
	QWidget::mouseMoveEvent(event);
}

// ============ 颜色方案 ============

QList<QColor> ChartWidget::pieColors()
{
	// 暖色调配色：以红、橙、黄、赭、棕为主，明暗交替排列，保证相邻扇形区分度
	return {
		QColor(231, 76, 60),   // 暖红
		QColor(247, 178, 103), // 杏橙
		QColor(192, 57, 43),   // 砖红
		QColor(255, 209, 102), // 暖黄
		QColor(230, 126, 34),  // 橙
		QColor(166, 64, 90),   // 暖玫红
		QColor(244, 132, 95),  // 珊瑚
		QColor(156, 58, 46),   // 深赭红
		QColor(242, 166, 90),  // 浅橙
		QColor(217, 95, 67),   // 陶红
		QColor(184, 92, 56),   // 赭褐
		QColor(255, 180, 162)  // 桃粉
	};
}

QList<QColor> ChartWidget::barColors()
{
	return {
		QColor(74, 144, 217), QColor(91, 143, 249), QColor(97, 221, 170),
		QColor(246, 189, 22), QColor(255, 107, 107), QColor(93, 112, 146),
		QColor(154, 132, 230), QColor(255, 156, 87), QColor(52, 152, 219),
		QColor(46, 204, 113), QColor(230, 76, 60), QColor(241, 196, 15)
	};
}

// ============ Y轴刻度计算（nice numbers） ============

QVector<qint64> ChartWidget::calculateNiceYTicks(qint64 maxVal, int maxTicks)
{
	QVector<qint64> ticks;
	if (maxVal <= 0)
	{
		ticks.append(0);
		ticks.append(1);
		return ticks;
	}

	double range = static_cast<double>(maxVal);
	double roughStep = range / maxTicks;

	// 取 roughStep 的数量级
	double magnitude = std::pow(10.0, std::floor(std::log10(roughStep)));
	double residual = roughStep / magnitude;

	double niceStep;
	if (residual <= 1.5)       niceStep = 1.0 * magnitude;
	else if (residual <= 3.0)  niceStep = 2.0 * magnitude;
	else if (residual <= 7.0)  niceStep = 5.0 * magnitude;
	else                       niceStep = 10.0 * magnitude;

	qint64 step = static_cast<qint64>(std::ceil(niceStep));
	if (step < 1) step = 1;

	// 生成刻度：从 0 到覆盖 maxVal
	qint64 top = static_cast<qint64>(std::ceil(range / step)) * step;
	for (qint64 t = 0; t <= top; t += step)
	{
		ticks.append(t);
		if (ticks.size() > maxTicks + 3) break;
	}

	// 确保至少有 0 和 maxVal 附近的刻度
	if (ticks.size() < 3 && maxVal > 0)
	{
		ticks.clear();
		ticks.append(0);
		qint64 mid = maxVal / 2;
		if (mid > 0) ticks.append(mid);
		ticks.append(maxVal);
	}

	return ticks;
}

QString ChartWidget::formatTickValue(qint64 val)
{
	if (val >= 100000000)       // >= 1亿
		return QString::number(val / 100000000.0, 'f', 1) + QString::fromUtf8("亿");
	else if (val >= 100000)     // >= 10万，用万
		return QString::number(val / 10000.0, 'f', 1) + QString::fromUtf8("万");
	else if (val >= 10000)      // >= 1万
		return QString::number(val / 10000.0, 'f', 2) + QString::fromUtf8("万");
	else
		return QString::number(val);
}

// ============ 通用图例绘制 ============

int ChartWidget::drawLegend(QPainter& p, const QRect& area,
	const QVector<QPair<QString, int>>& items,
	const QList<QColor>& colors, int total, double fontScale)
{
	int legendX = area.x();
	int legendY = area.y();
	int itemH = qMax(18, area.height() / qMax(items.size(), 1));
	if (itemH > 32) itemH = 32;
	int blockSize = qMin(14, itemH - 4);
	int spacing = 2;

	QFont legendFont = p.font();
	int baseSize = qMax(10, itemH * 3 / 5);
	legendFont.setPixelSize(qMax(10, qRound(baseSize * fontScale)));
	p.setFont(legendFont);

	for (int i = 0; i < items.size(); ++i)
	{
		int y = legendY + i * (itemH + spacing);
		if (y + itemH > area.bottom()) break;

		// 颜色方块
		p.setPen(Qt::NoPen);
		p.setBrush(colors[i % colors.size()]);
		int blockY = y + (itemH - blockSize) / 2;
		p.drawRoundedRect(QRect(legendX + 2, blockY, blockSize, blockSize), 2, 2);

		// 标签文本
		p.setPen(QColor(60, 60, 60));
		double pct = total > 0 ? (100.0 * items[i].second / total) : 0;
		QString text = QString("%1  %2 (%3%)")
			.arg(items[i].first).arg(items[i].second).arg(pct, 0, 'f', 1);

		QRect textRect(legendX + blockSize + 8, y,
			area.width() - blockSize - 10, itemH);
		p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
	}

	return legendY + items.size() * (itemH + spacing) - spacing + 10;
}

// ============ 2D 垂直柱状图 ============

void ChartWidget::drawBarChart2D(QPainter& p, const QRect& area)
{
	auto clrs = barColors();

	QFont labelFont = p.font();
	labelFont.setPixelSize(38);

	QFont valFont = p.font();
	valFont.setPixelSize(38);
	valFont.setBold(true);

	int y = area.y();
	int sectionIdx = 0;
	int totalSections = mMetricCols.size();

	for (const auto& col : mMetricCols)
	{
		if (mRows.isEmpty()) continue;

		qint64 maxVal = 0;
		QVector<QPair<QString, qint64>> data;
		for (const auto& row : mRows)
		{
			qint64 val = row.metrics.value(col).toLongLong();
			data.append({ row.timePeriod, val });
			maxVal = qMax(maxVal, val);
		}
		if (data.isEmpty() || maxVal == 0) { sectionIdx++; continue; }

		int sectionsRemaining = totalSections - sectionIdx;
		int availH = area.bottom() - y;
		if (availH < 200) break;

		// 计算 Y 轴 nice ticks
		QVector<qint64> yTicks = calculateNiceYTicks(maxVal, 4);
		qint64 yMax = yTicks.isEmpty() ? maxVal : yTicks.last();

		int secH = qMin(544, availH / sectionsRemaining);
		if (sectionIdx == totalSections - 1)
			secH = qMax(160, availH);

		int leftMargin = 200;
		int topPad    = 8;

		// ========== 自适应柱子间距（标签不重叠优先） ==========
		int barCount = data.size();
		int chartW = area.right() - 30 - (area.x() + leftMargin);

		// 测量所有标签的实际像素宽度
		QFontMetrics labelFm(labelFont);
		int maxLabelW = 0;
		for (const auto& d : data)
			maxLabelW = qMax(maxLabelW, labelFm.horizontalAdvance(d.first));
		maxLabelW += 10;  // 标签左右留白

		// 分级策略：尽量让标签水平显示不重叠
		int barW, gap;
		bool rotateLabels = false;
		bool skipLabels   = false;

		int const MIN_BAR_W  = 36;
		int const WIDE_GAP   = 24;
		int const NORMAL_GAP = 16;
		int const TIGHT_GAP  = 8;

		auto tryFit = [&](int tryBarW, int tryGap) -> bool
		{
			int slotW = tryBarW + tryGap;
			return slotW * barCount - tryGap <= chartW;
		};

		// 策略1：宽敞间距 + 标签宽度等于柱子宽度（最理想）
		if (tryFit(qMax(MIN_BAR_W, maxLabelW), WIDE_GAP))
		{
			barW = qMax(MIN_BAR_W, maxLabelW);
			gap  = WIDE_GAP;
		}
		// 策略2：正常间距
		else if (tryFit(qMax(MIN_BAR_W, maxLabelW), NORMAL_GAP))
		{
			barW = qMax(MIN_BAR_W, maxLabelW);
			gap  = NORMAL_GAP;
		}
		// 策略3：紧凑间距，柱子宽度 = 标签宽度
		else if (tryFit(qMax(MIN_BAR_W, maxLabelW), TIGHT_GAP))
		{
			barW = qMax(MIN_BAR_W, maxLabelW);
			gap  = TIGHT_GAP;
		}
		// 策略4：标签比柱子宽 → 柱子可以窄于标签，用间距补足
		else
		{
			// 每个 slot 至少需要 maxLabelW 宽度防重叠
			int idealSlot = maxLabelW + TIGHT_GAP;
			if (idealSlot * barCount - TIGHT_GAP <= chartW)
			{
				// 柱子可以比标签窄，slot 宽度足够
				barW = qMax(24, maxLabelW);
				gap  = TIGHT_GAP;
				// 如果 slot 还不够，缩小柱子进一步释放间距
				int slotW = barW + gap;
				if (slotW * barCount - gap > chartW)
				{
					barW = qMax(24, maxLabelW);
					gap  = qMax(6, TIGHT_GAP - 2);
					// 如果柱子比标签窄，靠 slot 宽度保证间距
				}
			}

			// 策略5：到此仍放不下 → 旋转标签
			int slotW = barW + gap;
			if (slotW * barCount - gap > chartW)
			{
				rotateLabels = true;
				barW = MIN_BAR_W;
				gap  = TIGHT_GAP;
				// 旋转后柱子可以更窄
				if ((barW + gap) * barCount - gap > chartW)
				{
					barW = qMax(24, chartW / barCount - gap);
				}
				// 极致情况下跳过部分标签
				if (barCount > 8 && (barW + gap) * barCount - gap > chartW)
					skipLabels = true;
			}
		}

		// 柱子宽度减小 25%
		barW = qMax(MIN_BAR_W * 3 / 4, static_cast<int>(barW * 0.75));

		// 分配剩余空间作为额外间距
		{
			int slotW = barW + gap;
			int totalUsed = slotW * barCount - gap;
			int slack = chartW - totalUsed;
			if (slack > 0 && !rotateLabels)
			{
				int extra = slack / barCount;
				gap += extra;
			}
		}

		int slotW = barW + gap;
		int totalBarSpan = slotW * barCount - gap;
		int barStartX = area.x() + leftMargin + (chartW - totalBarSpan) / 2;

	// 旋转标签需要额外高度
	int axisLabelH;
	if (rotateLabels)
		axisLabelH = maxLabelW + 24;
	else
		axisLabelH = 96;

		int chartBodyH = secH - axisLabelH - topPad;
		if (chartBodyH < 100) { sectionIdx++; continue; }

		int chartBottom = y + chartBodyH;
		int chartLeft  = area.x() + leftMargin;
		int chartRight = area.right() - 30;

		// ---- Y 轴网格 & 刻度（nice numbers） ----
		p.setFont(labelFont);
		for (int t = 0; t < yTicks.size(); ++t)
		{
			qint64 tickVal = yTicks[t];
			double ratio = yMax > 0 ? static_cast<double>(tickVal) / yMax : 0;
			int tickY = chartBottom - static_cast<int>(ratio * chartBodyH);

			p.setPen(QColor(100, 100, 100));
			p.drawText(QRect(area.x(), tickY - 32, leftMargin - 12, 64),
				Qt::AlignRight | Qt::AlignVCenter, formatTickValue(tickVal));

			if (t > 0 && tickY > y)
			{
				p.setPen(QPen(QColor(180, 180, 180), 2, Qt::DashLine));
				p.drawLine(chartLeft, tickY, chartRight, tickY);
			}
		}

		// ---- X / Y 轴实线（主题色 + 箭头） ----
		QColor axisColor = clrs[0];
		p.setPen(QPen(axisColor, 2));
		p.drawLine(chartLeft, y, chartLeft, chartBottom);          // Y 轴
		p.drawLine(chartLeft, chartBottom, chartRight, chartBottom); // X 轴

		const int arrowR = 7;
		// Y 轴箭头（向上）
		{
			QPainterPath head;
			head.moveTo(chartLeft - arrowR, y + arrowR * 2);
			head.lineTo(chartLeft, y);
			head.lineTo(chartLeft + arrowR, y + arrowR * 2);
			p.setPen(Qt::NoPen);
			p.setBrush(axisColor);
			p.drawPath(head);
		}
		// X 轴箭头（向右）
		{
			QPainterPath head;
			head.moveTo(chartRight - arrowR * 2, chartBottom - arrowR);
			head.lineTo(chartRight, chartBottom);
			head.lineTo(chartRight - arrowR * 2, chartBottom + arrowR);
			p.setBrush(axisColor);
			p.drawPath(head);
		}

		// ---- 垂直柱形 ----
		for (int i = 0; i < data.size(); ++i)
		{
			int barX = barStartX + i * slotW;
			double ratio = yMax > 0 ? static_cast<double>(data[i].second) / yMax : 0;
			int barH = static_cast<int>(ratio * chartBodyH);
			if (barH < 2 && data[i].second > 0) barH = 2;
			int barTop = chartBottom - barH;

			QColor base = clrs[i % clrs.size()];
			QLinearGradient grad(barX, barTop, barX, chartBottom);
			grad.setColorAt(0.0, base.lighter(140));
			grad.setColorAt(0.4, base);
			grad.setColorAt(1.0, base.darker(140));

			p.setPen(Qt::NoPen);
			p.setBrush(grad);
			p.drawRoundedRect(QRect(barX, barTop, barW, barH), 4, 4);

			// 数值（柱顶上方）
			p.setFont(valFont);
			p.setPen(QColor(50, 50, 50));
			p.drawText(QRect(barX - 20, barTop - 52, barW + 40, 48),
				Qt::AlignCenter, formatTickValue(data[i].second));

			// X 轴标签
			p.setFont(labelFont);
			p.setPen(QColor(60, 60, 60));

			if (skipLabels && (i % 2 != 0))
				continue;

			if (rotateLabels)
			{
				p.save();
				p.translate(barX + barW / 2, chartBottom + axisLabelH - 10);
				p.rotate(45);
				p.drawText(QRect(-80, -24, 160, 48), Qt::AlignLeft | Qt::AlignVCenter, data[i].first);
				p.restore();
			}
			else
			{
				// 标签居中于整个 slot，保证不重叠
				int labelX = barStartX + i * slotW - gap / 2;
				int labelW = slotW;
				p.drawText(QRect(labelX, chartBottom + 6, labelW, axisLabelH - 12),
					Qt::AlignCenter, data[i].first);
			}
		}

		y = chartBottom + axisLabelH;
		sectionIdx++;
	}
}

// ============ 2D 饼图（无内部标注） ============

void ChartWidget::drawPieChart2D(QPainter& p, const QRect& area)
{
	auto clrs = pieColors();

	QVector<QPair<QString, int>> items;
	int total = 0;
	for (const auto& col : mMetricCols)
	{
		qint64 sum = 0;
		for (const auto& row : mRows)
			sum += row.metrics.value(col).toLongLong();
		if (sum > 0)
		{
			items.append({ mKeyToName.value(col, col), static_cast<int>(sum) });
			total += static_cast<int>(sum);
		}
	}
	if (items.isEmpty() || total <= 0) return;

	// 居中饼图
	int pieSize = qMin(area.width() / 3, area.height() / 2 - 60);
	if (pieSize < 60) pieSize = 60;
	if (pieSize > 1040) pieSize = 1040;
	int pieRadius = pieSize / 2;
	int pieCenterX = area.center().x();
	int pieCenterY = area.y() + pieRadius + 20;

	// 记录扇形区域（供 tooltip 使用）
	mPieSlices.clear();
	mPieCenterX = pieCenterX;
	mPieCenterY = pieCenterY;
	mPieRadius  = pieRadius;

	// 绘制饼图扇形
	double startAngle = 90.0 * 16;
	for (int i = 0; i < items.size(); ++i)
	{
		double span = total > 0 ? (360.0 * 16 * items[i].second / total) : 0;
		if (span <= 0) continue;

		QColor c = clrs[i % clrs.size()];
		p.setPen(QPen(Qt::white, 2));
		p.setBrush(c);
		p.drawPie(pieCenterX - pieRadius, pieCenterY - pieRadius,
			pieSize, pieSize, qRound(startAngle), qRound(span));

		// 保存扇形数据
		PieSlice slice;
		slice.name       = items[i].first;
		slice.value      = items[i].second;
		slice.percentage = total > 0 ? (100.0 * items[i].second / total) : 0;
		slice.color      = c;
		slice.startAngleDeg = startAngle / 16.0;
		slice.spanAngleDeg  = span / 16.0;
		while (slice.startAngleDeg < 0) slice.startAngleDeg += 360.0;
		while (slice.startAngleDeg >= 360.0) slice.startAngleDeg -= 360.0;
		mPieSlices.append(slice);

		startAngle += span;
	}

	// 图例（底部居中）
	int legendY = pieCenterY + pieRadius + 10;
	QRect legendArea(area.x(), legendY, area.width(), area.bottom() - legendY);
	drawLegend(p, legendArea, items, clrs, total, 1.5);
}

// ============ 3D 垂直柱状图 ============

void ChartWidget::drawBarChart3D(QPainter& p, const QRect& area)
{
	auto clrs = barColors();
	int depth = 16;

	QFont labelFont = p.font();
	labelFont.setPixelSize(38);

	QFont valFont = p.font();
	valFont.setPixelSize(38);
	valFont.setBold(true);

	int y = area.y();
	int sectionIdx = 0;
	int totalSections = mMetricCols.size();

	for (const auto& col : mMetricCols)
	{
		if (mRows.isEmpty()) continue;

		qint64 maxVal = 0;
		QVector<QPair<QString, qint64>> data;
		for (const auto& row : mRows)
		{
			qint64 val = row.metrics.value(col).toLongLong();
			data.append({ row.timePeriod, val });
			maxVal = qMax(maxVal, val);
		}
		if (data.isEmpty() || maxVal == 0) { sectionIdx++; continue; }

		int sectionsRemaining = totalSections - sectionIdx;
		int availH = area.bottom() - y;
		if (availH < 200) break;

		// 计算 Y 轴 nice ticks
		QVector<qint64> yTicks = calculateNiceYTicks(maxVal, 4);
		qint64 yMax = yTicks.isEmpty() ? maxVal : yTicks.last();

		int secH = qMin(560, availH / sectionsRemaining);
		if (sectionIdx == totalSections - 1)
			secH = qMax(160, availH);

		int leftMargin = 200;
		int topPad    = 10;

		// ========== 自适应柱子间距（标签不重叠优先） ==========
		int barCount = data.size();
		int chartW = area.right() - 30 - (area.x() + leftMargin) - depth;

		// 测量所有标签的实际像素宽度
		QFontMetrics labelFm(labelFont);
		int maxLabelW = 0;
		for (const auto& d : data)
			maxLabelW = qMax(maxLabelW, labelFm.horizontalAdvance(d.first));
		maxLabelW += 10;

		// 分级策略
		int barW, gap;
		bool rotateLabels = false;
		bool skipLabels   = false;

		int const MIN_BAR_W  = 36;
		int const WIDE_GAP   = 24;
		int const NORMAL_GAP = 16;
		int const TIGHT_GAP  = 8;

		auto tryFit = [&](int tryBarW, int tryGap) -> bool
		{
			int slotW = tryBarW + tryGap;
			return slotW * barCount - tryGap <= chartW;
		};

		if (tryFit(qMax(MIN_BAR_W, maxLabelW), WIDE_GAP))
		{
			barW = qMax(MIN_BAR_W, maxLabelW);
			gap  = WIDE_GAP;
		}
		else if (tryFit(qMax(MIN_BAR_W, maxLabelW), NORMAL_GAP))
		{
			barW = qMax(MIN_BAR_W, maxLabelW);
			gap  = NORMAL_GAP;
		}
		else if (tryFit(qMax(MIN_BAR_W, maxLabelW), TIGHT_GAP))
		{
			barW = qMax(MIN_BAR_W, maxLabelW);
			gap  = TIGHT_GAP;
		}
		else
		{
			int idealSlot = maxLabelW + TIGHT_GAP;
			if (idealSlot * barCount - TIGHT_GAP <= chartW)
			{
				barW = qMax(24, maxLabelW);
				gap  = TIGHT_GAP;
				int slotW = barW + gap;
				if (slotW * barCount - gap > chartW)
				{
					barW = qMax(24, maxLabelW);
					gap  = qMax(6, TIGHT_GAP - 2);
				}
			}

			int slotW = barW + gap;
			if (slotW * barCount - gap > chartW)
			{
				rotateLabels = true;
				barW = MIN_BAR_W;
				gap  = TIGHT_GAP;
				if ((barW + gap) * barCount - gap > chartW)
				{
					barW = qMax(24, chartW / barCount - gap);
				}
				if (barCount > 8 && (barW + gap) * barCount - gap > chartW)
					skipLabels = true;
			}
		}

		// 柱子宽度减小 25%
		barW = qMax(MIN_BAR_W * 3 / 4, static_cast<int>(barW * 0.75));

		// 分配剩余空间
		{
			int slotW = barW + gap;
			int totalUsed = slotW * barCount - gap;
			int slack = chartW - totalUsed;
			if (slack > 0 && !rotateLabels)
			{
				int extra = slack / barCount;
				gap += extra;
			}
		}

		int slotW = barW + gap;
		int totalBarSpan = slotW * barCount - gap;
		int barStartX = area.x() + leftMargin + (chartW - totalBarSpan) / 2;

	int axisLabelH;
	if (rotateLabels)
		axisLabelH = maxLabelW + 24;
	else
		axisLabelH = 96;

	int chartBodyH = secH - axisLabelH - topPad - depth;
		if (chartBodyH < 100) { sectionIdx++; continue; }

		int chartBottom = y + chartBodyH;
		int chartLeft  = area.x() + leftMargin;
		int chartRight = area.right() - 30;

		// ---- Y 轴网格 & 刻度（nice numbers） ----
		p.setFont(labelFont);
		for (int t = 0; t < yTicks.size(); ++t)
		{
			qint64 tickVal = yTicks[t];
			double ratio = yMax > 0 ? static_cast<double>(tickVal) / yMax : 0;
			int tickY = chartBottom - static_cast<int>(ratio * chartBodyH);

			p.setPen(QColor(100, 100, 100));
			p.drawText(QRect(area.x(), tickY - 32, leftMargin - 12, 64),
				Qt::AlignRight | Qt::AlignVCenter, formatTickValue(tickVal));

			if (t > 0 && tickY > y)
			{
				p.setPen(QPen(QColor(180, 180, 180), 2, Qt::DashLine));
				p.drawLine(chartLeft, tickY, chartRight, tickY);
			}
		}

		// ---- X / Y 轴实线（主题色 + 箭头） ----
		QColor axisColor = clrs[0];
		p.setPen(QPen(axisColor, 2));
		p.drawLine(chartLeft, y, chartLeft, chartBottom);          // Y 轴
		p.drawLine(chartLeft, chartBottom, chartRight, chartBottom); // X 轴

		const int arrowR = 7;
		// Y 轴箭头（向上）
		{
			QPainterPath head;
			head.moveTo(chartLeft - arrowR, y + arrowR * 2);
			head.lineTo(chartLeft, y);
			head.lineTo(chartLeft + arrowR, y + arrowR * 2);
			p.setPen(Qt::NoPen);
			p.setBrush(axisColor);
			p.drawPath(head);
		}
		// X 轴箭头（向右）
		{
			QPainterPath head;
			head.moveTo(chartRight - arrowR * 2, chartBottom - arrowR);
			head.lineTo(chartRight, chartBottom);
			head.lineTo(chartRight - arrowR * 2, chartBottom + arrowR);
			p.setBrush(axisColor);
			p.drawPath(head);
		}

		// ---- 3D 垂直柱形 ----
		for (int i = 0; i < data.size(); ++i)
		{
			int barX = barStartX + i * slotW;
			double ratio = yMax > 0 ? static_cast<double>(data[i].second) / yMax : 0;
			int barH = static_cast<int>(ratio * chartBodyH);
			if (barH < 2 && data[i].second > 0) barH = 2;
			int barTop = chartBottom - barH;

			QColor base   = clrs[i % clrs.size()];
			QColor dark   = base.darker(145);
			QColor light  = base.lighter(140);

			// ---- 正面 ----
			p.setPen(QPen(base.darker(125), 1));
			QLinearGradient frontGrad(barX, barTop, barX, chartBottom);
			frontGrad.setColorAt(0.0, light);
			frontGrad.setColorAt(0.5, base);
			frontGrad.setColorAt(1.0, base.darker(120));
			p.setBrush(frontGrad);
			p.drawRect(QRect(barX, barTop, barW, barH));

			// ---- 右侧面 ----
			{
				QPainterPath side;
				side.moveTo(barX + barW, barTop);
				side.lineTo(barX + barW + depth, barTop - depth);
				side.lineTo(barX + barW + depth, chartBottom - depth);
				side.lineTo(barX + barW, chartBottom);
				side.closeSubpath();
				p.setPen(QPen(base.darker(130), 1));
				p.setBrush(dark);
				p.drawPath(side);
			}

			// ---- 顶面 ----
			{
				QPainterPath top;
				top.moveTo(barX, barTop);
				top.lineTo(barX + depth, barTop - depth);
				top.lineTo(barX + barW + depth, barTop - depth);
				top.lineTo(barX + barW, barTop);
				top.closeSubpath();
				p.setPen(QPen(base.darker(110), 1));
				p.setBrush(base.lighter(160));
				p.drawPath(top);
			}

			// 数值（3D 柱顶右上方）
			p.setFont(valFont);
			p.setPen(QColor(50, 50, 50));
			p.drawText(QRect(barX + depth - 20, barTop - depth - 54, barW + 50, 48),
				Qt::AlignCenter, formatTickValue(data[i].second));

			// X 轴标签
			p.setFont(labelFont);
			p.setPen(QColor(60, 60, 60));

			if (skipLabels && (i % 2 != 0))
				continue;

			if (rotateLabels)
			{
				p.save();
				p.translate(barX + barW / 2, chartBottom + axisLabelH - 10);
				p.rotate(45);
				p.drawText(QRect(-80, -24, 160, 48), Qt::AlignLeft | Qt::AlignVCenter, data[i].first);
				p.restore();
			}
			else
			{
				int labelX = barStartX + i * slotW - gap / 2;
				int labelW = slotW;
				p.drawText(QRect(labelX, chartBottom + 6, labelW, axisLabelH - 12),
					Qt::AlignCenter, data[i].first);
			}
		}

		y = chartBottom + axisLabelH + depth;
		sectionIdx++;
	}
}

// ============ 伪 3D 饼图（通用实现，控件与报表导出共用） ============

// 椭圆上指定角度对应的点：Qt 角度 0°=3 点钟方向，逆时针为正
static QPointF pointOnPieEllipse(const QRectF& rect, double angleDeg)
{
	const double rad = qDegreesToRadians(angleDeg);
	return QPointF(rect.center().x() + rect.width()  / 2.0 * std::cos(rad),
		rect.center().y() - rect.height() / 2.0 * std::sin(rad));
}

void ChartWidget::drawPie3D(QPainter& p, const QPoint& topCenter, int radius, int depth,
	const QVector<QPair<QString, int>>& items, const QList<QColor>& colors,
	QVector<PieSlice>* outSlices, double verticalScale, bool drawGloss)
{
	if (items.isEmpty() || colors.isEmpty() || radius <= 0) return;

	qint64 totalSum = 0;
	for (const auto& item : items) totalSum += item.second;
	if (totalSum <= 0) return;
	const double total = static_cast<double>(totalSum);

	if (depth < 1) depth = 1;
	if (verticalScale <= 0.0 || verticalScale > 1.0) verticalScale = pie3DVerticalScale();

	const double cx = topCenter.x();
	const double cy = topCenter.y();
	const double ry = radius * verticalScale;
	const QRectF topRect(cx - radius, cy - ry, radius * 2.0, ry * 2.0);
	const QRectF bottomRect = topRect.translated(0.0, depth);

	// Qt 角度下 180°~360° 为朝向观察者的前半圆（270° = 6 点钟方向）
	const double frontFrom = 180.0;
	const double frontTo   = 360.0;

	p.save();

	// ---- 底部柔和投影，让饼图"浮"起来 ----
	p.setPen(Qt::NoPen);
	for (int k = 6; k >= 1; --k)
	{
		p.setBrush(QColor(0, 0, 0, 7));
		p.drawEllipse(QRectF(cx - radius - k, cy + ry * 0.55 + depth - k * 0.3,
			radius * 2.0 + k * 2.0, ry * 1.1 + k));
	}

	// ---- 侧壁：把前半圆的圆弧向下拉伸成柱面，纵向渐变着色 ----
	double angleCursor = 90.0;   // 与 Qt drawPie 一致，从 12 点钟方向起算
	for (int i = 0; i < items.size(); ++i)
	{
		const double span = 360.0 * items[i].second / total;
		const double a0 = angleCursor;
		const double a1 = angleCursor + span;
		angleCursor = a1;
		if (span <= 0.02) continue;

		const QColor base = colors[i % colors.size()];
		const double gradBottom = cy + ry + depth;

		for (int k = 0; k <= 1; ++k)
		{
			const double s = qMax(a0, frontFrom + 360.0 * k);
			const double e = qMin(a1, frontTo   + 360.0 * k);
			if (e - s <= 0.02) continue;

			QPainterPath wall;
			wall.arcMoveTo(topRect, s);
			wall.arcTo(topRect, s, e - s);
			wall.lineTo(pointOnPieEllipse(bottomRect, e));
			wall.arcTo(bottomRect, e, -(e - s));
			wall.closeSubpath();

			QLinearGradient grad(cx, cy, cx, gradBottom);
			grad.setColorAt(0.00, base.lighter(122));
			grad.setColorAt(0.55, base.darker(118));
			grad.setColorAt(1.00, base.darker(168));

			p.setPen(QPen(base.darker(195), 1.0));
			p.setBrush(grad);
			p.drawPath(wall);
		}
	}

	// ---- 顶面：完整扇形，扇形之间用白色细线分隔 ----
	double startAngle = 90.0;
	for (int i = 0; i < items.size(); ++i)
	{
		const double span = 360.0 * items[i].second / total;
		if (span <= 0.02) continue;

		const QColor color = colors[i % colors.size()];
		p.setPen(QPen(Qt::white, 2));
		p.setBrush(color);
		p.drawPie(topRect, qRound(startAngle * 16.0), qRound(span * 16.0));

		if (outSlices)
		{
			PieSlice slice;
			slice.name       = items[i].first;
			slice.value      = items[i].second;
			slice.percentage = 100.0 * items[i].second / total;
			slice.color      = color;
			slice.startAngleDeg = std::fmod(startAngle, 360.0);
			if (slice.startAngleDeg < 0) slice.startAngleDeg += 360.0;
			slice.spanAngleDeg  = span;
			outSlices->append(slice);
		}

		startAngle += span;
	}

	// ---- 顶面玻璃高光 + 外轮廓，强化立体质感 ----
	// 高光为带 alpha 的径向渐变：QPdfEngine 会丢弃渐变的 alpha 并当作不透明填充，
	// 从而把整个顶面盖成白色（PDF 中饼图“空白”的根因），故 PDF 导出时关闭高光。
	if (drawGloss)
	{
		QRadialGradient gloss(QPointF(cx - radius * 0.35, cy - ry * 0.45), radius * 1.25);
		gloss.setColorAt(0.0, QColor(255, 255, 255, 92));
		gloss.setColorAt(0.5, QColor(255, 255, 255, 30));
		gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
		p.setPen(Qt::NoPen);
		p.setBrush(gloss);
		p.drawEllipse(topRect);
	}

	p.setBrush(Qt::NoBrush);
	p.setPen(QPen(QColor(0, 0, 0, 55), 1.0));
	p.drawEllipse(topRect);

	p.restore();
}

// ============ 3D 饼图（无内部标注） ============

void ChartWidget::drawPieChart3D(QPainter& p, const QRect& area)
{
	auto clrs = pieColors();

	QVector<QPair<QString, int>> items;
	int total = 0;
	for (const auto& col : mMetricCols)
	{
		qint64 sum = 0;
		for (const auto& row : mRows)
			sum += row.metrics.value(col).toLongLong();
		if (sum > 0)
		{
			items.append({ mKeyToName.value(col, col), static_cast<int>(sum) });
			total += static_cast<int>(sum);
		}
	}
	if (items.isEmpty() || total <= 0) return;

	// 顶面椭圆水平直径：先为底部图例预留高度，余下空间按整体高度（≈0.62 直径 + 侧壁厚度）确定尺寸
	const int legendNeed = qMin(area.height() / 2, items.size() * 34 + 16);
	const int availH = qMax(140, area.height() - legendNeed);
	int pieSize = qMin(area.width() * 2 / 5, availH * 4 / 5);
	if (pieSize < 120) pieSize = 120;
	if (pieSize > 1040) pieSize = 1040;

	const int pieRadius  = pieSize / 2;
	const int pieRadiusY = qRound(pieRadius * pie3DVerticalScale());
	const int depth      = qBound(10, pieRadius / 5, 42);

	// 整体（椭圆顶面 + 侧壁）在绘图区内垂直居中
	const int graphicH = pieRadiusY * 2 + depth;
	const int pieCenterX = area.center().x();
	const int pieCenterY = area.y() + qMax(0, (availH - graphicH) / 2) + pieRadiusY;

	// 记录扇形区域（供 tooltip 使用）
	mPieSlices.clear();
	mPieCenterX = pieCenterX;
	mPieCenterY = pieCenterY;
	mPieRadius  = pieRadius;
	mPieRadiusY = pieRadiusY;

	drawPie3D(p, QPoint(pieCenterX, pieCenterY), pieRadius, depth, items, clrs, &mPieSlices);

	// 图例（底部居中）
	int legendY = pieCenterY + pieRadiusY + depth + 12;
	QRect legendArea(area.x(), legendY, area.width(), area.bottom() - legendY);
	drawLegend(p, legendArea, items, clrs, total, 1.5);
}
