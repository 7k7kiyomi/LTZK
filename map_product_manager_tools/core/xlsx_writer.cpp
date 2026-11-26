#include "xlsx_writer.h"
#include "zip_packer.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QDataStream>
#include <QtEndian>

// ==================== XlsxWriter 实现 ====================

void XlsxWriter::clear()
{
	m_sheets.clear();
	m_currentSheet = -1;
}

int XlsxWriter::addSheet(const QString& name)
{
	SheetData sd;
	sd.name = name;
	m_sheets.append(sd);
	m_currentSheet = m_sheets.size() - 1;
	return m_currentSheet;
}

void XlsxWriter::setSheetName(const QString& name)
{
	addSheet(name);
}

void XlsxWriter::setColumnWidths(const QVector<int>& widths)
{
	if (m_currentSheet >= 0 && m_currentSheet < m_sheets.size())
		m_sheets[m_currentSheet].columnWidths = widths;
}

void XlsxWriter::writeHeader(const QStringList& headers)
{
	if (m_currentSheet < 0) addSheet("Sheet1");
	auto& rows = m_sheets[m_currentSheet].rows;
	QVector<CellInfo> row;
	for (const auto& h : headers)
	{
		CellInfo cell;
		cell.value = h;
		cell.isHeader = true;
		row.append(cell);
	}
	rows.append(row);
}

void XlsxWriter::writeRow(const QStringList& values)
{
	if (m_currentSheet < 0) addSheet("Sheet1");
	auto& rows = m_sheets[m_currentSheet].rows;
	QVector<CellInfo> row;
	for (const auto& v : values)
	{
		CellInfo cell;
		cell.value = v;
		row.append(cell);
	}
	rows.append(row);
}

void XlsxWriter::mergeCells(int firstRow, int firstCol, int lastRow, int lastCol)
{
	if (m_currentSheet < 0) return;
	MergeInfo m;
	m.firstRow = firstRow;
	m.firstCol = firstCol;
	m.lastRow = lastRow;
	m.lastCol = lastCol;
	m_sheets[m_currentSheet].merges.append(m);
}

int XlsxWriter::rowCount() const
{
	if (m_currentSheet < 0 || m_currentSheet >= m_sheets.size()) return 0;
	return m_sheets[m_currentSheet].rows.size();
}

QString XlsxWriter::escapeXml(const QString& text)
{
	QString result = text;
	result.replace("&", "&amp;");
	result.replace("<", "&lt;");
	result.replace(">", "&gt;");
	result.replace("\"", "&quot;");
	result.replace("'", "&apos;");
	return result;
}

QString XlsxWriter::columnLetter(int col)
{
	QString result;
	while (col > 0)
	{
		col--;
		result.prepend(QChar('A' + (col % 26)));
		col /= 26;
	}
	return result;
}

QString XlsxWriter::buildSharedStringsXml()
{
	// 收集所有 sheet 中所有字符串
	QStringList strings;
	for (const auto& sheet : m_sheets)
	{
		for (const auto& row : sheet.rows)
		{
			for (const auto& cell : row)
			{
				if (!cell.value.isEmpty() && !strings.contains(cell.value))
					strings.append(cell.value);
			}
		}
	}

	QString xml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
	xml += "<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" count=\"" +
		   QString::number(strings.size()) + "\" uniqueCount=\"" + QString::number(strings.size()) + "\">\n";
	for (const auto& s : strings)
	{
		xml += "<si><t>" + escapeXml(s) + "</t></si>\n";
	}
	xml += "</sst>";
	return xml;
}

QString XlsxWriter::buildStylesXml()
{
	// 定义两种样式：普通单元格 + 表头（加粗居中黑色边框）
	QString xml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
	xml += "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n";

	// 字体：0=默认, 1=表头加粗
	xml += "<fonts count=\"2\">\n";
	xml += "<font><sz val=\"11\"/><name val=\"Microsoft YaHei\"/></font>\n";
	xml += "<font><b/><sz val=\"11\"/><name val=\"Microsoft YaHei\"/></font>\n";
	xml += "</fonts>\n";

	// 填充：0=none, 1=gray125（OOXML 规范要求至少2个）
	xml += "<fills count=\"2\">\n";
	xml += "<fill><patternFill patternType=\"none\"/></fill>\n";
	xml += "<fill><patternFill patternType=\"gray125\"/></fill>\n";
	xml += "</fills>\n";

	// 边框：0=无, 1=黑色 thin 四边（显式 RGB 黑色）
	xml += "<borders count=\"2\">\n";
	xml += "<border><left/><right/><top/><bottom/><diagonal/></border>\n";
	xml += "<border>"
		   "<left style=\"thin\"><color rgb=\"FF000000\"/></left>"
		   "<right style=\"thin\"><color rgb=\"FF000000\"/></right>"
		   "<top style=\"thin\"><color rgb=\"FF000000\"/></top>"
		   "<bottom style=\"thin\"><color rgb=\"FF000000\"/></bottom>"
		   "<diagonal/>"
		   "</border>\n";
	xml += "</borders>\n";

	// cellStyleXfs 至少需要1个（索引0指向 cellXfs[0]）
	xml += "<cellStyleXfs count=\"1\">"
		   "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/>"
		   "</cellStyleXfs>\n";

	// cellXfs：0=普通居中+边框, 1=表头加粗居中+边框
	xml += "<cellXfs count=\"2\">\n";
	// 索引 0：普通单元格 — 居中 + 黑色实线边框
	xml += "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" "
		   "applyAlignment=\"1\" applyBorder=\"1\">"
		   "<alignment horizontal=\"center\" vertical=\"center\" wrapText=\"0\"/>"
		   "</xf>\n";
	// 索引 1：表头 — 加粗 + 居中 + 黑色实线边框
	xml += "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"0\" borderId=\"1\" xfId=\"0\" "
		   "applyFont=\"1\" applyAlignment=\"1\" applyBorder=\"1\">"
		   "<alignment horizontal=\"center\" vertical=\"center\" wrapText=\"0\"/>"
		   "</xf>\n";
	xml += "</cellXfs>\n";

	xml += "</styleSheet>";
	return xml;
}

