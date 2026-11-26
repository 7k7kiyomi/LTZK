#pragma once

#include <QAbstractButton>
#include <QFontInfo>
#include <QLineEdit>
#include <QTreeWidget>

// Shared by the host's initial and deferred plugin-window style passes.
// The view's resolved font owns cell typography; theme geometry is left intact.
namespace LTZKSeriesTableTypography
{
    inline bool isConfigTree(const QTreeWidget* tree)
    {
        if (!tree || tree->columnCount()!=6 || !tree->headerItem()) return false;
        const auto* header=tree->headerItem();
        return header->text(0)==QStringLiteral("主检查地图")
            && header->text(1)==QStringLiteral("主图比例尺")
            && header->text(2)==QStringLiteral("关联地图")
            && header->text(5)==QStringLiteral("状态");
    }

    inline void apply(QWidget* window)
    {
        if (!window) return;
        const QString begin=QStringLiteral("\n/* LTZK series cell typography */\n");
        const QString end=QStringLiteral("\n/* LTZK series cell typography end */\n");
        for (auto* tree : window->findChildren<QTreeWidget*>()) {
            if (!isConfigTree(tree)) continue;
            tree->ensurePolished();
            if (!tree->property("ltzkSeriesCellTypography").toBool())
                tree->setProperty("ltzkSeriesCellTypography",true);
            const QFont font=tree->font();
            QString family=font.family();
            family.replace(QLatin1Char('\\'),QStringLiteral("\\\\"));
            family.replace(QLatin1Char('"'),QStringLiteral("\\\""));
            // This is already the final, screen-scaled view font. Never run
            // these pixels through LTZKUiScale a second time.
            const QString rule=QStringLiteral(
                "QLineEdit, QPushButton, QToolButton { font-family: \"%1\"; "
                "font-size: %2px; font-weight: %3; font-style: %4; }")
                .arg(family).arg(QFontInfo(font).pixelSize())
                .arg(font.bold() ? QStringLiteral("bold") : QStringLiteral("normal"))
                .arg(font.italic() ? QStringLiteral("italic") : QStringLiteral("normal"));
            const QString block=begin+rule+end;
            const auto controls=tree->viewport()->findChildren<QWidget*>();
            for (auto* control : controls) {
                if (!qobject_cast<QLineEdit*>(control) && !qobject_cast<QAbstractButton*>(control)) continue;
                QString style=control->styleSheet();
                const int start=style.indexOf(begin);
                if (start>=0) {
                    const int finish=style.indexOf(end,start);
                    if (finish>=0) style.remove(start,finish+end.size()-start);
                }
                style+=block;
                if (control->font()!=font) control->setFont(font);
                if (control->styleSheet()!=style) control->setStyleSheet(style);
            }
        }
    }
}
