const fs = require("fs");
const {
	Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
	HeadingLevel, BorderStyle, WidthType, ShadingType, AlignmentType,
	LevelFormat, PageBreak
} = require("docx");

// ============ 通用样式 ============
const border = { style: BorderStyle.SINGLE, size: 1, color: "CCCCCC" };
const borders = { top: border, bottom: border, left: border, right: border };
const cellMargins = { top: 80, bottom: 80, left: 120, right: 120 };

const codeFont = "Consolas";
const bodyFont = "Microsoft YaHei";

function codeRun(text) {
	return new TextRun({ text, font: codeFont, size: 20, color: "2d3436" });
}

function codeBold(text) {
	return new TextRun({ text, font: codeFont, size: 20, bold: true, color: "2d3436" });
}

function bodyRun(text) {
	return new TextRun({ text, font: bodyFont, size: 22 });
}

function bodyBold(text) {
	return new TextRun({ text, font: bodyFont, size: 22, bold: true });
}

function headerCell(text, width) {
	return new TableCell({
		borders,
		width: { size: width, type: WidthType.DXA },
		shading: { fill: "1a73e8", type: ShadingType.CLEAR },
		margins: cellMargins,
		children: [new Paragraph({
			alignment: AlignmentType.CENTER,
			children: [new TextRun({ text, font: bodyFont, size: 22, bold: true, color: "FFFFFF" })]
		})]
	});
}

function dataCell(text, width, isCode = false) {
	return new TableCell({
		borders,
		width: { size: width, type: WidthType.DXA },
		margins: cellMargins,
		children: [new Paragraph({
			children: [new TextRun({ text, font: isCode ? codeFont : bodyFont, size: 20, color: isCode ? "2d3436" : "333333" })]
		})]
	});
}

// ============ 代码块 ============
function codeBlock(lines) {
	return lines.map((line, i) => {
		const runs = [new TextRun({ text: line, font: codeFont, size: 20, color: "2d3436" })];
		return new Paragraph({
			spacing: { before: i === 0 ? 120 : 0, after: 0, line: 276 },
			shading: { fill: "f5f6fa", type: ShadingType.CLEAR },
			indent: { left: 360, right: 360 },
			children: runs
		});
	});
}

// ============ 提示/警告块 ============
function tipBox(title, content, color) {
	const emoji = color === "d63031" ? "\u26A0" : "\u2705";
	return [
		new Paragraph({
			spacing: { before: 200, after: 80 },
			children: [
				new TextRun({ text: emoji + " " + title, font: bodyFont, size: 22, bold: true, color })
			]
		}),
		...content.map(t => new Paragraph({
			spacing: { before: 40, after: 40, line: 320 },
			shading: { fill: color === "d63031" ? "fff5f5" : "f0fff4", type: ShadingType.CLEAR },
			indent: { left: 360, right: 360 },
			children: [new TextRun({ text: t, font: bodyFont, size: 20, color: "333333" })]
		}))
	];
}

// ============ 步骤标题 ============
function stepTitle(num, title) {
	return new Paragraph({
		heading: HeadingLevel.HEADING_2,
		spacing: { before: 360, after: 160 },
		children: [
			new TextRun({ text: `步骤 ${num}：${title}`, font: bodyFont, size: 28, bold: true, color: "1a73e8" })
		]
	});
}

// ============ 文档内容 ============
const children = [];

// 标题页
children.push(
	new Paragraph({
		spacing: { before: 2400 },
		alignment: AlignmentType.CENTER,
		children: [new TextRun({ text: "PostgreSQL \u5B89\u88C5\u90E8\u7F72\u4E0E\u5C40\u57DF\u7F51\u8BBF\u95EE\u914D\u7F6E\u6307\u5357", font: bodyFont, size: 44, bold: true, color: "1a73e8" })]
	}),
	new Paragraph({
		spacing: { before: 200, after: 200 },
		alignment: AlignmentType.CENTER,
		children: [new TextRun({ text: "\u2014\u2014 \u8BA9\u5C40\u57DF\u7F51\u5185\u5176\u4ED6\u7528\u6237\u8BBF\u95EE PostgreSQL \u6570\u636E\u5E93", font: bodyFont, size: 26, color: "5f6368" })]
	}),
	new Paragraph({ children: [new PageBreak()] })
);

