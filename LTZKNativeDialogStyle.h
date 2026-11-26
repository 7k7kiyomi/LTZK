#pragma once

#include "LTZKUiScale.h"
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QScrollBar>
#include <QStyle>
#include <QTableView>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextTable>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

namespace LTZKNativeDialogStyle
{
    inline QString embeddedEditorStyleSheet()
    {
        // A compound input owns its frame. Its child editor only draws text.
        return QStringLiteral(
            "\nQSpinBox QLineEdit, QDoubleSpinBox QLineEdit, QDateEdit QLineEdit, QDateTimeEdit QLineEdit,"
            " QSpinBox QLineEdit:focus, QDoubleSpinBox QLineEdit:focus, QDateEdit QLineEdit:focus, QDateTimeEdit QLineEdit:focus {"
            " border: 0; background: transparent; }"
            "\nQComboBox QLineEdit, QComboBox QLineEdit:focus { border: 0; border-radius: 0;"
            " background: transparent; padding: 0; min-height: 0; }"
            // These QGIS controls have compact, sometimes fixed dimensions. The
            // application-wide 12 px padding otherwise leaves no room for icons.
            "\nQgsPropertyOverrideButton, QgsTextFormatWidget QToolButton {"
            " padding: 2px; min-width: 0; min-height: 0; }\n");
    }

    inline bool isQgisDialog(const QWidget *widget)
    {
        return widget && QByteArray(widget->metaObject()->className()).startsWith("Qgs");
    }

    inline QString nativeHeaderStyleSheet(qreal scale)
    {
        return LTZKUiScale::scaleStyleSheetPixels(QStringLiteral(
            "\nQGroupBox[ltzkPluginNativeHeader=\"true\"] { margin-top: 28px; padding-top: 8px; }"
            "\nQGroupBox[ltzkPluginNativeHeader=\"true\"]::title {"
            " subcontrol-origin: margin; subcontrol-position: top left; left: 0px; top: 0px;"
            " margin-left: 0; margin-right: 0; padding: 0; font-size: 16px; }"
            // Metadata sections keep their native titles inside the frame instead
            // of reserving the platform's 52 px margin for overlay headings.
            "\nQDialog#MetadataManagerDialog QGroupBox[ltzkPluginNativeHeader=\"true\"] {"
            " margin-top: 0px; padding-top: 28px; }"
            "\nQDialog#MetadataManagerDialog QGroupBox[ltzkPluginNativeHeader=\"true\"]::title {"
            " subcontrol-origin: padding; }"
            "\nQHeaderView[ltzkPluginNativeHeader=\"true\"]::section {"
            " padding: 3px 8px; font-size: 16px; }\n"), scale);
    }

    inline QToolButton *collapseButton(QGroupBox *group)
    {
        if (!group)
            return nullptr;
        if (QToolButton *button = group->findChild<QToolButton *>(QStringLiteral("collapseButton"), Qt::FindDirectChildrenOnly))
            return button;
        for (QToolButton *button : group->findChildren<QToolButton *>(QString(), Qt::FindDirectChildrenOnly))
            if (button->inherits("QgsGroupBoxCollapseButton"))
                return button;
        return nullptr;
    }

    inline void prepareHeaders(QWidget *widget)
    {
        if (!widget)
            return;
        const bool nativeDialog = isQgisDialog(widget);
        const bool metadataDialog = widget->objectName() == QStringLiteral("MetadataManagerDialog");
        for (QGroupBox *group : widget->findChildren<QGroupBox *>())
        {
            const bool nativeHeader = nativeDialog || metadataDialog || group->isCheckable()
                || group->inherits("QgsCollapsibleGroupBoxBasic");
            group->setProperty("ltzkPluginNativeHeader", nativeHeader);
            if (nativeHeader)
                group->setAlignment(Qt::AlignLeft);
        }
        for (QHeaderView *header : widget->findChildren<QHeaderView *>())
            header->setProperty("ltzkPluginNativeHeader", true);
    }

