#pragma once

#include <QFont>
#include <QGuiApplication>
#include <QMargins>
#include <QRegularExpression>
#include <QScreen>
#include <QSize>
#include <QString>
#include <QWidget>
#include <QtGlobal>

namespace LTZKUiScale
{
    constexpr qreal kBaseLogicalWidth = 1920.0;
    constexpr qreal kBaseLogicalHeight = 1080.0;
    constexpr const char* kScaleOverrideProperty = "ltzkUiScaleOverride";
    inline qreal factorForLogicalSize(const QSize& logicalSize)
    {
        if (!logicalSize.isValid() || logicalSize.isEmpty())
        {
            return 1.0;
        }

        const qreal widthScale = logicalSize.width() / kBaseLogicalWidth;
        const qreal heightScale = logicalSize.height() / kBaseLogicalHeight;
        const qreal scale = qMin(widthScale, heightScale);
        if (!qIsFinite(scale) || scale <= 0.0)
        {
            return 1.0;
        }

        return scale;
    }

    inline qreal factorForScreen(const QScreen* screen)
    {
        return screen
            ? factorForLogicalSize(screen->geometry().size())
            : 1.0;
    }

    inline qreal factorForWidget(const QWidget* widget)
    {
        const QWidget* current = widget;
        while (current)
        {
            const QVariant overrideValue = current->property(kScaleOverrideProperty);
            if (overrideValue.isValid())
            {
                bool ok = false;
                const qreal overrideScale = overrideValue.toDouble(&ok);
                if (ok && qIsFinite(overrideScale) && overrideScale > 0.0)
                {
                    return overrideScale;
                }
            }
            if (current->isWindow())
            {
                break;
            }
            current = current->parentWidget();
        }

        const QWidget* window = widget ? widget->window() : nullptr;
        const QScreen* screen = window ? window->screen() : QGuiApplication::primaryScreen();
        return factorForScreen(screen);
    }

    inline int scaledInt(int baseValue, qreal scale)
    {
        if (baseValue == 0)
        {
            return 0;
        }

        const int result = qRound(baseValue * scale);
        return result == 0 ? (baseValue > 0 ? 1 : -1) : result;
    }

    inline QSize scaledSize(const QSize& baseSize, qreal scale)
    {
        return QSize(
            scaledInt(baseSize.width(), scale),
            scaledInt(baseSize.height(), scale));
    }

    inline QMargins scaledMargins(const QMargins& baseMargins, qreal scale)
    {
        return QMargins(
            scaledInt(baseMargins.left(), scale),
            scaledInt(baseMargins.top(), scale),
            scaledInt(baseMargins.right(), scale),
            scaledInt(baseMargins.bottom(), scale));
    }

    inline QFont scaledFont(const QFont& baseFont, qreal scale)
    {
        QFont font(baseFont);
        if (font.pixelSize() > 0)
        {
            font.setPixelSize(qMax(1, scaledInt(font.pixelSize(), scale)));
        }
        else if (font.pointSizeF() > 0.0)
        {
            font.setPointSizeF(qMax<qreal>(1.0, font.pointSizeF() * scale));
        }
        return font;
    }

    inline QString scaleStyleSheetPixels(const QString& styleSheet, qreal scale)
    {
        if (styleSheet.isEmpty() || qFuzzyCompare(scale, 1.0))
        {
            return styleSheet;
        }

        static const QRegularExpression pixelExpression(
            QStringLiteral("(-?\\d+(?:\\.\\d+)?)px\\b"));

        QString scaledStyle;
        scaledStyle.reserve(styleSheet.size());
        qsizetype copiedThrough = 0;
        QRegularExpressionMatchIterator matches = pixelExpression.globalMatch(styleSheet);
        while (matches.hasNext())
        {
            const QRegularExpressionMatch match = matches.next();
            scaledStyle += styleSheet.mid(copiedThrough, match.capturedStart() - copiedThrough);

            bool ok = false;
            const qreal basePixels = match.captured(1).toDouble(&ok);
            if (!ok)
            {
                scaledStyle += match.captured(0);
            }
            else
            {
                int scaledPixels = qRound(basePixels * scale);
                if (!qFuzzyIsNull(basePixels) && scaledPixels == 0)
                {
                    scaledPixels = basePixels > 0.0 ? 1 : -1;
                }
                scaledStyle += QString::number(scaledPixels);
                scaledStyle += QStringLiteral("px");
            }
            copiedThrough = match.capturedEnd();
        }
        scaledStyle += styleSheet.mid(copiedThrough);
        return scaledStyle;
    }
}
