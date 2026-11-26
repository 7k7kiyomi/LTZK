#include "../DataCatalogWidget.cpp"
#include <qgsprovidermetadata.h>
#include <qgsproviderregistry.h>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>
template<typename Tag, typename Tag::type Member> struct MemberAccess { friend typename Tag::type access(Tag) { return Member; } };
struct SynchronizeTag { using type=bool(DataCatalogWidget::*)(bool); friend type access(SynchronizeTag); };
template struct MemberAccess<SynchronizeTag,&DataCatalogWidget::synchronizeMenuDatabaseConnection>;
class CatalogFixtureModel : public QgsBrowserModel {
public:
 explicit CatalogFixtureModel(QgsDataItem* root=nullptr) {
  if(root) {mRootItems << root; return;}
  mRootItems << new QgsLayerItem(nullptr,"point","fixture/point.shp","fixture/point.shp",Qgis::BrowserLayerType::Point,"catalog_probe");
  mRootItems << new QgsLayerItem(nullptr,"line","fixture/line.shp","fixture/line.shp",Qgis::BrowserLayerType::Line,"catalog_probe");
  mRootItems << new QgsLayerItem(nullptr,"polygon","fixture/polygon.shp","fixture/polygon.shp",Qgis::BrowserLayerType::Polygon,"catalog_probe");
  mRootItems << new QgsLayerItem(nullptr,"unknown","fixture/unknown.shp","fixture/unknown.shp",Qgis::BrowserLayerType::Vector,"catalog_probe");
 }
};
static QByteArray pixels(const QIcon& icon) { QImage i=icon.pixmap(24,24).toImage().convertToFormat(QImage::Format_ARGB32);return QCryptographicHash::hash(QByteArray(reinterpret_cast<const char*>(i.constBits()),i.sizeInBytes()),QCryptographicHash::Sha256); }
int main(int argc,char** argv) {
 QgsApplication app(argc,argv,true);QgsApplication::setPrefixPath("E:/extern/QGIS_32815",true);QgsApplication::initQgis();
 int opens=0;
 QgsProviderMetadata::CreateDataProviderFunction factory=[&](const QString&,const QgsDataProvider::ProviderOptions&,QgsDataProvider::ReadFlags&)->QgsDataProvider*{++opens;QThread::msleep(80);return nullptr;};
 QgsProviderRegistry::instance()->registerProvider(new QgsProviderMetadata("catalog_probe","catalog icon I/O probe",factory));
 CatalogFixtureModel model; CatalogBrowserProxyModel proxy; proxy.setBrowserModel(&model);
 QElapsedTimer time;time.start(); const char* names[]={"point","line","polygon"};bool icons=true;
 for(int row=0;row<model.rowCount();++row){auto *item=model.dataItem(model.index(row,0)); const auto uris=item->mimeUris(); printf("row %d wkb=%d iconName=%s\n",row,uris.isEmpty()?-1:int(uris.first().wkbType),qPrintable(item->name())); QIcon icon=qvariant_cast<QIcon>(proxy.data(proxy.mapFromSource(model.index(row,0)),Qt::DecorationRole));if(row<3) icons=icons&&pixels(icon)==pixels(catalogIcon(names[row]));}
 printf("%s catalog icons use browser geometry metadata\n",icons?"PASS":"FAIL");
 printf("%s painting catalog opens %d providers, time=%lld ms\n",opens==0?"PASS":"FAIL",opens,time.elapsed());
 if(qEnvironmentVariableIsSet("LTZK_CATALOG_TEST_DATABASE")) {
  DataCatalogWidget widget; for(auto* timer:widget.findChildren<QTimer*>())timer->stop();
  gCurrentUserSession.isLoggedIn=true; bool heartbeat=false; QTimer::singleShot(0,&widget,[&](){heartbeat=true;});
  QElapsedTimer databaseTime;databaseTime.start(); (widget.*access(SynchronizeTag{}))(false);
  const auto blockedMs=databaseTime.elapsed(); QCoreApplication::processEvents();
  printf("%s automatic saved database synchronization returns in %lld ms, UI heartbeat=%d\n",blockedMs<250&&heartbeat?"PASS":"FAIL",blockedMs,heartbeat);fflush(stdout);
  icons=icons && blockedMs<250 && heartbeat;
  QElapsedTimer wait;wait.start();while(wait.elapsed()<6000){QCoreApplication::processEvents();QThread::msleep(5);}
  (widget.*access(SynchronizeTag{}))(true); widget.applyMenuDatabaseConnection(QVariantMap());
  auto* tree=widget.findChild<QTreeView*>("databaseCatalogTree");const QString resetText=tree->model()->index(0,0).data().toString();
  wait.restart();while(wait.elapsed()<6000){QCoreApplication::processEvents();QThread::msleep(5);}
  const bool staleIgnored=tree->model()->index(0,0).data().toString()==resetText;
  printf("%s cancelled automatic connection leaves current catalog unchanged\n",staleIgnored?"PASS":"FAIL");icons=icons&&staleIgnored;
  gCurrentUserSession.clear();
 }
 const QString folder=qEnvironmentVariable("LTZK_CATALOG_TEST_FOLDER");
 if(!folder.isEmpty()) {
  auto* directory=new QgsDirectoryItem(nullptr,"real folder",folder); directory->populate(true);
  CatalogFixtureModel realModel(directory); CatalogBrowserProxyModel realProxy; realProxy.setBrowserModel(&realModel);
  QModelIndex root=realModel.index(0,0); int count=0; QElapsedTimer oldTime; oldTime.start();
  for(int row=0;row<realModel.rowCount(root);++row){auto* item=realModel.dataItem(realModel.index(row,0,root)); if(!qobject_cast<QgsLayerItem*>(item))continue; for(const auto& uri:item->mimeUris()){if(uri.layerType!="vector")continue; bool owner=false;QString error;auto* l=uri.vectorLayer(owner,error);if(owner)delete l;++count;break;}}
  const auto legacyMs=oldTime.elapsed(); QElapsedTimer newTime;newTime.start();
  for(int row=0;row<realModel.rowCount(root);++row) realProxy.data(realProxy.mapFromSource(realModel.index(row,0,root)),Qt::DecorationRole);
  printf("real local folder: %d vector entries, legacy provider-open icons=%lld ms, metadata icons=%lld ms\n",count,legacyMs,newTime.elapsed());
 }
 return icons&&opens==0?0:1;
}