QString XlsxWriter::buildSheetXml(int sheetIdx)
{
	const auto& sheet = m_sheets[sheetIdx];
	const auto& rows = sheet.rows;
	const auto& merges = sheet.merges;
	const auto& widths = sheet.columnWidths;

	QString xml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
	xml += "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
		   "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n";

	// 列宽
	if (!widths.isEmpty())
	{
		xml += "<cols>\n";
		for (int i = 0; i < widths.size(); ++i)
		{
			xml += "<col min=\"" + QString::number(i + 1) + "\" max=\"" + QString::number(i + 1) +
				   "\" width=\"" + QString::number(widths[i]) + "\" customWidth=\"1\"/>\n";
		}
		xml += "</cols>\n";
	}

	// 数据行
	xml += "<sheetData>\n";
	for (int r = 0; r < rows.size(); ++r)
	{
		xml += "<row r=\"" + QString::number(r + 1) + "\">\n";
		for (int c = 0; c < rows[r].size(); ++c)
		{
			const auto& cell = rows[r][c];
			int styleIdx = cell.isHeader ? 1 : 0;

			QString cellRef = columnLetter(c + 1) + QString::number(r + 1);
			if (cell.value.isEmpty())
			{
				// 空单元格也输出，确保边框能显示
				xml += "<c r=\"" + cellRef + "\" s=\"" + QString::number(styleIdx) + "\"/>\n";
			}
			else
			{
				xml += "<c r=\"" + cellRef + "\" s=\"" + QString::number(styleIdx) + "\" t=\"inlineStr\">";
				xml += "<is><t>" + escapeXml(cell.value) + "</t></is></c>\n";
			}
		}
		xml += "</row>\n";
	}
	xml += "</sheetData>\n";

	// 合并单元格
	if (!merges.isEmpty())
	{
		xml += "<mergeCells count=\"" + QString::number(merges.size()) + "\">\n";
		for (const auto& m : merges)
		{
			QString ref = columnLetter(m.firstCol) + QString::number(m.firstRow) + ":" +
						  columnLetter(m.lastCol) + QString::number(m.lastRow);
			xml += "<mergeCell ref=\"" + ref + "\"/>\n";
		}
		xml += "</mergeCells>\n";
	}

	xml += "</worksheet>";
	return xml;
}

bool XlsxWriter::save(const QString& filePath)
{
	// 确保至少有一个 sheet
	if (m_sheets.isEmpty())
		addSheet("Sheet1");

	int sheetCount = m_sheets.size();

	ZipPacker zip;

	// [Content_Types].xml
	QString contentTypes = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
		"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
		"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
		"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n";
	for (int i = 0; i < sheetCount; ++i)
	{
		contentTypes += "<Override PartName=\"/xl/worksheets/sheet" + QString::number(i + 1) +
			".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n";
	}
	contentTypes += "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>\n"
		"<Override PartName=\"/xl/sharedStrings.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml\"/>\n"
		"</Types>";
	zip.addEntry("[Content_Types].xml", contentTypes.toUtf8());

	// _rels/.rels
	QString rels = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
		"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
		"</Relationships>";
	zip.addEntry("_rels/.rels", rels.toUtf8());

	// xl/workbook.xml
	QString workbook = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
		"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
		"<sheets>\n";
	for (int i = 0; i < sheetCount; ++i)
	{
		workbook += "<sheet name=\"" + escapeXml(m_sheets[i].name) + "\" sheetId=\"" +
			QString::number(i + 1) + "\" r:id=\"rId" + QString::number(i + 1) + "\"/>\n";
	}
	workbook += "</sheets>\n"
		"</workbook>";
	zip.addEntry("xl/workbook.xml", workbook.toUtf8());

	// xl/_rels/workbook.xml.rels
	QString wbRels = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n";
	for (int i = 0; i < sheetCount; ++i)
	{
		wbRels += "<Relationship Id=\"rId" + QString::number(i + 1) +
			"\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet" +
			QString::number(i + 1) + ".xml\"/>\n";
	}
	wbRels += "<Relationship Id=\"rId" + QString::number(sheetCount + 1) +
		"\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>\n"
		"<Relationship Id=\"rId" + QString::number(sheetCount + 2) +
		"\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings\" Target=\"sharedStrings.xml\"/>\n"
		"</Relationships>";
	zip.addEntry("xl/_rels/workbook.xml.rels", wbRels.toUtf8());

	// xl/worksheets/sheetN.xml
	for (int i = 0; i < sheetCount; ++i)
	{
		zip.addEntry("xl/worksheets/sheet" + QString::number(i + 1) + ".xml",
			buildSheetXml(i).toUtf8());
	}

	// xl/styles.xml
	zip.addEntry("xl/styles.xml", buildStylesXml().toUtf8());

	// xl/sharedStrings.xml
	zip.addEntry("xl/sharedStrings.xml", buildSharedStringsXml().toUtf8());

	return zip.save(filePath);
}