    inline void polishMetadataBrowser(QTextBrowser *browser)
    {
        if (browser->property("ltzkMetadataThemeBusy").toBool())
            return;
        browser->setProperty("ltzkMetadataThemeBusy", true);
        browser->ensurePolished();
        QTextDocument *document = browser->document();
        const bool modified = document->isModified();
        const int scrollPosition = browser->verticalScrollBar()->value();
        const QPalette palette = browser->palette();
        const QColor background = palette.color(QPalette::Base);
        const QColor foreground = palette.color(QPalette::Text);
        const QColor heading = palette.color(QPalette::WindowText);
        const QColor link = palette.color(QPalette::Highlight);
        const QColor alternate = background.lightness() < 128 ? background.lighter(125) : background.darker(103);
        if (!browser->property("ltzkMetadataReportOriginalCss").isValid())
            browser->setProperty("ltzkMetadataReportOriginalCss", document->defaultStyleSheet());
        document->setDefaultStyleSheet(browser->property("ltzkMetadataReportOriginalCss").toString()
            + QStringLiteral("\nbody { background-color: %1; color: %2; }"
                " h1, h2, h3, h4, h5 { color: %3; background-color: transparent; }"
                " a { color: %4; } .tabular-view .odd-row { background-color: %5; }")
                .arg(background.name(), foreground.name(), heading.name(), link.name(),
                    alternate.name()));

        // QSS does not recolor formats already parsed from QGIS's HTML report CSS.
        // Merge only paint properties, preserving fields, links and rich text layout.
        QTextCursor cursor(document);
        cursor.beginEditBlock();
        cursor.select(QTextCursor::Document);
        QTextCharFormat textFormat;
        textFormat.setForeground(foreground);
        textFormat.setBackground(QBrush(Qt::NoBrush));
        cursor.mergeCharFormat(textFormat);
        for (QTextBlock block = document->begin(); block.isValid(); block = block.next())
        {
            if (block.blockFormat().headingLevel() > 0)
            {
                QTextCursor headingCursor(block);
                headingCursor.select(QTextCursor::BlockUnderCursor);
                QTextCharFormat headingFormat;
                headingFormat.setForeground(heading);
                headingCursor.mergeCharFormat(headingFormat);
            }
            for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it)
            {
                const QTextFragment fragment = it.fragment();
                if (!fragment.isValid() || !fragment.charFormat().isAnchor())
                    continue;
                QTextCursor anchorCursor(document);
                anchorCursor.setPosition(fragment.position());
                anchorCursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
                QTextCharFormat anchorFormat;
                anchorFormat.setForeground(link);
                anchorCursor.mergeCharFormat(anchorFormat);
            }
        }
        const auto recolorFrame = [&alternate, &background](QTextFrame *frame, const auto &self) -> void
        {
            QTextFrameFormat format = frame->frameFormat();
            format.setBackground(background);
            frame->setFrameFormat(format);
            if (QTextTable *table = qobject_cast<QTextTable *>(frame))
            {
                for (int row = 0; row < table->rows(); ++row)
                    for (int column = 0; column < table->columns(); ++column)
                    {
                        QTextTableCell cell = table->cellAt(row, column);
                        QTextCharFormat cellFormat = cell.format();
                        if (cellFormat.background().style() != Qt::NoBrush)
                        {
                            cellFormat.setBackground(row % 2 ? alternate : background);
                            cell.setFormat(cellFormat);
                        }
                    }
            }
            for (QTextFrame *child : frame->childFrames())
                self(child, self);
        };
        recolorFrame(document->rootFrame(), recolorFrame);
        cursor.endEditBlock();
        document->setModified(modified);
        browser->verticalScrollBar()->setValue(scrollPosition);
        browser->setProperty("ltzkMetadataThemeBusy", false);

