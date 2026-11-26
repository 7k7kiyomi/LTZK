#include "xlsx_template_filler.h"
#include "zip_packer.h"

#include <QDomDocument>
#include <QDomElement>
#include <QRegularExpression>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

#include "cpl_vsi.h"
#include "cpl_string.h"

// 将 Excel 列索引（0 起）转换为列字母：0->A, 25->Z, 26->AA
static QString columnLetter(int col)
{
    QString s;
    int n = col + 1;
    while (n > 0)
    {
        int r = (n - 1) % 26;
        s.prepend(QChar('A' + r));
        n = (n - 1) / 26;
    }
    return s;
}

// 将单元格引用 "A3" 解析为列索引（0 起），返回 -1 表示非法
static int columnIndexOfRef(const QString& ref)
{
    int i = 0;
    while (i < ref.size() && ref[i].isLetter())
        ++i;
    if (i == 0) return -1;
    int col = 0;
    for (int j = 0; j < i; ++j)
        col = col * 26 + (ref[j].toUpper().unicode() - 'A' + 1);
    return col - 1;
}

// 将公式中的单元格行号统一替换为 newRow（保留列字母）
static QString replaceRowRefs(const QString& formula, int newRow)
{
    static const QRegularExpression re(QLatin1String("([A-Za-z]+)(\\d+)"));
    QString out;
    int last = 0;
    QRegularExpressionMatchIterator it = re.globalMatch(formula);
    while (it.hasNext())
    {
        QRegularExpressionMatch m = it.next();
        out += formula.mid(last, m.capturedStart() - last);
        out += m.captured(1) + QString::number(newRow);
        last = m.capturedEnd();
    }
    out += formula.mid(last);
    return out;
}

// 从公式 "SUM(I3:I4)" 提取首列字母，默认 "I"
static QString columnLetterOfFormula(const QString& formula, const QString& fallback)
{
    static const QRegularExpression re(QLatin1String("([A-Za-z]+)\\d+"));
    QRegularExpressionMatch m = re.match(formula);
    return m.hasMatch() ? m.captured(1).toUpper() : fallback;
}

bool XlsxTemplateFiller::load(const QString& templatePath)
{
    m_entries.clear();
    m_sheetXml.clear();
    m_lastError.clear();

    QString norm = QDir::fromNativeSeparators(templatePath);
    if (!readZipEntries(norm))
    {
        m_lastError = QStringLiteral("\u65E0\u6CD5\u8BFB\u53D6\u6A21\u677F: ") + templatePath;
        return false;
    }

    // 定位第一个 worksheet
    QString path = sheetXmlPath();
    if (path.isEmpty() || !m_entries.contains(path))
    {
        m_lastError = QStringLiteral("\u6A21\u677F\u4E2D\u672A\u627E\u5230\u5DE5\u4F5C\u8868");
        return false;
    }

    m_sheetXml = QString::fromUtf8(m_entries.value(path));
    if (m_sheetXml.isEmpty())
    {
        m_lastError = QStringLiteral("\u5DE5\u4F5C\u8868 XML \u4E3A\u7A7A");
        return false;
    }

    return true;
}

bool XlsxTemplateFiller::readZipEntries(const QString& zipPath)
{
    // 使用 GDAL /vsizip/ 虚拟文件系统读取 zip 内全部条目
    QString vsiRoot = QStringLiteral("/vsizip/") + zipPath;
    collectZipDir(vsiRoot, QString(), m_entries);
    return !m_entries.isEmpty();
}

void XlsxTemplateFiller::collectZipDir(const QString& vsiDir,
                                       const QString& prefix,
                                       QMap<QString, QByteArray>& out)
{
    char** items = VSIReadDir(vsiDir.toUtf8().constData());
    if (!items) return;

    for (int i = 0; items[i]; ++i)
    {
        QString name = QString::fromUtf8(items[i]);
        if (name == QStringLiteral(".") || name == QStringLiteral("..")) continue;

        QString fullPath = vsiDir + QStringLiteral("/") + name;
        QString relPath = prefix.isEmpty() ? name : prefix + QStringLiteral("/") + name;

        VSIStatBufL st;
        if (VSIStatL(fullPath.toUtf8().constData(), &st) == 0)
        {
            if (VSI_ISDIR(st.st_mode))
            {
                collectZipDir(fullPath, relPath, out);
            }
            else
            {
                VSILFILE* f = VSIFOpenL(fullPath.toUtf8().constData(), "rb");
                if (f)
                {
                    VSIFSeekL(f, 0, SEEK_END);
                    vsi_l_offset size = VSIFTellL(f);
                    VSIFSeekL(f, 0, SEEK_SET);
                    QByteArray data;
                    data.resize((int)size);
                    if (size > 0)
                        VSIFReadL(data.data(), 1, (size_t)size, f);
                    VSIFCloseL(f);
                    out[relPath] = data;
                }
            }
        }
    }
    CSLDestroy(items);
}