// 目录
children.push(
	new Paragraph({
		heading: HeadingLevel.HEADING_1,
		children: [new TextRun({ text: "\u76EE\u5F55", font: bodyFont, size: 32, bold: true, color: "2c3e50" })]
	}),
	new Paragraph({ spacing: { after: 100 }, children: [bodyRun("1. \u524D\u63D0\u6761\u4EF6\u4E0E\u73AF\u5883\u786E\u8BA4")] }),
	new Paragraph({ spacing: { after: 100 }, children: [bodyRun("2. \u4FEE\u6539 YOUR_PASSWORDql.conf \u2014\u2014 \u76D1\u542C\u5730\u5740")] }),
	new Paragraph({ spacing: { after: 100 }, children: [bodyRun("3. \u4FEE\u6539 pg_hba.conf \u2014\u2014 \u8BBF\u95EE\u63A7\u5236")] }),
	new Paragraph({ spacing: { after: 100 }, children: [bodyRun("4. \u9632\u706B\u5899\u914D\u7F6E \u2014\u2014 \u5F00\u653E 5432 \u7AEF\u53E3")] }),
	new Paragraph({ spacing: { after: 100 }, children: [bodyRun("5. \u91CD\u542F PostgreSQL \u670D\u52A1")] }),
	new Paragraph({ spacing: { after: 100 }, children: [bodyRun("6. \u9A8C\u8BC1\u914D\u7F6E\u662F\u5426\u6B63\u786E")] }),
	new Paragraph({ spacing: { after: 100 }, children: [bodyRun("7. \u5E38\u89C1\u95EE\u9898\u6392\u67E5")] }),
	new Paragraph({ spacing: { after: 200 }, children: [bodyRun("8. \u5B89\u5168\u5EFA\u8BAE")] }),
	new Paragraph({ children: [new PageBreak()] })
);

// ============ 步骤 1：前提条件 ============
children.push(
	new Paragraph({
		heading: HeadingLevel.HEADING_1,
		children: [new TextRun({ text: "\u6B65\u9AA4 1\uFF1A\u524D\u63D0\u6761\u4EF6\u4E0E\u73AF\u5883\u786E\u8BA4", font: bodyFont, size: 32, bold: true, color: "2c3e50" })]
	}),
	new Paragraph({
		spacing: { after: 120, line: 360 },
		children: [bodyRun("\u5728\u5F00\u59CB\u914D\u7F6E\u4E4B\u524D\uFF0C\u786E\u8BA4\u4EE5\u4E0B\u4FE1\u606F\uFF1A")]
	}),
	...tipBox("\u786E\u8BA4\u6E05\u5355", [
		"1. PostgreSQL \u5DF2\u6210\u529F\u5B89\u88C5\u5E76\u80FD\u672C\u5730\u8BBF\u95EE\uFF08psql -U YOUR_PASSWORD\uFF09",
		"2. \u8BB0\u5F55\u5B89\u88C5\u8DEF\u5F84\uFF08\u9ED8\u8BA4\uFF1AC:\\Program Files\\PostgreSQL\\<\u7248\u672C>\\data\\\uFF09",
		"3. \u786E\u8BA4 PostgreSQL \u7248\u672C\u53F7\uFF1A\u5728 psql \u4E2D\u6267\u884C SELECT version();",
		"4. \u786E\u8BA4\u670D\u52A1\u5668\u672C\u673A IP \u5730\u5740\uFF1A\u5728\u547D\u4EE4\u884C\u6267\u884C ipconfig",
		"5. \u786E\u8BA4\u6709\u7BA1\u7406\u5458\u6743\u9650\uFF08\u80FD\u4FEE\u6539\u914D\u7F6E\u6587\u4EF6\u3001\u91CD\u542F\u670D\u52A1\uFF09"
	], "00b894")
);

