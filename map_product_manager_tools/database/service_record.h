#ifndef SERVICE_RECORD_H
#define SERVICE_RECORD_H

#include <QString>
#include <QDateTime>
#include <QMap>
#include <QStringList>
#include <QSettings>
#include <QVariant>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// ============================================================================
// 枚举定义
// ============================================================================

/**
 * @brief 服务单状态
 */
enum class ServiceStatus
{
	Pending = 0,    // 处理中
	Completed = 1,  // 已完成
	Cancelled = 2,  // 已取消
	Archived = 3    // 已归档
};

/**
 * @brief 图件种类（地图编制Tab用）
 */
namespace CompilationMapKind
{
	static const QStringList& all()
	{
		static const QStringList types = {
			QStringLiteral("单张图"),
			QStringLiteral("图集图册"),
			QStringLiteral("政务图包"),
			QStringLiteral("制图数据"),
			QStringLiteral("矢量数据"),
			QStringLiteral("其他")
		};
		return types;
	}
}

/**
 * @brief 各「服务形式」分页对应的图件（数据）种类选项
 *
 * 各服务形式可提供的种类不同，页面按服务形式分别展示：
 *  - 地图编制：单张图、图集图册、其他
 *  - 地图打印：单张图、其他
 *  - 地图装裱：单张图、其他
 *  - 数据发送：矢量数据、制图数据、其他
 *  - 地图成品：单张图、图集图册、政务图包、创意地图（日历/丝巾）、其他
 *  - 其他    ：保持通用种类列表
 */
namespace ServiceFormMapKind
{
	/// 该种类是否允许在「名称」列填写内容（图集图册名称、创意地图类型、其他自定义名称）
	inline bool allowCustomName(const QString& kind)
	{
		return kind == QStringLiteral("图集图册")
			|| kind == QStringLiteral("创意地图")
			|| kind == QStringLiteral("其他");
	}

	/// 「名称」列的填写提示（无特殊要求时返回空）
	inline QString nameHint(const QString& kind)
	{
		if (kind == QStringLiteral("创意地图"))
			return QStringLiteral("可填写：日历、丝巾");
		if (kind == QStringLiteral("图集图册"))
			return QStringLiteral("填写图集图册名称");
		if (kind == QStringLiteral("其他"))
			return QStringLiteral("填写自定义名称");
		return QString();
	}

	/// 服务形式 → 该形式下的图件（数据）种类列表
	inline const QStringList& forServiceForm(const QString& serviceForm)
	{
		static const QStringList compileKinds = {
			QStringLiteral("单张图"),
			QStringLiteral("图集图册"),
			QStringLiteral("其他")
		};
		static const QStringList printKinds = {
			QStringLiteral("单张图"),
			QStringLiteral("其他")
		};
		static const QStringList mountingKinds = {
			QStringLiteral("单张图"),
			QStringLiteral("其他")
		};
		static const QStringList dataKinds = {
			QStringLiteral("矢量数据"),
			QStringLiteral("制图数据"),
			QStringLiteral("其他")
		};
		static const QStringList productKinds = {
			QStringLiteral("单张图"),
			QStringLiteral("图集图册"),
			QStringLiteral("政务图包"),
			QStringLiteral("创意地图"),
			QStringLiteral("其他")
		};

		if (serviceForm == QStringLiteral("地图编制")) return compileKinds;
		if (serviceForm == QStringLiteral("地图打印")) return printKinds;
		if (serviceForm == QStringLiteral("地图装裱")) return mountingKinds;
		if (serviceForm == QStringLiteral("数据发送")) return dataKinds;
		if (serviceForm == QStringLiteral("地图成品")) return productKinds;
		return CompilationMapKind::all();
	}

	/// 依据种类列表生成「名称」列表头，如「名称（图集图册/其他）」
	inline QString nameColumnHeader(const QStringList& kinds)
	{
		QStringList names;
		for (const QString& kind : kinds)
		{
			if (allowCustomName(kind) && !names.contains(kind))
				names.append(kind);
		}
		if (names.isEmpty())
			return QStringLiteral("名称");
		return QStringLiteral("名称（%1）").arg(names.join(QStringLiteral("/")));
	}
}