        if (!browser->property("ltzkMetadataThemeConnected").toBool())
        {
            browser->setProperty("ltzkMetadataThemeConnected", true);
            QObject::connect(document, &QTextDocument::contentsChanged, browser, [browser]()
            {
                if (browser->property("ltzkMetadataThemeBusy").toBool()
                    || browser->property("ltzkMetadataThemePending").toBool())
                    return;
                browser->setProperty("ltzkMetadataThemePending", true);
                QTimer::singleShot(0, browser, [browser]()
                {
                    browser->setProperty("ltzkMetadataThemePending", false);
                    polishMetadataBrowser(browser);
                });
            });
        }
    }

    inline void polish(QWidget *widget, qreal scale)
    {
        if (!widget)
            return;
        const QString headerStyle = nativeHeaderStyleSheet(scale);
        for (QGroupBox *group : widget->findChildren<QGroupBox *>())
        {
            if (!group->property("ltzkPluginNativeHeader").toBool())
                continue;

            int indicatorGutter = 0;
            if (group->isCheckable())
            {
                const int indicatorWidth = group->style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, group);
                indicatorGutter = indicatorWidth + LTZKUiScale::scaledInt(4, scale);
            }
            else if (group->inherits("QgsCollapsibleGroupBoxBasic"))
            {
                if (QToolButton *button = collapseButton(group))
                {
                    const int buttonExtent = LTZKUiScale::scaledInt(16, scale);
                    const int iconPixels = LTZKUiScale::scaledInt(12, scale);
                    const QSize iconExtent(iconPixels, iconPixels);
                    if (button->iconSize() != iconExtent)
                        button->setIconSize(iconExtent);
                    if (button->size() != QSize(buttonExtent, buttonExtent))
                        button->setFixedSize(buttonExtent, buttonExtent);

                    // The global tool-button rule adds 12 px on each side. The
                    // QGIS arrow button is only 16 px wide, so its icon has no
                    // drawable content area until its own padding is cleared.
                    const QString buttonMarker = QStringLiteral("\n/* LTZK collapse button */\n");
                    const QString buttonEndMarker = QStringLiteral("\n/* LTZK collapse button end */\n");
                    const QString buttonBlock = LTZKUiScale::scaleStyleSheetPixels(
                        QStringLiteral("\nQToolButton#collapseButton { padding: 0px; margin: 0px;"
                            " min-width: 16px; min-height: 16px; max-width: 16px; max-height: 16px; }"
                            "\nQToolButton#collapseButton:focus { border: 1px solid palette(highlight); }\n"), scale);
                    const QString replacement = buttonMarker + buttonBlock + buttonEndMarker;
                    QString buttonStyle = button->styleSheet();
                    const int previous = buttonStyle.indexOf(buttonMarker);
                    if (previous >= 0)
                    {
                        const int end = buttonStyle.indexOf(buttonEndMarker, previous);
                        if (end >= 0)
                            buttonStyle.replace(previous, end + buttonEndMarker.size() - previous, replacement);
                    }
                    else
                        buttonStyle += replacement;
                    if (button->styleSheet() != buttonStyle)
                        button->setStyleSheet(buttonStyle);
                    const int arrowWidth = button->width() > 0 ? button->width() : buttonExtent;
                    indicatorGutter = arrowWidth + LTZKUiScale::scaledInt(4, scale);
                }
            }
            // Each group gets its measured indicator gutter locally. This lets
            // plain groups start at x=0 while checkable and collapsible groups
            // reserve only the space their actual native control needs.
            const QString marker = QStringLiteral("\n/* LTZK native header */\n");
            const QString endMarker = QStringLiteral("\n/* LTZK native header end */\n");
            const QString groupHeaderStyle = headerStyle
                + QStringLiteral("\nQGroupBox[ltzkPluginNativeHeader=\"true\"]::title { margin-left: %1px; }\n")
                    .arg(indicatorGutter);
            const QString replacement = marker + groupHeaderStyle + endMarker;
            QString style = group->styleSheet();
            const int previous = style.indexOf(marker);
            if (previous >= 0)
            {
                const int end = style.indexOf(endMarker, previous);
                if (end >= 0)
                    style.replace(previous, end + endMarker.size() - previous, replacement);
            }
            else
                style += replacement;
            if (group->styleSheet() != style)
                group->setStyleSheet(style);
        }
        for (QHeaderView *header : widget->findChildren<QHeaderView *>())
        {
            if (header->orientation() != Qt::Horizontal || header->isHidden())
                continue;
            header->ensurePolished();
            header->setDefaultAlignment((header->defaultAlignment() & ~Qt::AlignVertical_Mask) | Qt::AlignVCenter);
            const int padding = LTZKUiScale::scaledInt(3, scale);
            const int height = qMax(header->sizeHint().height(), header->fontMetrics().height() + 2 * padding + 2);
            if (!header->property("ltzkOriginalHeaderMinimumHeight").isValid())
                header->setProperty("ltzkOriginalHeaderMinimumHeight", header->minimumHeight());
            header->setMinimumHeight(qMax(header->property("ltzkOriginalHeaderMinimumHeight").toInt(), height));
            if (QAbstractItemView *view = qobject_cast<QAbstractItemView *>(header->parentWidget()))
            {
                // In a crowded scroll page the whole view may shrink below its header.
                const int bodyRow = qMax(view->fontMetrics().height() + 2 * padding, LTZKUiScale::scaledInt(24, scale));
                if (!view->property("ltzkOriginalViewMinimumHeight").isValid())
                    view->setProperty("ltzkOriginalViewMinimumHeight", view->minimumHeight());
                view->setMinimumHeight(qMax(view->property("ltzkOriginalViewMinimumHeight").toInt(), height + bodyRow + 2 * view->frameWidth()));
            }
        }
        for (QTextBrowser *browser : widget->findChildren<QTextBrowser *>())
            if (browser->objectName() == QStringLiteral("teMetadataViewer"))
                polishMetadataBrowser(browser);
    }
}