// ============ 步骤 2：YOUR_PASSWORDql.conf ============
children.push(
	stepTitle("2", "\u4FEE\u6539 YOUR_PASSWORDql.conf \u2014\u2014 \u76D1\u542C\u5730\u5740"),
	new Paragraph({
		spacing: { after: 80, line: 360 },
		children: [bodyRun("\u8FD9\u662F\u6700\u6838\u5FC3\u7684\u914D\u7F6E\uFF0C\u9ED8\u8BA4 PostgreSQL \u53EA\u76D1\u542C localhost\uFF0C\u5FC5\u987B\u8BA9\u5B83\u76D1\u542C\u6240\u6709\u7F51\u7EDC\u63A5\u53E3\u3002")]
	})
);

// 2.1 找到文件
children.push(
	new Paragraph({
		spacing: { before: 160, after: 80 },
		children: [bodyBold("2.1 \u627E\u5230\u914D\u7F6E\u6587\u4EF6")]
	}),
	new Paragraph({
		spacing: { after: 80, line: 320 },
		children: [bodyRun("\u914D\u7F6E\u6587\u4EF6\u901A\u5E38\u4F4D\u4E8E PostgreSQL \u5B89\u88C5\u76EE\u5F55\u4E0B\u7684 data \u6587\u4EF6\u5939\u4E2D\uFF1A")]
	}),
	...codeBlock([
		"C:\\Program Files\\PostgreSQL\\16\\data\\YOUR_PASSWORDql.conf",
		"",
		"# \u6216\u8005\u5728 pgAdmin \u4E2D\u67E5\u770B\uFF1A",
		"SHOW config_file;",
	])
);

// 2.2 修改
children.push(
	new Paragraph({
		spacing: { before: 200, after: 80 },
		children: [bodyBold("2.2 \u4FEE\u6539 listen_addresses")]
	}),
	new Paragraph({
		spacing: { after: 80, line: 320 },
		children: [bodyRun("\u7528\u6587\u672C\u7F16\u8F91\u5668\uFF08\u5982 Notepad++\u3001VS Code\uFF09\u6253\u5F00 YOUR_PASSWORDql.conf\uFF0C\u627E\u5230\u4EE5\u4E0B\u884C\uFF1A")]
	}),
	...codeBlock([
		"#listen_addresses = 'localhost'         # what IP address(es) to listen on;",
	]),
	new Paragraph({
		spacing: { before: 120, after: 80, line: 320 },
		children: [bodyRun("\u4FEE\u6539\u4E3A\uFF08\u53BB\u6389\u524D\u9762\u7684 # \u6CE8\u91CA\u7B26\uFF09\uFF1A")]
	}),
	...codeBlock([
		"listen_addresses = '*'                  # \u76D1\u542C\u6240\u6709\u7F51\u7EDC\u63A5\u53E3",
	]),
	...tipBox("\u8BF4\u660E", [
		"'*' \u8868\u793A\u76D1\u542C\u6240\u6709\u7F51\u7EDC\u63A5\u53E3\uFF0C\u5305\u62EC localhost \u548C\u6240\u6709\u7F51\u5361 IP",
		"\u4E5F\u53EF\u4EE5\u6307\u5B9A\u5177\u4F53 IP\uFF0C\u4F8B\u5982 listen_addresses = '192.168.1.100'",
		"\u4FDD\u5B58\u6587\u4EF6\u540E\u6682\u4E0D\u91CD\u542F\uFF0C\u7B49\u6240\u6709\u914D\u7F6E\u5B8C\u6210\u540E\u518D\u91CD\u542F"
	], "0984e3")
);

// ============ 步骤 3：pg_hba.conf ============
children.push(
	stepTitle("3", "\u4FEE\u6539 pg_hba.conf \u2014\u2014 \u8BBF\u95EE\u63A7\u5236"),
	new Paragraph({
		spacing: { after: 80, line: 360 },
		children: [bodyRun("\u5373\u4F7F listen_addresses \u5DF2\u7ECF\u8BBE\u4E3A '*'\uFF0CPostgreSQL \u8FD8\u4F1A\u901A\u8FC7 pg_hba.conf \u63A7\u5236\u54EA\u4E9B\u7528\u6237\u53EF\u4EE5\u4ECE\u54EA\u4E9B\u5730\u5740\u8BBF\u95EE\u3002\u9ED8\u8BA4\u53EA\u5141\u8BB8\u672C\u5730\u8BBF\u95EE\u3002")]
	})
);