// 工具函数：类别选项固定值列表
inline QStringList categoryOptions()
{
	static const QStringList options = {
		QStringLiteral("110：省委秘书一处"),
		QStringLiteral("100：省委其他单位"),
		QStringLiteral("200：省政府其他单位"),
		QStringLiteral("210：省政府秘书一处"),
		QStringLiteral("300：省人大"),
		QStringLiteral("400：省政协"),
		QStringLiteral("500：省上其他单位"),
		QStringLiteral("600：省自然资源厅"),
		QStringLiteral("610：厅下属单位"),
		QStringLiteral("660：市州自然资源局"),
		QStringLiteral("888：其他")
	};
	return options;
}

// 工具函数：应用场景选项固定值列表
inline QStringList applicationScenarioOptions()
{
	static const QStringList options = {
		QStringLiteral("日常政务服务"),
		QStringLiteral("政务调研用图"),
		QStringLiteral("应急地图服务")
	};
	return options;
}

// 工具函数：服务等级选项固定值列表
inline QStringList serviceLevelOptions()
{
	static const QStringList options = {
		QStringLiteral("国家级"),
		QStringLiteral("省级"),
		QStringLiteral("厅级"),
		QStringLiteral("其他政府单位")
	};
	return options;
}

// 工具函数：材质选项固定值列表（地图打印/装裱/其他通用）
inline QStringList materialOptions()
{
	static const QStringList options = {
		QStringLiteral("普通纸"),
		QStringLiteral("相纸"),
		QStringLiteral("布艺"),
		QStringLiteral("书法装裱绫布"),
		QStringLiteral("32×60北美黄杨木"),
		QStringLiteral("23×35柏木"),
		QStringLiteral("25×40柏木")
	};
	return options;
}

// 工具函数：装裱尺寸（图框规格）选项固定值列表
inline QStringList mountingSizeOptions()
{
	static const QStringList options = {
		QStringLiteral("86厘米×355厘米"),
		QStringLiteral("116厘米×166厘米"),
		QStringLiteral("152厘米×210厘米")
	};
	return options;
}

// 工具函数：服务形式选项固定值列表
inline QStringList serviceFormOptions()
{
	static const QStringList options = {
		QStringLiteral("地图编制"),
		QStringLiteral("地图打印"),
		QStringLiteral("地图装裱"),
		QStringLiteral("地图成品"),
		QStringLiteral("数据发送"),
		QStringLiteral("其他")
	};
	return options;
}

// 工具函数：状态字符串转换
inline QString serviceStatusToString(ServiceStatus status)
{
	switch (status)
	{
	case ServiceStatus::Pending:   return QStringLiteral("处理中");
	case ServiceStatus::Completed: return QStringLiteral("已完成");
	case ServiceStatus::Cancelled: return QStringLiteral("已取消");
	case ServiceStatus::Archived:  return QStringLiteral("已归档");
	default: return QStringLiteral("未知");
	}
}

inline ServiceStatus stringToServiceStatus(const QString& str)
{
	if (str == QStringLiteral("已完成")) return ServiceStatus::Completed;
	if (str == QStringLiteral("已取消")) return ServiceStatus::Cancelled;
	if (str == QStringLiteral("已归档")) return ServiceStatus::Archived;
	return ServiceStatus::Pending;
}

// ============================================================================
// 地图编制详情 — 图件种类+数量条目
// ============================================================================

struct CompilationItem
{
	QString kind;      // 图件种类：单张图/图集图册/政务图包/制图数据/矢量数据/其他
	QString name;      // 图集图册名称、其他自定义名称等
	int quantity = 0;  // 数量

	QJsonObject toJson() const
	{
		QJsonObject obj;
		obj["kind"] = kind;
		obj["name"] = name;
		obj["quantity"] = quantity;
		return obj;
	}

	static CompilationItem fromJson(const QJsonObject& obj)
	{
		CompilationItem item;
		item.kind = obj["kind"].toString();
		item.name = obj["name"].toString();
		item.quantity = obj["quantity"].toInt();
		return item;
	}
};

