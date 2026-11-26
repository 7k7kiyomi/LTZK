#ifndef USER_INFO_BAR_H
#define USER_INFO_BAR_H

#include <QBoxLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include "database/product_metadata.h"

/**
 * @brief 为对话框在右上角添加固定高度的用户信息栏
 *
 * @param parent  父窗口
 * @param layout  布局（QBoxLayout），信息栏插入到顶部
 */
inline void addUserInfoBar(QWidget* parent, QLayout* layout)
{
	auto* frame = new QFrame(parent);
	frame->setObjectName(QStringLiteral("userInfoFrame"));
	frame->setFixedHeight(56);
	frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	auto* hLayout = new QHBoxLayout(frame);
	hLayout->setContentsMargins(12, 0, 8, 0);

	// 左侧警告标签（默认隐藏，子对话框可按需显示）
	auto* warningLabel = new QLabel(frame);
	warningLabel->setObjectName(QStringLiteral("userInfoWarningLabel"));
	warningLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	QFont warningFont = warningLabel->font();
	warningFont.setPixelSize(28);
	warningFont.setBold(true);
	warningLabel->setFont(warningFont);
	warningLabel->setVisible(false);

	auto* label = new QLabel(frame);
	label->setObjectName(QStringLiteral("userInfoLabel"));
	label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	QFont labelFont = label->font();
	labelFont.setPixelSize(28);
	labelFont.setBold(true);
	label->setFont(labelFont);

	if (gCurrentUserSession.isLoggedIn)
	{
		label->setText(QStringLiteral("用户名：%1    用户类型：%2")
			.arg(gCurrentUserSession.userName)
			.arg(accessRoleToString(gCurrentUserSession.role)));
	}
	else
	{
		label->setText(QStringLiteral(""));
	}

	hLayout->addWidget(warningLabel);
	hLayout->addStretch();
	hLayout->addWidget(label);

	QBoxLayout* boxLayout = qobject_cast<QBoxLayout*>(layout);
	if (boxLayout)
		boxLayout->insertWidget(0, frame);
	else
		layout->addWidget(frame);
}

#endif // USER_INFO_BAR_H