children.push(
	new Paragraph({
		spacing: { before: 160, after: 80 },
		children: [bodyBold("3.1 \u627E\u5230\u914D\u7F6E\u6587\u4EF6")]
	}),
	...codeBlock([
		"C:\\Program Files\\PostgreSQL\\16\\data\\pg_hba.conf",
	])
);

children.push(
	new Paragraph({
		spacing: { before: 200, after: 80 },
		children: [bodyBold("3.2 \u6DFB\u52A0\u5C40\u57DF\u7F51\u8BBF\u95EE\u89C4\u5219")]
	}),
	new Paragraph({
		spacing: { after: 80, line: 320 },
		children: [bodyRun("\u5728\u6587\u4EF6\u672B\u5C3E\u7684 IPv4 local connections \u90E8\u5206\uFF0C\u6DFB\u52A0\u4EE5\u4E0B\u884C\uFF1A")]
	})
);

// pg_hba.conf 规则表格
const colW = [1800, 1400, 1400, 2400, 2360];
children.push(
	new Table({
		width: { size: 9360, type: WidthType.DXA },
		columnWidths: colW,
		rows: [
			new TableRow({ children: [headerCell("TYPE", colW[0]), headerCell("DATABASE", colW[1]), headerCell("USER", colW[2]), headerCell("ADDRESS", colW[3]), headerCell("METHOD", colW[4])] }),
			new TableRow({ children: [dataCell("host", colW[0], true), dataCell("all", colW[1], true), dataCell("all", colW[2], true), dataCell("192.168.1.0/24", colW[3], true), dataCell("scram-sha-256", colW[4], true)] }),
			new TableRow({ children: [dataCell("host", colW[0], true), dataCell("all", colW[1], true), dataCell("all", colW[2], true), dataCell("10.0.0.0/8", colW[3], true), dataCell("scram-sha-256", colW[4], true)] }),
		]
	}),
	new Paragraph({ spacing: { before: 120, after: 80 }, children: [bodyRun("")] })
);

children.push(
	...tipBox("\u89C4\u5219\u8BF4\u660E", [
		"192.168.1.0/24 = \u5141\u8BB8 192.168.1.1 ~ 192.168.1.254 \u8BBF\u95EE",
		"10.0.0.0/8 = \u5141\u8BB8 10.x.x.x \u6574\u4E2A\u7F51\u6BB5\u8BBF\u95EE",
		"scram-sha-256 = \u4F7F\u7528\u5B89\u5168\u7684\u5BC6\u7801\u9A8C\u8BC1\uFF08PostgreSQL 10+ \u63A8\u8350\uFF09",
		"\u5982\u679C\u60F3\u5F00\u653E\u6240\u6709\u7F51\u6BB5\uFF0C\u53EF\u7528 0.0.0.0/0\uFF08\u4E0D\u63A8\u8350\u751F\u4EA7\u73AF\u5883\uFF09",
		"\u89C4\u5219\u662F\u4ECE\u4E0A\u5F80\u4E0B\u5339\u914D\u7684\uFF0C\u8BF7\u786E\u4FDD\u65B0\u89C4\u5219\u653E\u5728\u8F83\u9760\u4E0A\u7684\u4F4D\u7F6E"
	], "0984e3")
);

// ============ 步骤 4：防火墙 ============
children.push(
	stepTitle("4", "\u9632\u706B\u5899\u914D\u7F6E \u2014\u2014 \u5F00\u653E 5432 \u7AEF\u53E3"),
	new Paragraph({
		spacing: { after: 120, line: 360 },
		children: [bodyRun("\u5373\u4F7F PostgreSQL \u914D\u7F6E\u6B63\u786E\uFF0CWindows \u9632\u706B\u5899\u4E5F\u4F1A\u963B\u6B62\u5916\u90E8\u8BBF\u95EE\u3002\u5FC5\u987B\u6DFB\u52A0\u5165\u7AD9\u89C4\u5219\u5F00\u653E 5432 \u7AEF\u53E3\u3002")]
	})
);