/// 将编制详情列表转为 JSON 字符串，用于数据库存储
inline QString compilationItemsToJson(const QList<CompilationItem>& items)
{
	QJsonArray arr;
	for (const auto& item : items)
		arr.append(item.toJson());
	return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

/// 从 JSON 字符串解析编制详情列表
inline QList<CompilationItem> compilationItemsFromJson(const QString& json)
{
	QList<CompilationItem> items;
	QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
	if (!doc.isArray()) return items;
	for (const auto& val : doc.array())
	{
		if (val.isObject())
			items.append(CompilationItem::fromJson(val.toObject()));
	}
	return items;
}

// ============================================================================
// 各服务形式Tab的通用详情结构
// ============================================================================

struct ServiceFormDetail
{
	QString mapName;               // 图名
	QString mappingRegion;         // 制图区域
	QString size;                  // 尺寸
	QString material;              // 材质
	QList<CompilationItem> items;  // 图件种类+数量列表

	bool isEmpty() const {
		return mapName.isEmpty() && mappingRegion.isEmpty() && size.isEmpty()
			&& material.isEmpty() && items.isEmpty();
	}

	QJsonObject toJson() const
	{
		QJsonObject obj;
		obj["mapName"] = mapName;
		obj["mappingRegion"] = mappingRegion;
		obj["size"] = size;
		obj["material"] = material;
		QJsonArray arr;
		for (const auto& item : items)
			arr.append(item.toJson());
		obj["items"] = arr;
		return obj;
	}

	static ServiceFormDetail fromJson(const QJsonObject& obj)
	{
		ServiceFormDetail d;
		d.mapName = obj["mapName"].toString();
		d.mappingRegion = obj["mappingRegion"].toString();
		d.size = obj["size"].toString();
		d.material = obj["material"].toString();
		QJsonArray arr = obj["items"].toArray();
		for (const auto& val : arr)
		{
			if (val.isObject())
				d.items.append(CompilationItem::fromJson(val.toObject()));
		}
		return d;
	}
};

/// 将 ServiceFormDetail 序列化为 JSON 字符串
inline QString serviceFormDetailToJson(const ServiceFormDetail& detail)
{
	return QString::fromUtf8(QJsonDocument(detail.toJson()).toJson(QJsonDocument::Compact));
}

/// 从 JSON 字符串反序列化 ServiceFormDetail
inline ServiceFormDetail serviceFormDetailFromJson(const QString& json)
{
	if (json.isEmpty()) return ServiceFormDetail();
	QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
	if (!doc.isObject()) return ServiceFormDetail();
	return ServiceFormDetail::fromJson(doc.object());
}

// ============================================================================
// 多记录支持：每种服务形式可有多条 ServiceFormDetail 记录
// 数据库存储为 JSON 数组；同时兼容旧的单对象格式
// ============================================================================

/// 将多条 ServiceFormDetail 序列化为 JSON 数组字符串
inline QString serviceFormDetailsToJson(const QList<ServiceFormDetail>& details)
{
	QJsonArray arr;
	for (const auto& d : details)
		arr.append(d.toJson());
	return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

/// 从 JSON 字符串反序列化多条 ServiceFormDetail。
/// 兼容三种存储格式：
///  - 数组（新格式）：每元素是一个 ServiceFormDetail
///  - 单对象（旧格式）：整体当作一条 ServiceFormDetail
///  - 空串：空列表
inline QList<ServiceFormDetail> serviceFormDetailsFromJson(const QString& json)
{
	QList<ServiceFormDetail> details;
	if (json.isEmpty()) return details;
	QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
	if (doc.isArray())
	{
		for (const auto& val : doc.array())
		{
			if (val.isObject())
				details.append(ServiceFormDetail::fromJson(val.toObject()));
		}
	}
	else if (doc.isObject())
	{
		details.append(ServiceFormDetail::fromJson(doc.object()));
	}
	return details;
}

// ============================================================================
// 地图编制旧格式兼容：CompilationItem 数组（无 mapName 键） → 单条 ServiceFormDetail
// ============================================================================

/// 将地图编制的 JSON 反序列化为 ServiceFormDetail 列表。
/// 兼容：
///  - 新格式 ServiceFormDetail 数组 / 单对象 → 直接返回
///  - 旧格式 CompilationItem 数组（元素含 "kind"/"quantity"，无 "mapName"/"items"）→
///    包裹成单条 ServiceFormDetail，items 为这些图件种类
inline QList<ServiceFormDetail> compilationDetailsFromJson(const QString& json)
{
	if (json.isEmpty()) return QList<ServiceFormDetail>();
	QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());

	// 单对象：新格式的某一条 ServiceFormDetail
	if (doc.isObject())
		return { ServiceFormDetail::fromJson(doc.object()) };

	if (doc.isArray())
	{
		const QJsonArray arr = doc.array();
		// 空数组
		if (arr.isEmpty())
			return QList<ServiceFormDetail>();

		// 判断是否旧格式 CompilationItem 数组（元素含 "kind" 且无 "mapName"）
		bool isOldCompile = true;
		for (const auto& val : arr)
		{
			if (!val.isObject()) { isOldCompile = false; break; }
			QJsonObject o = val.toObject();
			if (o.contains("mapName") || o.contains("items"))
			{
				isOldCompile = false;
				break;
			}
		}

		if (isOldCompile)
		{
			ServiceFormDetail d;
			for (const auto& val : arr)
			{
				if (val.isObject())
					d.items.append(CompilationItem::fromJson(val.toObject()));
			}
			return { d };
		}

		// 新格式 ServiceFormDetail 数组
		QList<ServiceFormDetail> details;
		for (const auto& val : arr)
		{
			if (val.isObject())
				details.append(ServiceFormDetail::fromJson(val.toObject()));
		}
		return details;
	}

	return QList<ServiceFormDetail>();
}

// ============================================================================
// 地图服务单数据结构
// ============================================================================

struct ServiceRecord
{
	int id = 0;
	QString serviceNumber;         // 序号（服务单号，自动生成）

	// 服务基本信息
	QString recordDate;            // 日期（服务接收时间）
	QString submitTime;            // 服务提交时间
	QString applicationScenario;   // 应用场景：日常政务服务/政务调研用图/应急地图服务
	QString coverageArea;          // 覆盖区域
	QString serviceContent;        // 服务内容

	// 服务数量（保留，部分兼容旧用图统计）
	int sheetCount = 0;            // 服务数量

	// 服务单位
	int unitId = 0;
	QString unitName;              // 服务单位

	// 分类与归档
	QString category;              // 服务类别
	QString serviceForm;           // 服务形式
	QString serviceLevel;          // 服务等级：国家级/省级/厅级/其他政府单位
	QString arrangedOffice;        // 安排处室（处室+安排领导）
	QString otherServiceForm;      // 当服务形式选"其他"时的自定义内容

	// 地图编制Tab — 基本信息（兼容旧数据，新数据存入 compilationDetails）
	QString mapName;               // 图名
	QString mappingRegion;         // 制图区域
	QString size;                  // 尺寸
	QString material;              // 材质（装裱用）

	// 地图编制Tab — 图件种类+数量（JSON存储）
	// 新格式为 ServiceFormDetail 数组：[{"mapName":"","mappingRegion":"","size":"","material":"","items":[...]}, ...]
	// 兼容旧格式 CompilationItem 数组：[{"kind":"单张图","name":"","quantity":3}, ...]
	QString compilationDetails;

	// 各服务形式Tab的详细数据（JSON存储，均为 ServiceFormDetail 数组）
	QString printDetails;          // 地图打印Tab详情
	QString mountingDetails;       // 地图装裱Tab详情
	QString productDetails;        // 地图成品Tab详情
	QString dataDetails;           // 数据发送Tab详情
	QString otherDetails;          // 其他Tab详情

	// 接收人
	QString receiver;              // 接收人姓名及电话

	ServiceStatus status = ServiceStatus::Pending;
	double funding = 0.0;          // 经费（元）— 保留数据库字段兼容，UI不再显示

	// 关联地图成果
	QStringList productIds;        // 关联成果ID列表
	QStringList productNames;      // 关联成果名称列表
	QString productIdsStr;         // 逗号分隔ID串（数据库存储）
	QString productNamesStr;       // 逗号分隔名称串（数据库存储）

	// 备注与归档
	QString remarks;               // 备注
	QString archivePath;           // 电子版归档路径
	QString archiveFormat;         // 归档格式
	QString approvalFilePath;      // 电子审批单文件路径
	int approvalFileOid = 0;       // 电子审批单 BLOB OID（支持跨机器访问）

	// 审计信息
	QString createdBy;
	QDateTime createdAt;
	QString updatedBy;
	QDateTime updatedAt;
};

// ============================================================================
// 搜索条件
// ============================================================================

struct SearchCondition
{
	QString keyword;
	QString unitName;
	QString category;              // 类别筛选
	QString serviceLevel;          // 服务等级筛选
	QString dateFrom;
	QString dateTo;
	QString status;
	int offset = 0;
	int limit = 20;
};

// ============================================================================
// 统计数据
// ============================================================================

struct ServiceStatistics
{
	int totalRecords = 0;                     // 服务单总数
	int totalSheetCount = 0;                  // 图幅总数
	int totalUnitCount = 0;                   // 服务单位数

	QMap<QString, int> byMapType;             // 按图件种类统计（数量求和，非记录数）
	QMap<QString, int> byUnit;                // 按服务单位统计
	QMap<QString, int> byLevel;               // 按服务等级统计
	QMap<QString, int> byArea;                // 按覆盖区域统计
	QMap<QString, int> byCategory;            // 按类别统计
	QMap<QString, int> bySvcForm;             // 按服务形式统计
	QMap<QString, int> monthlyFrequency;      // 服务频次（月/季/年）
	QString frequencyGranularity = QStringLiteral("month");

	QString dateFrom;
	QString dateTo;
};

// ============================================================================
// 动态统计 - 灵活列配置
// ============================================================================

/**
 * @brief 预定义统计指标（可灵活启用/禁用）
 */
namespace StatMetrics
{
	// 指标键名常量
	const QString KEY_SHEET_COUNT        = QStringLiteral("sheet_count");          // 用图数量（图幅数）
	const QString KEY_RECORD_COUNT       = QStringLiteral("record_count");         // 服务单数量
	const QString KEY_FUNDING            = QStringLiteral("funding");              // 经费

	// 服务形式（service_form 文本字段 ILIKE 匹配，共6种）
	const QString KEY_SVCFORM_COMPILE    = QStringLiteral("svcform_compile");      // 地图编制
	const QString KEY_SVCFORM_PRINT      = QStringLiteral("svcform_print");        // 地图打印
	const QString KEY_SVCFORM_MOUNTING   = QStringLiteral("svcform_mounting");     // 地图装裱
	const QString KEY_SVCFORM_PRODUCT    = QStringLiteral("svcform_product");      // 地图成品
	const QString KEY_SVCFORM_DATA       = QStringLiteral("svcform_data");         // 数据发送
	const QString KEY_SVCFORM_OTHER      = QStringLiteral("svcform_other");        // 其他

	// 覆盖区域（coverage_area 非空记录数）
	const QString KEY_COVERAGE_AREA      = QStringLiteral("covarea_count");        // 有覆盖区域记录数

	// 图件种类（compilation_details JSON 中按 kind 字段统计记录数，共6种）
	const QString KEY_MAPKIND_SHEET      = QStringLiteral("mapkind_single");       // 单张图
	const QString KEY_MAPKIND_ATLAS      = QStringLiteral("mapkind_atlas");        // 图集图册
	const QString KEY_MAPKIND_GOV        = QStringLiteral("mapkind_gov");          // 政务图包
	const QString KEY_MAPKIND_CARTO      = QStringLiteral("mapkind_carto");        // 制图数据
	const QString KEY_MAPKIND_VECTOR     = QStringLiteral("mapkind_vector");       // 矢量数据
	const QString KEY_MAPKIND_CREATIVE   = QStringLiteral("mapkind_creative");     // 创意地图（日历/丝巾）
	const QString KEY_MAPKIND_OTHER      = QStringLiteral("mapkind_other");        // 其他

	struct MetricDef
	{
		QString key;          // 数据库列名或计算表达式键
		QString displayName;  // 显示名称
		QString category;     // 分类：「服务统计」「用图统计」「服务形式」「经费」
		bool isCount;         // 是否为计数类(SUM)，否则为唯一计数(COUNT DISTINCT)
	};

	inline QList<MetricDef> allMetrics()
	{
		static const QList<MetricDef> metrics = {
			{ KEY_RECORD_COUNT,   QStringLiteral("服务单数量"),     QStringLiteral("服务统计"), false },
			{ KEY_SVCFORM_COMPILE,  QStringLiteral("地图编制"),     QStringLiteral("服务形式"), true  },
			{ KEY_SVCFORM_PRINT,    QStringLiteral("地图打印"),     QStringLiteral("服务形式"), true  },
			{ KEY_SVCFORM_MOUNTING, QStringLiteral("地图装裱"),     QStringLiteral("服务形式"), true  },
			{ KEY_SVCFORM_PRODUCT,  QStringLiteral("地图成品"),     QStringLiteral("服务形式"), true  },
			{ KEY_SVCFORM_DATA,     QStringLiteral("数据发送"),     QStringLiteral("服务形式"), true  },
			{ KEY_SVCFORM_OTHER,    QStringLiteral("其他"),         QStringLiteral("服务形式"), true  },
			{ KEY_COVERAGE_AREA,  QStringLiteral("有覆盖区域记录数"), QStringLiteral("覆盖区域"), true  },
		{ KEY_MAPKIND_SHEET,  QStringLiteral("单张图"),        QStringLiteral("用图统计"), true  },
		{ KEY_MAPKIND_ATLAS,  QStringLiteral("图集图册"),      QStringLiteral("用图统计"), true  },
		{ KEY_MAPKIND_GOV,    QStringLiteral("政务图包"),      QStringLiteral("用图统计"), true  },
		{ KEY_MAPKIND_CARTO,  QStringLiteral("制图数据"),      QStringLiteral("用图统计"), true  },
		{ KEY_MAPKIND_VECTOR, QStringLiteral("矢量数据"),      QStringLiteral("用图统计"), true  },
		{ KEY_MAPKIND_CREATIVE, QStringLiteral("创意地图"),    QStringLiteral("用图统计"), true  },
		{ KEY_MAPKIND_OTHER,  QStringLiteral("其他"),          QStringLiteral("用图统计"), true  },
			{ KEY_FUNDING,        QStringLiteral("经费(元)"),       QStringLiteral("经费"),   true  }
		};
		return metrics;
	}

	/// 特殊指标键 → ILIKE 匹配配置：{ 字段名, 匹配模式 }
	inline QPair<QString, QString> specialMetricPattern(const QString& key)
	{
		if (key.startsWith(QStringLiteral("svcform_")))
		{
			if (key == KEY_SVCFORM_COMPILE)  return { QStringLiteral("service_form"), QStringLiteral("地图编制") };
			if (key == KEY_SVCFORM_PRINT)    return { QStringLiteral("service_form"), QStringLiteral("地图打印") };
			if (key == KEY_SVCFORM_MOUNTING) return { QStringLiteral("service_form"), QStringLiteral("地图装裱") };
			if (key == KEY_SVCFORM_PRODUCT)  return { QStringLiteral("service_form"), QStringLiteral("地图成品") };
			if (key == KEY_SVCFORM_DATA)     return { QStringLiteral("service_form"), QStringLiteral("数据发送") };
			if (key == KEY_SVCFORM_OTHER)    return { QStringLiteral("service_form"), QStringLiteral("其他") };
		}
		else if (key.startsWith(QStringLiteral("mapkind_")))
		{
			// compilation_details JSON 中按 kind 值统计
			if (key == KEY_MAPKIND_SHEET)  return { QStringLiteral("compilation_details"), QStringLiteral("\"kind\": \"单张图\"") };
			if (key == KEY_MAPKIND_ATLAS)  return { QStringLiteral("compilation_details"), QStringLiteral("\"kind\": \"图集图册\"") };
			if (key == KEY_MAPKIND_GOV)    return { QStringLiteral("compilation_details"), QStringLiteral("\"kind\": \"政务图包\"") };
			if (key == KEY_MAPKIND_CARTO)  return { QStringLiteral("compilation_details"), QStringLiteral("\"kind\": \"制图数据\"") };
			if (key == KEY_MAPKIND_VECTOR) return { QStringLiteral("compilation_details"), QStringLiteral("\"kind\": \"矢量数据\"") };
			if (key == KEY_MAPKIND_CREATIVE) return { QStringLiteral("compilation_details"), QStringLiteral("\"kind\": \"创意地图\"") };
			if (key == KEY_MAPKIND_OTHER)  return { QStringLiteral("compilation_details"), QStringLiteral("\"kind\": \"其他\"") };
		}
		else if (key == KEY_COVERAGE_AREA)
		{
			return { QStringLiteral("coverage_area"), QString() };  // 非空计数
		}
		return {};
	}

	/// 判断是否为需要特殊处理的指标
	inline bool isSpecialMetric(const QString& key)
	{
		return key.startsWith(QStringLiteral("svcform_"))
			|| key.startsWith(QStringLiteral("mapkind_"))
			|| key == KEY_COVERAGE_AREA;
	}

	/// 服务形式指标键 → 对应的 JSON 列名
	/// compilation_details 是纯数组，其余 5 个是对象（含 items 数组）
	inline QString svcformJsonColumn(const QString& key)
	{
		if (key == KEY_SVCFORM_COMPILE)  return QStringLiteral("compilation_details");
		if (key == KEY_SVCFORM_PRINT)    return QStringLiteral("print_details");
		if (key == KEY_SVCFORM_MOUNTING) return QStringLiteral("mounting_details");
		if (key == KEY_SVCFORM_PRODUCT)  return QStringLiteral("product_details");
		if (key == KEY_SVCFORM_DATA)     return QStringLiteral("data_details");
		if (key == KEY_SVCFORM_OTHER)    return QStringLiteral("other_details");
		return QString();
	}
}

/**
 * @brief 图件种类可配置项
 */
struct MapCategoryItem
{
	QString name;
	QString dbColumn;
	bool enabled = true;
};

/**
 * @brief 动态统计一行数据（灵活列）
 */
struct DynamicStatsRow
{
	QString timePeriod;
	QMap<QString, QVariant> metrics;
};

/**
 * @brief 动态统计配置
 */
struct DynamicStatisticsConfig
{
	QString granularity = QStringLiteral("year");
	QStringList enabledColumns;
	QList<MapCategoryItem> mapCategories;
	QString dateFrom;
	QString dateTo;
};

// ============================================================================
// 地图装裱统计报表行（用于导出Excel）
// ============================================================================

struct MountingStatsRow
{
	int seq = 0;                    // 序号
	QString submitTime;             // 服务提交时间
	QString unitName;               // 服务部门（服务单位）
	QString frameSpec;              // 图框规格（米）← 从 size
	QString material;               // 材质
	int totalQuantity = 0;          // 数量（各图件种类的求和）
	double unitPrice = 0.0;         // 单价（元）— 不预填
	double amount = 0.0;            // 金额（元）— 不预填
	double subtotal = 0.0;          // 小计（元）— 不预填
};

/**
 * @brief 图件种类持久化管理
 */
namespace MapCategoryManager
{
	inline QString settingsKey() { return QStringLiteral("MapProductTools/CustomMapCategories"); }

	inline QList<MapCategoryItem> load()
	{
		QSettings settings;
		QList<MapCategoryItem> items;
		int size = settings.beginReadArray(settingsKey());
		for (int i = 0; i < size; ++i)
		{
			settings.setArrayIndex(i);
			MapCategoryItem item;
			item.name = settings.value("name").toString();
			item.dbColumn = settings.value("dbColumn").toString();
			item.enabled = settings.value("enabled", true).toBool();
			items.append(item);
		}
		settings.endArray();
		return items;
	}

	inline void save(const QList<MapCategoryItem>& items)
	{
		QSettings settings;
		settings.beginWriteArray(settingsKey());
		for (int i = 0; i < items.size(); ++i)
		{
			settings.setArrayIndex(i);
			settings.setValue("name", items[i].name);
			settings.setValue("dbColumn", items[i].dbColumn);
			settings.setValue("enabled", items[i].enabled);
		}
		settings.endArray();
	}
}

#endif // SERVICE_RECORD_H
