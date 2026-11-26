#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QTimer>
#include "qgsmapcanvas.h"

// The properties dialogs apply changes before the next event-loop turn. Redraw
// then so the canvas discards cached layer images for both Apply and OK.
inline void connectLayerStyleRefresh(QDialog* dialog, QgsMapCanvas* canvas)
{
    if (!dialog || !canvas) return;
    QDialogButtonBox* buttons = nullptr;
    // QGIS embeds other dialogs with their own "buttonBox". The properties
    // button box is the one containing both Apply and OK.
    for (QDialogButtonBox* candidate : dialog->findChildren<QDialogButtonBox*>(QStringLiteral("buttonBox")))
    {
        if (candidate->button(QDialogButtonBox::Apply) && candidate->button(QDialogButtonBox::Ok))
        {
            buttons = candidate;
            break;
        }
    }
    if (!buttons) return;
    QObject::connect(buttons, &QDialogButtonBox::clicked, canvas,
        [buttons, canvas](QAbstractButton* button)
        {
            const auto standardButton = buttons->standardButton(button);
            if (standardButton == QDialogButtonBox::Apply || standardButton == QDialogButtonBox::Ok)
                QTimer::singleShot(0, canvas, [canvas]() { canvas->redrawAllLayers(); });
        });
}