children.push(
	new Paragraph({
		spacing: { before: 160, after: 80 },
		children: [bodyBold("4.1 \u901A\u8FC7\u56FE\u5F62\u754C\u9762\u8BBE\u7F6E\uFF08\u63A8\u8350\uFF09")]
	}),
	new Paragraph({ spacing: { after: 60 }, children: [bodyRun("\u2460 \u6253\u5F00\u201C\u63A7\u5236\u9762\u677F\u201D\u2192\u201CWindows Defender \u9632\u706B\u5899\u201D")] }),
	new Paragraph({ spacing: { after: 60 }, children: [bodyRun("\u2461 \u70B9\u51FB\u5DE6\u4FA7\u201C\u9AD8\u7EA7\u8BBE\u7F6E\u201D")] }),
	new Paragraph({ spacing: { after: 60 }, children: [bodyRun("\u2462 \u70B9\u51FB\u201C\u5165\u7AD9\u89C4\u5219\u201D\u2192\u201C\u65B0\u5EFA\u89C4\u5219\u201D")] }),
	new Paragraph({ spacing: { after: 60 }, children: [bodyRun("\u2463 \u9009\u62E9\u201C\u7AEF\u53E3\u201D\u2192\u201CTCP\u201D\u2192\u8F93\u5165\u201C5432\u201D")] }),
	new Paragraph({ spacing: { after: 60 }, children: [bodyRun("\u2464 \u9009\u62E9\u201C\u5141\u8BB8\u8FDE\u63A5\u201D\u2192\u786E\u8BA4\u89C4\u5219\u540D\u79F0\u4E3A PostgreSQL")] }),
	new Paragraph({ spacing: { after: 160 }, children: [bodyRun("\u2465 \u70B9\u51FB\u201C\u5B8C\u6210\u201D")] })
);

children.push(
	new Paragraph({
		spacing: { before: 160, after: 80 },
		children: [bodyBold("4.2 \u901A\u8FC7\u547D\u4EE4\u884C\u8BBE\u7F6E\uFF08\u4EE5\u7BA1\u7406\u5458\u8FD0\u884C\uFF09")]
	}),
	...codeBlock([
		"netsh advfirewall firewall add rule name=\"PostgreSQL\"",
		"    dir=in action=allow protocol=TCP localport=5432",
	]),
	...tipBox("\u9A8C\u8BC1\u9632\u706B\u5899\u89C4\u5219", [
		"\u6267\u884C\u4EE5\u4E0B\u547D\u4EE4\u67E5\u770B\u89C4\u5219\u662F\u5426\u6DFB\u52A0\u6210\u529F\uFF1A",
		"netsh advfirewall firewall show rule name=\"PostgreSQL\""
	], "00b894")
);

// ============ 步骤 5：重启服务 ============
children.push(
	stepTitle("5", "\u91CD\u542F PostgreSQL \u670D\u52A1"),
	new Paragraph({
		spacing: { after: 120, line: 360 },
		children: [bodyRun("\u6240\u6709\u914D\u7F6E\u6587\u4EF6\u4FEE\u6539\u5B8C\u6210\u540E\uFF0C\u5FC5\u987B\u91CD\u542F\u670D\u52A1\u624D\u80FD\u751F\u6548\u3002")]
	})
);

children.push(
	new Paragraph({
		spacing: { before: 160, after: 80 },
		children: [bodyBold("5.1 \u901A\u8FC7\u670D\u52A1\u7BA1\u7406\u5668\u91CD\u542F\uFF08\u63A8\u8350\uFF09")]
	}),
	new Paragraph({ spacing: { after: 60 }, children: [bodyRun("\u2460 \u6309 Win+R\uFF0C\u8F93\u5165 services.msc \u6253\u5F00\u670D\u52A1\u7BA1\u7406\u5668")] }),
	new Paragraph({ spacing: { after: 60 }, children: [bodyRun("\u2461 \u627E\u5230\u201CYOUR_PASSWORDql-x64-16\u201D\uFF08\u5177\u4F53\u540D\u79F0\u53D6\u51B3\u4E8E\u5B89\u88C5\u7248\u672C\uFF09")] }),
	new Paragraph({ spacing: { after: 60 }, children: [bodyRun("\u2462 \u53F3\u952E\u70B9\u51FB\u2192\u201C\u91CD\u65B0\u542F\u52A8\u201D")] }),
	new Paragraph({ spacing: { after: 160 }, children: [bodyRun("\u2463 \u786E\u8BA4\u72B6\u6001\u4E3A\u201C\u6B63\u5728\u8FD0\u884C\u201D")] })
);

