#include <QApplication>
#include <QCryptographicHash>
#include <QHash>
#include <QImage>
#include <QList>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QTimer>
#include <QTreeView>
#include <QVariantMap>
#include <QVector>
#include <QDebug>
#include <functional>
#include <QFontDatabase>
#include <QFont>
#include <cstdio>
#include "qgsapplication.h"
#include "qgsdatacollectionitem.h"
#include "qgsmimedatautils.h"
#include "auth/LTZKAuthManager.h"
#include "DataCatalogWidget.h"
template<typename Tag, typename Tag::type Member> struct Access { friend typename Tag::type access(Tag) { return Member; } };
struct Loader { using type=bool(DataCatalogWidget::*)(QSqlDatabase,QStandardItem*,QString*); friend type access(Loader); };
template struct Access<Loader,&DataCatalogWidget::loadProductDirectoryTree>;
UserSession gCurrentUserSession;
bool UserSession::canOperateProductStore() const { return false; }
QByteArray iconPixels(const QIcon& icon) {
 const QImage image=icon.pixmap(24,24).toImage().convertToFormat(QImage::Format_ARGB32);
 return QCryptographicHash::hash(QByteArray(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes()),QCryptographicHash::Sha256).toHex();
}
QStandardItem* findDirectory(QStandardItem* p,int id) {
 if(p->text()==QStringLiteral("\u6c38\u767b\u53bf")) return p;
 for(int row=0;row<p->rowCount();++row) if(auto* f=findDirectory(p->child(row),id)) return f;
 return nullptr;
}
int main(int argc,char** argv) {
 qputenv("QT_QPA_PLATFORM","offscreen");
 QgsApplication app(argc,argv,true);
 QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"),true);
 QgsApplication::initQgis();
 int fontId=QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
 const auto families=QFontDatabase::applicationFontFamilies(fontId);
 if(!families.isEmpty()) app.setFont(QFont(families.first(),10));
 int result=0;
 {
 QSettings s(QStringLiteral("GarMap"),QStringLiteral("MapProductManager"));
 auto db=QSqlDatabase::addDatabase(QStringLiteral("QPSQL"),QStringLiteral("icon-probe"));
 db.setHostName(s.value(QStringLiteral("db/host")).toString());
 db.setPort(s.value(QStringLiteral("db/port"),5432).toInt());
 db.setDatabaseName(s.value(QStringLiteral("db/database")).toString());
 db.setUserName(s.value(QStringLiteral("db/user")).toString());
 db.setPassword(s.value(QStringLiteral("db/password")).toString());
 if(!db.open()) { qCritical()<<"Database connection failed"; return 2; }
 QSqlQuery q(db); q.exec(QStringLiteral("SET default_transaction_read_only=on"));
 QString schema=s.value(QStringLiteral("db/schema"),QStringLiteral("public")).toString();
 schema.replace(QLatin1Char('"'),QStringLiteral("\"\""));
 q.exec(QStringLiteral("SET search_path TO \"%1\"").arg(schema));
 DataCatalogWidget w; for(auto* timer:w.findChildren<QTimer*>()) timer->stop();
 auto* tree=w.findChild<QTreeView*>(QStringLiteral("databaseCatalogTree"));
 auto* model=qobject_cast<QStandardItemModel*>(tree->model()); model->clear();
 auto* root=new QStandardItem(db.databaseName());
 model->appendRow(root); QString error;
 if(!(w.*access(Loader{}))(db,root,&error)) { qCritical()<<"Tree load failed"<<error; return 4; }
 auto* d=findDirectory(root,64); if(!d) { qCritical()<<"Target not rendered"; return 3; }
 const auto expected=QgsDataCollectionItem::iconDataCollection();
 const bool matches=iconPixels(d->icon())==iconPixels(expected);
 std::printf("directory_id=64 children=%d matches_qgis_gdb=%s\n",d->rowCount(),matches?"true":"false"); std::fflush(stdout);
 expected.pixmap(64,64).save(QStringLiteral("qgis_collection_icon.png"));
 d->icon().pixmap(64,64).save(QStringLiteral("actual_directory_icon.png"));
 w.resize(420,720); tree->collapseAll(); tree->setCurrentIndex(d->index());
 for(auto index=d->index().parent();index.isValid();index=index.parent()) tree->expand(index);
 w.show(); app.processEvents(); tree->grab().save(QStringLiteral("catalog_tree.png"));
 if(!matches) result=1;
 db.close();
 }
 QgsApplication::exitQgis(); return result;
}
