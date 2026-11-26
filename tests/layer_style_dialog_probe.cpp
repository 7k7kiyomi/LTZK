#include <qgsapplication.h>
#include <qgsgui.h>
#include <qgseditorwidgetregistry.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsgeometry.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgsvectorlayerproperties.h>
#include <qgsmapcanvas.h>
#include <qgsmessagebar.h>
#include <qgssymbolselectordialog.h>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>
#include "../WorkspaceLayerStyleRefresh.h"
static void process(int ms) { QElapsedTimer t; t.start(); while(t.elapsed()<ms) { QCoreApplication::processEvents(); QThread::msleep(5); } }
int main(int argc,char** argv) {
 QgsApplication app(argc,argv,true); QgsApplication::setPrefixPath("E:/extern/QGIS_32815",true); QgsApplication::initQgis(); QgsGui::instance(); QgsGui::editorWidgetRegistry()->initEditors();
 QgsVectorLayer layer("Polygon?crs=EPSG:4326","road","memory"); QgsFeature f(layer.fields()); f.setGeometry(QgsGeometry::fromWkt("POLYGON ((0 0,10 0,10 10,0 10,0 0))")); layer.dataProvider()->addFeatures(QgsFeatureList()<<f); layer.updateExtents();
 static_cast<QgsSingleSymbolRenderer*>(layer.renderer())->symbol()->setColor(Qt::red);
 QgsMapCanvas canvas; canvas.resize(240,240); canvas.setCachingEnabled(true); canvas.setLayers({&layer}); canvas.setExtent(QgsRectangle(-1,-1,11,11)); canvas.show(); canvas.refresh(); process(1000);
 QgsMessageBar bar; QElapsedTimer t; t.start(); QgsVectorLayerProperties dialog(&canvas,&bar,&layer); printf("construct %lld ms\n",t.elapsed());
 auto boxes=dialog.findChildren<QDialogButtonBox*>("buttonBox"); QDialogButtonBox* actual=nullptr; for(auto *box:boxes) if(box->button(QDialogButtonBox::Apply)&&box->button(QDialogButtonBox::Ok)) actual=box;
 if (!actual || boxes.size()<2) { printf("FAIL actual property buttons unavailable\n"); return 2; }
 for(auto* b:boxes) printf("box parent %s apply %p ok %p enabled %d\n", qPrintable(b->parent()->objectName()), b->button(QDialogButtonBox::Apply), b->button(QDialogButtonBox::Ok), b->button(QDialogButtonBox::Apply)?b->button(QDialogButtonBox::Apply)->isEnabled():0);
 connectLayerStyleRefresh(&dialog,&canvas);
 dialog.setCurrentPage("mOptsPage_Style"); dialog.show(); process(200);
 auto selectors=dialog.findChildren<QgsSymbolSelectorWidget*>(); printf("selectors %d\n",selectors.size());
 if(selectors.isEmpty() || !selectors.first()->symbol()) return 3;
 selectors.first()->symbol()->setColor(Qt::blue); selectors.first()->symbolModified();
 actual->button(QDialogButtonBox::Apply)->setEnabled(true); actual->button(QDialogButtonBox::Apply)->click(); process(1000);
 QColor color=canvas.grab().toImage().pixelColor(120,120); bool pass=color.blue()>color.red(); printf("%s real dialog Apply refresh: %s, boxes=%d\n",pass?"PASS":"FAIL",qPrintable(color.name()),boxes.size());
 selectors.first()->symbol()->setColor(Qt::green); selectors.first()->symbolModified(); actual->button(QDialogButtonBox::Ok)->click(); process(1000);
 const QColor ok=canvas.grab().toImage().pixelColor(120,120); const bool okPass=ok.green()>ok.blue() && ok.green()>ok.red(); printf("%s real dialog OK refresh: %s\n",okPass?"PASS":"FAIL",qPrintable(ok.name()));
 QgsVectorLayerProperties cancelled(&canvas,&bar,&layer); cancelled.setCurrentPage("mOptsPage_Style"); cancelled.show(); process(200);
 auto cancelSelectors=cancelled.findChildren<QgsSymbolSelectorWidget*>(); QDialogButtonBox* cancelBox=nullptr; for(auto* b:cancelled.findChildren<QDialogButtonBox*>("buttonBox")) if(b->button(QDialogButtonBox::Apply)&&b->button(QDialogButtonBox::Cancel)) cancelBox=b;
 if(cancelSelectors.isEmpty()||!cancelBox) return 4; cancelSelectors.first()->symbol()->setColor(Qt::red); cancelSelectors.first()->symbolModified(); cancelBox->button(QDialogButtonBox::Cancel)->click(); process(500);
 const QColor cancel=canvas.grab().toImage().pixelColor(120,120); const bool cancelPass=cancel.green()>cancel.red(); printf("%s real dialog Cancel retains applied style: %s\n",cancelPass?"PASS":"FAIL",qPrintable(cancel.name())); return pass&&okPass&&cancelPass?0:1;
}