children.push(
	new Paragraph({
		spacing: { before: 160, after: 80 },
		children: [bodyBold("5.2 \u901A\u8FC7\u547D\u4EE4\u884C\u91CD\u542F\uFF08\u4EE5\u7BA1\u7406\u5458\u8FD0\u884C\uFF09")]
	}),
	...codeBlock([
		"net stop YOUR_PASSWORDql-x64-16",
		"net start YOUR_PASSWORDql-x64-16",
	]),
	...tipBox("\u6CE8\u610F", [
		"\u670D\u52A1\u540D\u79F0\u53EF\u80FD\u56E0\u7248\u672C\u4E0D\u540C\u800C\u5DEE\u5F02\uFF0C\u53EF\u4EE5\u7528\u4EE5\u4E0B\u547D\u4EE4\u67E5\u770B\u7CBE\u786E\u540D\u79F0\uFF1A",
		"sc query state= all | findstr /i YOUR_PASSWORD"
	], "0984e3")
);

// ============ 步骤 6：验证 ============
children.push(
	stepTitle("6", "\u9A8C\u8BC1\u914D\u7F6E\u662F\u5426\u6B63\u786E"),
	new Paragraph({
		spacing: { after: 120, line: 360 },
		children: [bodyRun("\u914D\u7F6E\u5B8C\u6210\u540E\uFF0C\u53EF\u4EE5\u4ECE\u672C\u673A\u548C\u5176\u4ED6\u5C40\u57DF\u7F51\u7535\u8111\u8FDB\u884C\u9A8C\u8BC1\u3002")]
	})
);

children.push(
	new Paragraph({
		spacing: { before: 160, after: 80 },
		children: [bodyBold("6.1 \u5728\u670D\u52A1\u5668\u672C\u673A\u9A8C\u8BC1")]
	}),
	...tipBox("\u9A8C\u8BC1\u65B9\u6CD5", [
		"\u2460 \u68C0\u67E5\u76D1\u542C\u72B6\u6001\uFF1Anetstat -an | findstr 5432",
		"   \u671F\u671B\u7ED3\u679C\uFF1ATCP    0.0.0.0:5432    0.0.0.0:0    LISTENING",
		"   \u5982\u679C\u53EA\u770B\u5230 127.0.0.1:5432\uFF0C\u8BF4\u660E listen_addresses \u672A\u751F\u6548",
		"\u2461 \u672C\u5730 psql \u8FDE\u63A5\uFF1Apsql -h localhost -U YOUR_PASSWORD -d YOUR_PASSWORD",
		"\u2462 \u4F7F\u7528\u670D\u52A1\u5668 IP \u8FDE\u63A5\uFF1Apsql -h 192.168.1.100 -U YOUR_PASSWORD -d YOUR_PASSWORD"
	], "00b894")
);