QString XlsxTemplateFiller::sheetXmlPath() const
{
    // 优先按常见约定：第一个 worksheet 为 xl/worksheets/sheet1.xml
    QStringList candidates;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it)
    {
        if (it.key().startsWith(QLatin1String("xl/worksheets/sheet"))
            && it.key().endsWith(QLatin1String(".xml")))
            candidates.append(it.key());
    }
    candidates.sort();
    return candidates.isEmpty() ? QString() : candidates.first();
}

bool XlsxTemplateFiller::fillData(int headerRows,
                                  const QVector<QStringList>& rows,
                                  const QString& summaryCell)
{
    return fillData(headerRows, rows, summaryCell, QStringList());
}

bool XlsxTemplateFiller::fillData(int headerRows,
                                  const QVector<QStringList>& rows,
                                  const QString& summaryCell,
                                  const QStringList& summaryValues)
{
    if (headerRows <= 0)
    {
        m_lastError = QStringLiteral("\u5934\u884C\u6570\u4E0D\u80FD\u4E3A\u96F6");
        return false;
    }
    return updateSheetXml(headerRows, rows, summaryCell, summaryValues);
}

bool XlsxTemplateFiller::updateSheetXml(int headerRows,
                                        const QVector<QStringList>& rows,
                                        const QString& summaryCell,
                                        const QStringList& summaryValues)
{
    QDomDocument doc;
    if (!doc.setContent(m_sheetXml))
    {
        m_lastError = QStringLiteral("\u89E3\u6790 worksheet XML \u5931\u8D25");
        return false;
    }

    QDomElement root = doc.documentElement();
    if (root.isNull())
    {
        m_lastError = QStringLiteral("worksheet \u6839\u8282\u70B9\u4E3A\u7A7A");
        return false;
    }

    // 命名空间（模板默认命名空间）
    QString ns = root.namespaceURI();

    QDomElement sheetData = root.firstChildElement(QStringLiteral("sheetData"));
    if (sheetData.isNull())
    {
        m_lastError = QStringLiteral("\u672A\u627E\u5230 sheetData");
        return false;
    }

    // 收集所有行元素
    QVector<QDomElement> rowEls;
    QDomNodeList kids = sheetData.childNodes();
    for (int i = 0; i < kids.size(); ++i)
    {
        QDomNode n = kids.item(i);
        if (n.isElement() && n.nodeName() == QLatin1String("row"))
            rowEls.append(n.toElement());
    }

    // 表头行保留
    QVector<QDomElement> headerEls;
    for (int i = 0; i < rowEls.size() && i < headerRows; ++i)
        headerEls.append(rowEls[i]);

    // 计算列数：取模板示例数据行列数 与 各数据行最大列数 的较大值
    int colCount = 0;
    for (const auto& row : rows)
        if (row.size() > colCount)
            colCount = row.size();
    if (rowEls.size() > headerRows)
    {
        const int sampleCols = rowEls[headerRows].childNodes().size();
        if (sampleCols > colCount)
            colCount = sampleCols;
    }
    if (colCount <= 0)
        colCount = 1;
    if (colCount > 40)
        colCount = 40; // 合理上限，防止异常模板导致列数失控

    // 从模板示例数据行提取每列样式与公式
    QVector<QString> dataStyles(colCount);
    QVector<QString> dataFormulas(colCount);
    if (rowEls.size() > headerRows)
    {
        QDomElement sample = rowEls[headerRows];
        QDomNodeList cells = sample.childNodes();
        for (int i = 0; i < cells.size(); ++i)
        {
            QDomNode n = cells.item(i);
            if (!n.isElement() || n.nodeName() != QLatin1String("c")) continue;
            QDomElement ce = n.toElement();
            int col = columnIndexOfRef(ce.attribute(QStringLiteral("r")));
            if (col < 0 || col >= colCount) continue;
            if (dataStyles[col].isEmpty())
                dataStyles[col] = ce.attribute(QStringLiteral("s"));
            QDomNodeList fNodes = ce.elementsByTagName(QStringLiteral("f"));
            if (!fNodes.isEmpty())
                dataFormulas[col] = fNodes.item(0).toElement().text().simplified();
        }
    }

    // 从模板合计行提取样式与公式
    QVector<QString> sumStyles(colCount);
    QString sumFormula;
    if (rowEls.size() > headerRows)
    {
        QDomElement last = rowEls.last();
        QDomNodeList cells = last.childNodes();
        for (int i = 0; i < cells.size(); ++i)
        {
            QDomNode n = cells.item(i);
            if (!n.isElement() || n.nodeName() != QLatin1String("c")) continue;
            QDomElement ce = n.toElement();
            int col = columnIndexOfRef(ce.attribute(QStringLiteral("r")));
            if (col < 0 || col >= colCount) continue;
            if (sumStyles[col].isEmpty())
                sumStyles[col] = ce.attribute(QStringLiteral("s"));
            QDomNodeList fNodes = ce.elementsByTagName(QStringLiteral("f"));
            if (!fNodes.isEmpty() && col == colCount - 1)
                sumFormula = fNodes.item(0).toElement().text().simplified();
        }
    }

    // 删除全部原数据行（示例数据与旧合计行）
    for (const auto& e : rowEls)
        sheetData.removeChild(e);

    // 重新插入表头行
    QDomElement lastRowEl;
    for (const auto& e : headerEls)
    {
        sheetData.appendChild(e);
        lastRowEl = e;
    }

    // 追加新数据行
    int startRow = headerRows + 1;
    for (int r = 0; r < rows.size(); ++r)
    {
        QDomElement rowEl = doc.createElementNS(ns, QStringLiteral("row"));
        rowEl.setAttribute(QStringLiteral("r"), startRow + r);
        rowEl.setAttribute(QStringLiteral("spans"), QStringLiteral("1:%1").arg(colCount));

        for (int col = 0; col < colCount; ++col)
        {
            QDomElement c = doc.createElementNS(ns, QStringLiteral("c"));
            c.setAttribute(QStringLiteral("r"), columnLetter(col) + QString::number(startRow + r));
            if (!dataStyles[col].isEmpty())
                c.setAttribute(QStringLiteral("s"), dataStyles[col]);

            QString val = (col < rows[r].size()) ? rows[r][col] : QString();
            if (!val.isEmpty())
            {
                // 数值单元格：尽量写为数字，保证 SUM 等公式可正常求和
                bool numeric = false;
                const double num = val.toDouble(&numeric);
                if (numeric)
                {
                    c.setAttribute(QStringLiteral("t"), QStringLiteral("n"));
                    QDomElement v = doc.createElementNS(ns, QStringLiteral("v"));
                    v.appendChild(doc.createTextNode(QString::number(num, 'g', 12)));
                    c.appendChild(v);
                }
                else
                {
                    c.setAttribute(QStringLiteral("t"), QStringLiteral("inlineStr"));
                    QDomElement is = doc.createElementNS(ns, QStringLiteral("is"));
                    QDomElement t = doc.createElementNS(ns, QStringLiteral("t"));
                    t.appendChild(doc.createTextNode(val));
                    is.appendChild(t);
                    c.appendChild(is);
                }
            }
            else if (!dataFormulas[col].isEmpty())
            {
                QDomElement f = doc.createElementNS(ns, QStringLiteral("f"));
                f.appendChild(doc.createTextNode(replaceRowRefs(dataFormulas[col], startRow + r)));
                c.appendChild(f);
            }

            rowEl.appendChild(c);
        }
        sheetData.appendChild(rowEl);
        lastRowEl = rowEl;
    }

    // 合计行
    int summaryRow = startRow + rows.size();
    if (!summaryCell.isEmpty())
    {
        QDomElement rowEl = doc.createElementNS(ns, QStringLiteral("row"));
        rowEl.setAttribute(QStringLiteral("r"), summaryRow);
        rowEl.setAttribute(QStringLiteral("spans"), QStringLiteral("1:%1").arg(colCount));

        for (int col = 0; col < colCount; ++col)
        {
            QDomElement c = doc.createElementNS(ns, QStringLiteral("c"));
            c.setAttribute(QStringLiteral("r"), columnLetter(col) + QString::number(summaryRow));
            if (!sumStyles[col].isEmpty())
                c.setAttribute(QStringLiteral("s"), sumStyles[col]);

            if (col == 0)
            {
                c.setAttribute(QStringLiteral("t"), QStringLiteral("inlineStr"));
                QDomElement is = doc.createElementNS(ns, QStringLiteral("is"));
                QDomElement t = doc.createElementNS(ns, QStringLiteral("t"));
                t.appendChild(doc.createTextNode(summaryCell));
                is.appendChild(t);
                c.appendChild(is);
            }
            else if (col < summaryValues.size() && !summaryValues[col].isEmpty())
            {
                // 显式给定的合计数值
                bool numeric = false;
                const double num = summaryValues[col].toDouble(&numeric);
                if (numeric)
                {
                    c.setAttribute(QStringLiteral("t"), QStringLiteral("n"));
                    QDomElement v = doc.createElementNS(ns, QStringLiteral("v"));
                    v.appendChild(doc.createTextNode(QString::number(num, 'g', 12)));
                    c.appendChild(v);
                }
                else
                {
                    c.setAttribute(QStringLiteral("t"), QStringLiteral("inlineStr"));
                    QDomElement is = doc.createElementNS(ns, QStringLiteral("is"));
                    QDomElement t = doc.createElementNS(ns, QStringLiteral("t"));
                    t.appendChild(doc.createTextNode(summaryValues[col]));
                    is.appendChild(t);
                    c.appendChild(is);
                }
            }
            else if (summaryValues.isEmpty() && col == colCount - 1
                     && !sumFormula.isEmpty() && rows.size() > 0)
            {
                QString colLetter = columnLetterOfFormula(sumFormula, QStringLiteral("I"));
                QString formula = QStringLiteral("SUM(%1%2:%1%3)")
                                  .arg(colLetter)
                                  .arg(startRow)
                                  .arg(startRow + rows.size() - 1);
                QDomElement f = doc.createElementNS(ns, QStringLiteral("f"));
                f.appendChild(doc.createTextNode(formula));
                c.appendChild(f);
            }
            rowEl.appendChild(c);
        }
        sheetData.appendChild(rowEl);

        // 更新合并单元格：仅保留表头合并（行号 <= headerRows），移除数据区旧合并，追加合计行合并
        QDomElement mergeCells = root.firstChildElement(QStringLiteral("mergeCells"));
        if (!mergeCells.isNull())
        {
            QDomNodeList mcs = mergeCells.childNodes();
            QVector<QDomElement> toRemove;
            for (int i = 0; i < mcs.size(); ++i)
            {
                QDomNode n = mcs.item(i);
                if (!n.isElement() || n.nodeName() != QLatin1String("mergeCell")) continue;
                QDomElement me = n.toElement();
                QString firstRef = me.attribute(QStringLiteral("ref")).section(QLatin1Char(':'), 0, 0);
                int j = 0;
                while (j < firstRef.size() && firstRef[j].isLetter()) ++j;
                int rowNum = 0;
                if (j < firstRef.size())
                    rowNum = firstRef.mid(j).toInt();
                if (rowNum > headerRows)
                    toRemove.append(me);
            }
            for (const auto& e : toRemove)
                mergeCells.removeChild(e);

            // 仅当模板本身含有合并单元格（如装裱清单模板的 A:C）时，
            // 才对合计行追加同样的合并；纯数据台账模板（无合并）则保持不合并。
            if (mergeCells.childNodes().size() > 0)
            {
                QDomElement newMc = doc.createElementNS(ns, QStringLiteral("mergeCell"));
                newMc.setAttribute(QStringLiteral("ref"),
                                   QStringLiteral("A%1:C%1").arg(summaryRow));
                mergeCells.appendChild(newMc);
                mergeCells.setAttribute(QStringLiteral("count"),
                                        QString::number(mergeCells.childNodes().size()));
            }
        }
    }

    // 更新 dimension 范围
    QDomElement dim = root.firstChildElement(QStringLiteral("dimension"));
    if (!dim.isNull())
    {
        int lastRow = summaryCell.isEmpty() ? startRow + rows.size() - 1 : summaryRow;
        if (lastRow < headerRows) lastRow = headerRows;
        dim.setAttribute(QStringLiteral("ref"),
                         QStringLiteral("A1:%1%2").arg(columnLetter(colCount - 1)).arg(lastRow));
    }

    m_sheetXml = doc.toString();
    return true;
}

bool XlsxTemplateFiller::save(const QString& outPath)
{
    if (m_sheetXml.isEmpty())
    {
        m_lastError = QStringLiteral("\u6CA1\u6709\u53EF\u4FDD\u5B58\u7684\u5185\u5BB9");
        return false;
    }

    // 找到 sheet 路径并替换内容
    QString path = sheetXmlPath();
    if (path.isEmpty())
    {
        m_lastError = QStringLiteral("\u672A\u627E\u5230\u5DE5\u4F5C\u8868\u8DEF\u5F84");
        return false;
    }
    m_entries[path] = m_sheetXml.toUtf8();

    ZipPacker packer;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it)
        packer.addEntry(it.key(), it.value());

    QDir().mkpath(QFileInfo(outPath).absolutePath());
    if (!packer.save(QDir::toNativeSeparators(outPath)))
    {
        // 附带具体原因（文件被占用/目录不可写等），便于用户处理
        m_lastError = QStringLiteral("\u4FDD\u5B58 xlsx \u5931\u8D25: ") + outPath;
        if (!packer.lastError().isEmpty())
            m_lastError += QStringLiteral("\n") + packer.lastError();
        return false;
    }
    return true;
}
