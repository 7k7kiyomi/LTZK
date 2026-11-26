#pragma once

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVector>

// Collect check-state from the same model and indexes that the tree displays.
inline QVector<QModelIndex> collectCheckedCatalogIndexes(
    const QAbstractItemModel* model, const QModelIndex& root)
{
    QVector<QModelIndex> checked;
    if (!model || (root.isValid() && root.model() != model))
    {
        return checked;
    }

    const auto visit = [&](const auto& self, const QModelIndex& parent) -> void
    {
        for (int row = 0; row < model->rowCount(parent); ++row)
        {
            const QModelIndex index = model->index(row, 0, parent);
            if (!index.isValid())
            {
                continue;
            }
            if ((model->flags(index) & Qt::ItemIsUserCheckable) &&
                model->data(index, Qt::CheckStateRole).toInt() == Qt::Checked)
            {
                checked.append(index);
            }
            self(self, index);
        }
    };
    visit(visit, root);
    return checked;
}