children.push(
	new Paragraph({
		spacing: { before: 200, after: 80 },
		children: [bodyBold("6.2 \u4ECE\u5C40\u57DF\u7F51\u5185\u5176\u4ED6\u7535\u8111\u9A8C\u8BC1")]
	}),
	...tipBox("\u9A8C\u8BC1\u65B9\u6CD5", [
		"\u2460 \u68C0\u67E5\u7F51\u7EDC\u8FDE\u901A\u6027\uFF1Atelnet 192.168.1.100 5432",
		"   \u5982\u679C\u8FDE\u63A5\u6210\u529F\uFF0C\u5C4F\u5E55\u4F1A\u53D8\u4E3A\u7A7A\u767D\u6216\u663E\u793A\u4E71\u7801\uFF0C\u8868\u793A\u7AEF\u53E3\u5DF2\u5F00\u653E",
		"   \u5982\u679C\u8FDE\u63A5\u5931\u8D25\uFF0C\u8BF4\u660E\u9632\u706B\u5899\u6216\u7F51\u7EDC\u914D\u7F6E\u6709\u95EE\u9898",
		"\u2461 \u4F7F\u7528 pg_isready \u68C0\u67E5\uFF1A",
		"   pg_isready -h 192.168.1.100 -p 5432 -U YOUR_PASSWORD",
		"   \u671F\u671B\u7ED3\u679C\uFF1A192.168.1.100:5432 - accepting connections",
		"\u2462 \u4F7F\u7528 psql \u5B8C\u6574\u8FDE\u63A5\u6D4B\u8BD5\uFF1A",
		"   psql -h 192.168.1.100 -p 5432 -U YOUR_PASSWORD -d YOUR_PASSWORD",
		"   \u8F93\u5165\u5BC6\u7801\u540E\u80FD\u8FDB\u5165 psql \u547D\u4EE4\u884C\u5373\u8868\u793A\u914D\u7F6E\u6210\u529F"
	], "00b894")
);

// ============ 步骤 7：常见问题 ============
children.push(
	stepTitle("7", "\u5E38\u89C1\u95EE\u9898\u6392\u67E5"),
	new Paragraph({
		spacing: { after: 120, line: 360 },
		children: [bodyRun("\u4EE5\u4E0B\u662F\u914D\u7F6E\u8FC7\u7A0B\u4E2D\u6700\u5E38\u89C1\u7684\u95EE\u9898\u53CA\u89E3\u51B3\u65B9\u6848\uFF1A")]
	})
);

// 问题表格
const qColW = [3120, 6240];
children.push(
	new Table({
		width: { size: 9360, type: WidthType.DXA },
		columnWidths: qColW,
		rows: [
			new TableRow({ children: [headerCell("\u95EE\u9898\u73B0\u8C61", qColW[0]), headerCell("\u89E3\u51B3\u65B9\u6848", qColW[1])] }),
			new TableRow({ children: [
				dataCell("\u201CConnection refused\u201D \u9519\u8BEF", qColW[0]),
				dataCell("\u68C0\u67E5 PostgreSQL \u670D\u52A1\u662F\u5426\u8FD0\u884C\u3001listen_addresses \u662F\u5426\u4E3A '*' \u3001\u9632\u706B\u5899\u662F\u5426\u5F00\u653E 5432 \u7AEF\u53E3", qColW[1])
			]}),
			new TableRow({ children: [
				dataCell("\u201Cno pg_hba.conf entry\u201D \u9519\u8BEF", qColW[0]),
				dataCell("\u68C0\u67E5 pg_hba.conf \u662F\u5426\u6DFB\u52A0\u4E86\u5BF9\u5E94\u7F51\u6BB5\u7684\u89C4\u5219\u3001IP \u5730\u5740\u683C\u5F0F\u662F\u5426\u6B63\u786E\uFF08\u6CE8\u610F\u7528 CIDR \u683C\u5F0F\uFF09", qColW[1])
			]}),
			new TableRow({ children: [
				dataCell("\u201Cpassword authentication failed\u201D \u9519\u8BEF", qColW[0]),
				dataCell("\u786E\u8BA4\u7528\u6237\u540D\u5BC6\u7801\u6B63\u786E\u3001\u8BE5\u7528\u6237\u662F\u5426\u5B58\u5728\u3001\u662F\u5426\u6709\u767B\u5F55\u6743\u9650", qColW[1])
			]}),
			new TableRow({ children: [
				dataCell("pgAdmin \u8FDE\u63A5\u8D85\u65F6", qColW[0]),
				dataCell("\u68C0\u67E5\u9632\u706B\u5899\u3001\u786E\u8BA4\u670D\u52A1\u5668 IP \u6B63\u786E\u3001\u5C1D\u8BD5\u5148\u7528 telnet \u6D4B\u8BD5\u7AEF\u53E3\u53EF\u8FBE\u6027", qColW[1])
			]}),
			new TableRow({ children: [
				dataCell("\u91CD\u542F\u540E\u914D\u7F6E\u672A\u751F\u6548", qColW[0]),
				dataCell("\u786E\u8BA4\u4FEE\u6539\u7684\u662F\u6B63\u786E\u7684\u914D\u7F6E\u6587\u4EF6\u3001\u53BB\u6389\u4E86\u6CE8\u91CA\u7B26 #\u3001\u5B8C\u5168\u91CD\u542F\u800C\u975E reload", qColW[1])
			]}),
		]
	})
);

