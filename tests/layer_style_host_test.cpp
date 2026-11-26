#include "../LTZK.h"
#include <qgsapplication.h>
#include <qgsgui.h>
#include <qgseditorwidgetregistry.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsgeometry.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgslinesymbol.h>
#include <qgssymbolselectordialog.h>
#include <qgsvectorlayerproperties.h>
#include <qgslayertreemodel.h>
#include <qgslayertreelayer.h>
#include <qgslayertreegroup.h>
#include <qgsmapcanvas.h>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QPushButton>
#include <QTimer>
#include <QFontDatabase>
#include <QMenu>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>
template<typename Tag, typename Tag::type Member> struct MemberAccess { friend typename Tag::type access(Tag) { return Member; } };
struct CurrentModuleTag { using type = QString LTZK::*; friend type access(CurrentModuleTag); };
template struct MemberAccess<CurrentModuleTag, &LTZK::m_currentBusinessModuleTitle>;
struct RegisterResultTag { using type = bool (LTZK::*)(QgsVectorLayer*, const QString&); friend type access(RegisterResultTag); };
template struct MemberAccess<RegisterResultTag, &LTZK::registerQualityResultLayer>;
static void spin(int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){QCoreApplication::processEvents();QThread::msleep(5);}}
static int countColor(const QImage& image,bool blue) { int count=0; for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x){ QColor c=image.pixelColor(x,y); if(blue ? (c.blue()>180 && c.blue()>c.red()+60 && c.blue()>c.green()+60) : (c.red()>180 && c.red()>c.blue()+60 && c.red()>c.green()+60)) ++count; } return count; }
int main(int argc,char** argv){
 QgsApplication app(argc,argv,true); const int fontId=QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc"); const auto families=QFontDatabase::applicationFontFamilies(fontId); if(!families.isEmpty()) app.setFont(QFont(families.first(),10)); QgsApplication::setPrefixPath("E:/extern/QGIS_32815",true); QgsApplication::initQgis(); QgsGui::instance(); QgsGui::editorWidgetRegistry()->initEditors();
 LTZK window; for(auto* t:window.findChildren<QTimer*>()) t->stop();
 const QString source=qEnvironmentVariable("LTZK_STYLE_TEST_SOURCE");
 auto* layer=new QgsVectorLayer(source.isEmpty()?QString("LineString?crs=EPSG:4326"):source,"county road",source.isEmpty()?"memory":"ogr");
 if(!layer->isValid()) {printf("FAIL source invalid\n"); return 3;}
 if(source.isEmpty()) {QgsFeature f(layer->fields()); f.setGeometry(QgsGeometry::fromWkt("LINESTRING (0 5, 10 5)")); layer->dataProvider()->addFeatures(QgsFeatureList()<<f); layer->updateExtents();}
 if(qEnvironmentVariableIsSet("LTZK_STYLE_TEST_RESULT")) {
    const QString module=QStringLiteral("\u5730\u56fe\u6210\u679c\u8d28\u68c0");
    window.*access(CurrentModuleTag{})=module;
    if (!(window.*access(RegisterResultTag{}))(layer,module)) { printf("FAIL quality layer registration\n"); return 4; }
 }
 static_cast<QgsSingleSymbolRenderer*>(layer->renderer())->symbol()->setColor(Qt::red); static_cast<QgsLineSymbol*>(static_cast<QgsSingleSymbolRenderer*>(layer->renderer())->symbol())->setWidth(3); QgsProject::instance()->addMapLayer(layer,false);
 auto *group=QgsProject::instance()->layerTreeRoot()->addGroup("quality results");
 auto *severity=group->addGroup("high severity"); severity->addLayer(layer);
 QgsRectangle testExtent=source.isEmpty()?QgsRectangle(-1,-1,11,11):layer->extent(); testExtent.scale(1.1);
 auto* view=window.layerTreeView(); auto* node=QgsProject::instance()->layerTreeRoot()->findLayer(layer->id()); auto index=view->layerTreeModel()->node2index(node); view->setCurrentLayer(layer);
 auto* canvas=window.mapCanvas(); canvas->setParent(nullptr); canvas->resize(240,240); canvas->setExtent(testExtent); canvas->show(); spin(1200); canvas->setDestinationCrs(layer->crs()); canvas->setExtent(testExtent); canvas->refresh(); spin(1000);
 const QImage initial=canvas->grab().toImage(); const int beforeRed=countColor(initial,false); printf("initial red pixels=%d in canvas %d\n",beforeRed,canvas->layers().contains(layer));
 view->setParent(nullptr); view->resize(400,600); view->show(); view->expandAll(); view->scrollTo(view->currentIndex()); spin(200);
 index=view->currentIndex(); const QPoint pos=view->visualRect(index).center(); printf("pos %d %d indexvalid %d hitvalid %d layer=%s\n",pos.x(),pos.y(),index.isValid(),view->indexAt(pos).isValid(), qPrintable(view->currentLayer()?view->currentLayer()->name():"none"));
 bool opened=false; bool applied=false; QColor after; int afterRed=0, afterBlue=0;
 QTimer::singleShot(300,[&](){for(QWidget* w:QApplication::topLevelWidgets()) if(auto* menu=qobject_cast<QMenu*>(w)) if(menu->isVisible()){for(QAction* action:menu->actions()) if(action->menu()){for(QAction* item:action->menu()->actions()) if(item->text().contains(QStringLiteral("\u56fe\u5c42\u5c5e\u6027\u4e0e\u6837\u5f0f"))){opened=true; QTimer::singleShot(700,[&](){auto* d=window.findChild<QgsVectorLayerProperties*>(); if(!d)return; printf("dialog %s selected %s\n",qPrintable(d->windowTitle()), qPrintable(view->currentLayer()?view->currentLayer()->name():"none")); d->setCurrentPage("mOptsPage_Style"); spin(100); d->resize(1000,800); spin(100); d->grab().save("layer_style_host_build/properties_style.png"); if(auto*g=d->findChild<QGroupBox*>("mLayerRenderingGroupBox")) printf("native header initialized=%d marked=%d stylelen=%d font=%d height=%d\n",d->property("ltzkWindowInitialized").toBool(),g->property("ltzkPluginNativeHeader").toBool(),g->styleSheet().size(),g->font().pixelSize(),g->height()); d->setCurrentPage("mOptsPage_Information"); spin(200); d->grab().save("layer_style_host_build/properties_information.png"); d->setCurrentPage("mOptsPage_Style"); spin(200); auto sel=d->findChildren<QgsSymbolSelectorWidget*>(); QDialogButtonBox* box=nullptr;for(auto* b:d->findChildren<QDialogButtonBox*>("buttonBox"))if(b->button(QDialogButtonBox::Apply))box=b; if(!box||sel.isEmpty()||!sel.first()->symbol()){d->reject();return;} sel.first()->symbol()->setColor(Qt::blue);sel.first()->symbolModified(); box->button(qEnvironmentVariableIsSet("LTZK_STYLE_TEST_OK") ? QDialogButtonBox::Ok : QDialogButtonBox::Apply)->click(); spin(1200); const QImage image=canvas->grab().toImage(); after=image.pixelColor(120,120); afterRed=countColor(image,false); afterBlue=countColor(image,true); printf("after renderer=%s canvas layers=%d contains=%d frozen=%d\n",qPrintable(static_cast<QgsSingleSymbolRenderer*>(layer->renderer())->symbol()->color().name()),canvas->layers().size(),canvas->layers().contains(layer),canvas->isFrozen());applied=true; d->reject();}); item->trigger(); menu->close(); return;}}}});
 QTimer::singleShot(6000,[&](){for(QWidget* w:QApplication::topLevelWidgets()) if(auto* m=qobject_cast<QMenu*>(w))m->close();});
 QMetaObject::invokeMethod(view,"customContextMenuRequested",Qt::DirectConnection,Q_ARG(QPoint,pos));
 bool pass=opened&&applied&&beforeRed>10&&afterBlue>10&&afterRed<beforeRed/2;
 printf("%s real host context menu apply: opened=%d applied=%d red before=%d after=%d blue after=%d\n",pass?"PASS":"FAIL",opened,applied,beforeRed,afterRed,afterBlue);
 node->setItemVisibilityChecked(false); spin(600); const bool hidden=!canvas->layers().contains(layer);
 node->setItemVisibilityChecked(true); spin(600); const bool shown=canvas->layers().contains(layer);
 const int blueAfterToggle=countColor(canvas->grab().toImage(),true);
 const bool visibilityPass=hidden&&shown&&blueAfterToggle>10;
 printf("%s visibility toggle: hidden=%d shown=%d blue=%d\n",visibilityPass?"PASS":"FAIL",hidden,shown,blueAfterToggle);
 canvas->setParent(&window); view->setParent(&window); return pass&&visibilityPass?0:1;
}
