#ifndef CHART_WIDGET_H
#define CHART_WIDGET_H

#include <QWidget>
#include <QMap>
#include <QString>
#include <QList>
#include <QPair>
#include <QVector>
#include <QColor>
#include "database/service_record.h"

/**
 * @brief 伪 3D 饼图顶面椭圆的纵向压缩比（1.0 = 正圆，越小越接近俯视的立体效果）
 */
inline double pie3DVerticalScale() { return 0.62; }

/**
 * @brief 自定义统计图表控件
 *
 * 支持四种渲染模式：
 *  - Bar2D  (0)：二维水平柱状图
 *  - Pie2D  (1)：二维饼状图
 *  - Bar3D  (2)：伪三维柱状图（带顶面和侧面）
 *  - Pie3D  (3)：伪三维饼状图（椭圆顶面 + 面向观察者的侧壁）
 */
class ChartWidget : public QWidget
{
	Q_OBJECT

public:
	enum ChartType
	{
		Bar2D = 0,
		Pie2D = 1,
		Bar3D = 2,
		Pie3D = 3
	};
	Q_ENUM(ChartType)

	/** 扇形数据（供 tooltip 使用） */
	struct PieSlice
	{
		QString name;
		int     value      = 0;
		double  percentage = 0;
		QColor  color;
		double  startAngleDeg = 0;   // 起始角度（度，12 点钟=90）
		double  spanAngleDeg  = 0;   // 跨度角度（度）
	};

	explicit ChartWidget(QWidget* parent = nullptr);

	void setChartType(ChartType type);
	ChartType chartType() const { return mChartType; }

	/** 传入统计数据，widget 自动重绘 */
	void setData(const QList<DynamicStatsRow>& rows,
		const QStringList& metricCols,
		const QMap<QString, QString>& keyToName);

	/** 清空数据 */
	void clearData();

	/**
	 * @brief 通用伪 3D 饼图绘制（椭圆顶面 + 面向观察者的侧壁），供图表控件与报表导出复用
	 *
	 * - 顶面为纵向压缩的椭圆，配合向下拉伸的柱面侧壁，获得接近 Excel 三维饼图的立体感；
	 * - 侧壁只绘制前半圆（180°~360°）对应的柱面，并使用纵向渐变着色，
	 *   避免"多层圆盘堆叠"造成的横向条纹；
	 * - 顶面额外叠加玻璃高光与轮廓线，底部带一层柔和投影。
	 *
	 * @param p             画笔
	 * @param topCenter     顶面椭圆中心
	 * @param radius        顶面椭圆水平半径（像素）
	 * @param depth         侧壁厚度（像素）
	 * @param items         扇形数据（名称 + 数值），按绘制顺序排列
	 * @param colors        配色表
	 * @param outSlices     可选，输出扇形角度信息（供 tooltip 命中判定）
	 * @param verticalScale 顶面椭圆纵向压缩比，默认取 pie3DVerticalScale()
	 * @param drawGloss     是否绘制顶面玻璃高光。高光使用带 alpha 的 QRadialGradient，
	 *                      而 Qt 的 PDF 引擎（QPdfEngine）会丢弃渐变中的 alpha 并当作
	 *                      不透明填充，把整个顶面盖成白色（PDF 里饼图“空白”即由此产生），
	 *                      因此导出 PDF 时应传 false
	 */
	static void drawPie3D(QPainter& p,
		const QPoint& topCenter, int radius, int depth,
		const QVector<QPair<QString, int>>& items,
		const QList<QColor>& colors,
		QVector<PieSlice>* outSlices = nullptr,
		double verticalScale = -1.0,
		bool drawGloss = true);

	// ---- 颜色方案（供图表控件与报表导出共用，保证风格统一）----
	/** 饼图配色（暖色调） */
	static QList<QColor> pieColors();
	/** 柱状图配色 */
	static QList<QColor> barColors();

protected:
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;

private:
	// ---- 2D 渲染 ----
	void drawBarChart2D(QPainter& p, const QRect& area);
	void drawPieChart2D(QPainter& p, const QRect& area);

	// ---- 伪 3D 渲染 ----
	void drawBarChart3D(QPainter& p, const QRect& area);
	void drawPieChart3D(QPainter& p, const QRect& area);

	// ---- 辅助 ----
	int drawLegend(QPainter& p, const QRect& area, const QVector<QPair<QString, int>>& items,
		const QList<QColor>& colors, int total, double fontScale = 1.0);

	/** 计算规整的 Y 轴刻度值（nice numbers） */
	static QVector<qint64> calculateNiceYTicks(qint64 maxVal, int maxTicks = 4);
	/** 格式化刻度数值（大数自动加万/亿后缀） */
	static QString formatTickValue(qint64 val);

	ChartType                   mChartType = Bar2D;
	QList<DynamicStatsRow>      mRows;
	QStringList                 mMetricCols;
	QMap<QString, QString>      mKeyToName;

	// 饼图 hover tooltip
	QVector<PieSlice>  mPieSlices;
	int                mPieCenterX = 0;
	int                mPieCenterY = 0;
	int                mPieRadius  = 0;   // 顶面椭圆水平半径
	int                mPieRadiusY = 0;   // 顶面椭圆纵向半径
};

#endif // CHART_WIDGET_H
