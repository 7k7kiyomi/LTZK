#include "../CatalogCheckedSelection.h"

#include <QCoreApplication>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

class CheckedProxyModel : public QSortFilterProxyModel
{
public:
    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        Qt::ItemFlags result = QSortFilterProxyModel::flags(index);
        if (index.isValid() && !index.data().toString().isEmpty() &&
            index.data().toString().endsWith(QStringLiteral(".shp")))
        {
            result |= Qt::ItemIsUserCheckable;
        }
        return result;
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (role == Qt::CheckStateRole &&
            (flags(index) & Qt::ItemIsUserCheckable))
        {
            return m_checkedNames.contains(index.data().toString())
                ? Qt::Checked : Qt::Unchecked;
        }
        return QSortFilterProxyModel::data(index, role);
    }

    void setChecked(const QString& name, bool checked)
    {
        if (checked)
        {
            m_checkedNames.insert(name);
        }
        else
        {
            m_checkedNames.remove(name);
        }
    }

private:
    QSet<QString> m_checkedNames;
};

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    QStandardItemModel source;
    auto* favorites = new QStandardItem(QStringLiteral("Favorites"));
    auto* directory = new QStandardItem(QStringLiteral("Folder"));
    auto* first = new QStandardItem(QStringLiteral("A_LyYouJingQu.shp"));
    auto* second = new QStandardItem(QStringLiteral("AANP.shp"));
    auto* unchecked = new QStandardItem(QStringLiteral("other.shp"));
    for (QStandardItem* item : {first, second, unchecked})
    {
        directory->appendRow(item);
    }
    favorites->appendRow(directory);
    source.appendRow(favorites);

    CheckedProxyModel proxy;
    proxy.setSourceModel(&source);
    proxy.setChecked(QStringLiteral("A_LyYouJingQu.shp"), true);
    proxy.setChecked(QStringLiteral("AANP.shp"), true);
    const QModelIndex root = proxy.mapFromSource(source.indexFromItem(favorites));
    const QModelIndex current = proxy.mapFromSource(source.indexFromItem(second));
    if (!root.isValid() || !current.isValid())
    {
        return 1;
    }

    const QVector<QModelIndex> checked = collectCheckedCatalogIndexes(&proxy, root);
    if (checked.size() != 2 ||
        checked.at(0).data().toString() != QStringLiteral("A_LyYouJingQu.shp") ||
        checked.at(1).data().toString() != QStringLiteral("AANP.shp"))
    {
        return 2;
    }

    proxy.setChecked(QStringLiteral("A_LyYouJingQu.shp"), false);
    if (collectCheckedCatalogIndexes(&proxy, root).size() != 1)
    {
        return 3;
    }
    return 0;
}
