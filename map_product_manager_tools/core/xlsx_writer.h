#ifndef XLSX_WRITER_H
#define XLSX_WRITER_H

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief 轻量级 XLSX 文件写入器（基于 OOXML 格式，无外部依赖）
 *
 * 直接生成符合 Office Open XML 规范的 .xlsx 文件（本质是 ZIP 包内含 XML），
 * 不需要任何第三方库。支持单元格合并、简单样式和多 sheet 页。
 */
class XlsxWriter
{
public:
	XlsxWriter() = default;

	// 添加一个工作表并返回其索引（自动切换为当前工作表）
	int addSheet(const QString& name);

	// 设置当前工作表名称（单 sheet 兼容接口，等价于 addSheet）
	void setSheetName(const QString& name);

	// 设置列宽（所有列统一宽度，单位：字符数，作用于当前工作表）
	void setColumnWidths(const QVector<int>& widths);

	// 写入表头行
	void writeHeader(const QStringList& headers);

	// 写入数据行
	void writeRow(const QStringList& values);

	// 合并单元格（例如合并标题行），坐标从 1 开始
	void mergeCells(int firstRow, int firstCol, int lastRow, int lastCol);

	// 获取当前行数（用于计算 mergeCells 的行号）
	int rowCount() const;

	// 保存为 .xlsx 文件
	bool save(const QString& filePath);

	// 清空当前数据
	void clear();

private:
	QString buildSheetXml(int sheetIdx);
	QString buildStylesXml();
	QString buildSharedStringsXml();
	QString escapeXml(const QString& text);
	QString columnLetter(int col);

	struct CellInfo
	{
		QString value;
		bool isHeader = false;
	};

	struct MergeInfo
	{
		int firstRow, firstCol, lastRow, lastCol;
	};

	struct SheetData
	{
		QString name;
		QVector<QVector<CellInfo>> rows;
		QVector<MergeInfo> merges;
		QVector<int> columnWidths;
	};

	QVector<SheetData> m_sheets;
	int m_currentSheet = -1;
};

#endif // XLSX_WRITER_H