// ============ 步骤 8：安全建议 ============
children.push(
	stepTitle("8", "\u5B89\u5168\u5EFA\u8BAE"),
	...tipBox("\u91CD\u8981\u63D0\u793A", [
		"1. \u4E0D\u8981\u4F7F\u7528 trust \u8BA4\u8BC1\u65B9\u5F0F\uFF0C\u59CB\u7EC8\u4F7F\u7528 scram-sha-256",
		"2. \u4E0D\u8981\u4F7F\u7528 0.0.0.0/0 \u5F00\u653E\u6240\u6709\u7F51\u6BB5\uFF0C\u53EA\u5F00\u653E\u9700\u8981\u7684\u7F51\u6BB5",
		"3. \u4E3A YOUR_PASSWORD \u8D85\u7EA7\u7528\u6237\u8BBE\u7F6E\u5F3A\u5BC6\u7801\uFF1AALTER USER YOUR_PASSWORD PASSWORD '\u5F3A\u5BC6\u7801';",
		"4. \u5EFA\u8BAE\u521B\u5EFA\u4E13\u7528\u7684\u5E94\u7528\u7528\u6237\uFF0C\u800C\u975E\u76F4\u63A5\u4F7F\u7528 YOUR_PASSWORD",
		"5. \u5B9A\u671F\u5907\u4EFD\u6570\u636E\u5E93\uFF1Apg_dump -h localhost -U YOUR_PASSWORD dbname > backup.sql",
		"6. \u8003\u8651\u542F\u7528 SSL \u52A0\u5BC6\u8FDE\u63A5\uFF08\u751F\u4EA7\u73AF\u5883\uFF09",
		"7. \u5B9A\u671F\u67E5\u770B PostgreSQL \u65E5\u5FD7\uFF0C\u76D1\u63A7\u5F02\u5E38\u8BBF\u95EE"
	], "d63031")
);

// 页码标记
children.push(
	new Paragraph({ spacing: { before: 600 }, children: [new TextRun({
		text: "\u2014\u2014 \u6587\u6863\u7ED3\u675F \u2014\u2014",
		font: bodyFont, size: 20, color: "95a5a6", italics: true
	})]})
);

// ============ 生成文档 ============
const doc = new Document({
	styles: {
		default: {
			document: {
				run: { font: bodyFont, size: 22 }
			}
		},
		paragraphStyles: [
			{
				id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
				run: { size: 32, bold: true, font: bodyFont, color: "2c3e50" },
				paragraph: { spacing: { before: 360, after: 200 }, outlineLevel: 0 }
			},
			{
				id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
				run: { size: 28, bold: true, font: bodyFont, color: "1a73e8" },
				paragraph: { spacing: { before: 280, after: 160 }, outlineLevel: 1 }
			},
		]
	},
	numbering: {
		config: [
			{
				reference: "bullets",
				levels: [{
					level: 0, format: LevelFormat.BULLET, text: "\u2022",
					alignment: AlignmentType.LEFT,
					style: { paragraph: { indent: { left: 720, hanging: 360 } } }
				}]
			}
		]
	},
	sections: [{
		properties: {
			page: {
				size: { width: 11906, height: 16838 },  // A4
				margin: { top: 1440, right: 1440, bottom: 1440, left: 1440 }
			}
		},
		children
	}]
});

const outPath = "d:/GarMap/qgis_plugins/cplusplus/map_product_manager_tools/scripts/PostgreSQL_局域网访问配置指南.docx";
Packer.toBuffer(doc).then(buf => {
	fs.writeFileSync(outPath, buf);
	console.log("文档已生成: " + outPath);
});
