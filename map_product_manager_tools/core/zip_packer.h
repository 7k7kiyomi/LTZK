#ifndef ZIP_PACKER_H
#define ZIP_PACKER_H

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QObject>
#include <QVector>
#include <QString>
#include <QByteArray>
#include <QDataStream>
#include <QtEndian>

// 本地文件头标志
static const quint32 ZIP_LOCAL_FILE_HEADER_SIG = 0x04034b50;
static const quint32 ZIP_CENTRAL_DIR_HEADER_SIG = 0x02014b50;
static const quint32 ZIP_END_OF_CENTRAL_DIR_SIG = 0x06054b50;

namespace xlsxzip
{

// CRC32 表
static quint32 crc32_table[256];
static bool crc32_table_initialized = false;

static void initCrc32Table()
{
	if (crc32_table_initialized) return;
	for (quint32 i = 0; i < 256; ++i)
	{
		quint32 crc = i;
		for (int j = 0; j < 8; ++j)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
		crc32_table[i] = crc;
	}
	crc32_table_initialized = true;
}

static quint32 crc32(const QByteArray& data)
{
	initCrc32Table();
	quint32 crc = 0xFFFFFFFF;
	for (int i = 0; i < data.size(); ++i)
		crc = crc32_table[(crc ^ (quint8)data[i]) & 0xFF] ^ (crc >> 8);
	return crc ^ 0xFFFFFFFF;
}

} // namespace xlsxzip

/**
 * @brief 极简 ZIP 打包器（纯 Qt 实现，STORE 不压缩）
 *
 * 与 Open XML (xlsx) 兼容：Excel/WPS 均可正常打开。
 */
class ZipPacker
{
public:
	void addEntry(const QString& name, const QByteArray& content)
	{
		Entry e;
		e.name = name;
		e.content = content;
		m_entries.append(e);
	}

	// 保存失败时返回具体原因（占用/只读/目录不可写等），无失败时为 QString()
	QString lastError() const { return m_lastError; }

	bool save(const QString& filePath)
	{
		QFile file(filePath);
		if (!file.open(QIODevice::WriteOnly))
		{
#if defined(Q_OS_WIN)
			QFileInfo fi(filePath);
			if (fi.exists())
			{
				if (fi.isWritable())
				{
					// 文件本身可写却打不开 => 正被其他进程（Excel/WPS）独占占用
					m_lastError = QObject::tr("\u6587\u4EF6\u6B63\u88AB\u5176\u4ED6\u7A0B\u5E8F\u5360\u7528\uFF08\u8BF7\u5148\u5173\u95ED Excel/WPS \u4E2D\u6253\u5F00\u7684\u8BE5\u6587\u4EF6\uFF09: ") + QDir::toNativeSeparators(filePath);
				}
				else
				{
					m_lastError = QObject::tr("\u6587\u4EF6\u4E3A\u53EA\u8BFB\u5C5E\u6027\uFF0C\u8BF7\u53D6\u6D88\u53EA\u8BFB\u540E\u91CD\u8BD5: ") + QDir::toNativeSeparators(filePath);
				}
			}
			else
			{
				m_lastError = QObject::tr("\u76EE\u6807\u76EE\u5F55\u4E0D\u53EF\u5199\u6216\u8DEF\u5F84\u65E0\u6548: ") + QDir::toNativeSeparators(filePath);
			}
#else
			m_lastError = QObject::tr("\u65E0\u6CD5\u6253\u5F00\u6587\u4EF6\u5199\u5165: ") + QDir::toNativeSeparators(filePath);
#endif
			return false;
		}

		QVector<WrittenEntry> written;

		for (const auto& entry : m_entries)
		{
			QByteArray nameBytes = entry.name.toUtf8();
			quint32 crcVal = xlsxzip::crc32(entry.content);

			// 使用 STORE（不压缩）避免 qCompress 兼容性问题
			QByteArray data = entry.content;
			quint32 compSize = data.size();
			quint32 uncompSize = data.size();
			quint16 method = 0;  // STORE, no compression

			quint64 localOffset = file.pos();

			QByteArray localHeader;
			QDataStream ls(&localHeader, QIODevice::WriteOnly);
			ls.setByteOrder(QDataStream::LittleEndian);
			ls << ZIP_LOCAL_FILE_HEADER_SIG;
			ls << quint16(20);
			ls << quint16(0);
			ls << method;
			ls << quint16(0);  // time
			ls << quint16(0);  // date
			ls << crcVal;
			ls << compSize;
			ls << uncompSize;
			ls << quint16(nameBytes.size());
			ls << quint16(0);
			file.write(localHeader);
			file.write(nameBytes);
			file.write(data);

			WrittenEntry we;
			we.name = entry.name;
			we.crc = crcVal;
			we.compSize = compSize;
			we.uncompSize = uncompSize;
			we.method = method;
			we.localOffset = localOffset;
			written.append(we);
		}

		// 中央目录
		quint64 centralDirOffset = file.pos();
		quint64 centralDirSize = 0;

		for (const auto& we : written)
		{
			QByteArray nameBytes = we.name.toUtf8();
			QByteArray cdHeader;
			QDataStream cs(&cdHeader, QIODevice::WriteOnly);
			cs.setByteOrder(QDataStream::LittleEndian);
			cs << ZIP_CENTRAL_DIR_HEADER_SIG;
			cs << quint16(20);               // version made by
			cs << quint16(20);               // version needed
			cs << quint16(0);                // flags
			cs << we.method;
			cs << quint16(0);                // time
			cs << quint16(0);                // date
			cs << we.crc;
			cs << we.compSize;
			cs << we.uncompSize;
			cs << quint16(nameBytes.size());
			cs << quint16(0);                // extra
			cs << quint16(0);                // comment
			cs << quint16(0);                // disk start
			cs << quint16(0);                // internal attrs
			cs << quint32(0);                // external attrs
			cs << quint32(we.localOffset);
			file.write(cdHeader);
			file.write(nameBytes);
			centralDirSize += cdHeader.size() + nameBytes.size();
		}

		// EOCD
		QByteArray eocd;
		QDataStream es(&eocd, QIODevice::WriteOnly);
		es.setByteOrder(QDataStream::LittleEndian);
		es << ZIP_END_OF_CENTRAL_DIR_SIG;
		es << quint16(0);                    // disk number
		es << quint16(0);                    // disk with CD
		es << quint16(written.size());       // entries on disk
		es << quint16(written.size());       // total entries
		es << quint32(centralDirSize);
		es << quint32(centralDirOffset);
		es << quint16(0);                    // comment length
		file.write(eocd);

		file.close();
		return true;
	}

private:
	struct Entry {
		QString name;
		QByteArray content;
	};
	struct WrittenEntry {
		QString name;
		quint32 crc;
		quint32 compSize;
		quint32 uncompSize;
		quint16 method;
		quint64 localOffset;
	};
	QVector<Entry> m_entries;
	QString m_lastError;
};

#endif // ZIP_PACKER_H
