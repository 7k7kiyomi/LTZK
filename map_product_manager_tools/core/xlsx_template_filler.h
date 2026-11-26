#ifndef XLSX_TEMPLATE_FILLER_H
#define XLSX_TEMPLATE_FILLER_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>

/**
 * @brief 基于已有 .xlsx 模板填充数据的工具
 *
 * 读取模板（例如装裱清单模板 zbqd.xlsx），保留模板的表头、样式、
 * 合并单元格、公式等格式，只替换 sheet 中的数据行与合计行，
 * 然后重新打包保存。
 *
 * 模板要求（与本项目装裱清单模板一致）：
 *   - 第一个 worksheet
 *   - 前两行为两行表头（含合并单元格）
 *   - 数据行从第 3 行开始
 *   - 末行为合计行（可缺省，缺省时自动创建）
 */
class XlsxTemplateFiller
{
public:
    XlsxTemplateFiller() = default;

    // 加载模板 xlsx（读入全部内部条目）。失败返回 false。
    bool load(const QString& templatePath);

    // 填入数据行。headerRows 为表头行数（默认 2）。
    // rows 为数据行（每行为一行的各列文本，缺列自动补空）。
    // summaryCell 为合计行首列文字（如 "合  计"），若为空则忽略合计行。
    // 成功返回 true。
    bool fillData(int headerRows,
                  const QVector<QStringList>& rows,
                  const QString& summaryCell);

    // 重载：额外指定合计行的各列数值（summaryValues），
    // 会将各数值写入合计行的对应列（列0仍为 summaryCell 文字）。
    // summaryValues 为空时退化为旧行为（仅末列生成 SUM 公式）。
    bool fillData(int headerRows,
                  const QVector<QStringList>& rows,
                  const QString& summaryCell,
                  const QStringList& summaryValues);

    // 保存到目标路径（重新打包为 xlsx）。
    bool save(const QString& outPath);

    // 最近一次错误信息
    QString lastError() const { return m_lastError; }

private:
    bool readZipEntries(const QString& zipPath);
    void collectZipDir(const QString& vsiDir,
                       const QString& prefix,
                       QMap<QString, QByteArray>& out);
    QString sheetXmlPath() const;
    bool updateSheetXml(int headerRows,
                        const QVector<QStringList>& rows,
                        const QString& summaryCell,
                        const QStringList& summaryValues);

    QMap<QString, QByteArray> m_entries; // zip 内路径 -> 内容
    QString m_sheetXml;                  // 第一个 worksheet 的 XML
    QString m_lastError;
};

#endif // XLSX_TEMPLATE_FILLER_H
