#include "../LTZK.h"
#include <qgsapplication.h>
#include <qgsgui.h>
#include <qgseditorwidgetregistry.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerproperties.h>
#include <qgsmapcanvas.h>
#include <qgsmessagebar.h>
#include <QElapsedTimer>
#include <QTimer>
#include <QThread>
#include <cstdio>
static void events(int ms) { QElapsedTimer t;t.start(); while(t.elapsed()<ms) {QCoreApplication::processEvents(); QThread::msleep(2);} }
int main(int argc,char** argv) {
 QgsApplication app(argc,argv,true); QgsApplication::setPrefixPath("E:/extern/QGIS_32815",true); QgsApplication::initQgis(); QgsGui::instance(); QgsGui::editorWidgetRegistry()->initEditors();
 LTZK window; for(auto* timer:window.findChildren<QTimer*>())timer->stop();
 const QString source=qEnvironmentVariable("LTZK_STYLE_TEST_SOURCE"); auto* layer=new QgsVectorLayer(source,"county road","ogr"); if(!layer->isValid()) return 2;
 QgsProject::instance()->addMapLayer(layer);
 for(int i=0;i<25;++i){auto* copy=static_cast<QgsVectorLayer*>(layer->clone());copy->setName(QString("result %1").arg(i));QgsProject::instance()->addMapLayer(copy);}
 events(100);
 if(qEnvironmentVariableIsSet("LTZK_BENCH_NO_FILTER")) qApp->removeEventFilter(&window);
 if(qEnvironmentVariableIsSet("LTZK_BENCH_NO_QSS")) qApp->setStyleSheet("");
 qint64 bestPostShow=999999;
 for(int i=0;i<3;++i){ QElapsedTimer t;t.start(); if(qEnvironmentVariableIsSet("LTZK_BENCH_DETACH_CONSTRUCT")) qApp->removeEventFilter(&window); auto* dialog=new QgsVectorLayerProperties(window.mapCanvas(),window.messageBarWidget(),layer,&window); if(qEnvironmentVariableIsSet("LTZK_BENCH_DETACH_CONSTRUCT")) qApp->installEventFilter(&window); qint64 construct=t.elapsed(); dialog->setCurrentPage("symbology");dialog->show();qint64 show=t.elapsed();events(50);qint64 ready=t.elapsed(); printf("round %d construct=%lld show=%lld ready=%lld widgets=%d\n",i+1,construct,show,ready,dialog->findChildren<QWidget*>().size()); if(i>0) bestPostShow=qMin(bestPostShow,ready-show);fflush(stdout);dialog->reject();delete dialog;events(20); }
 printf("%s repeated post-show processing %lld ms (limit 450 ms)\n",bestPostShow<=450?"PASS":"FAIL",bestPostShow); return bestPostShow<=450?0:1;
}
