#include "../LTZK.h"
#include "../auth/LTZKAuthManager.h"
#include <QTabWidget>
#include <QTimer>
#include <qgsapplication.h>
#include <qgsgui.h>
#include <qgseditorwidgetregistry.h>
#include <cstdio>
template<typename Tag, typename Tag::type Member> struct MemberAccess { friend typename Tag::type access(Tag) { return Member; } };
struct ActivateTag { using type = void (LTZK::*)(const QString&); friend type access(ActivateTag); };
template struct MemberAccess<ActivateTag, &LTZK::activateBusinessModule>;
static bool check(bool condition,const char* message) { std::printf("%s %s\n",condition?"PASS":"FAIL",message); return condition; }
int main(int argc,char** argv) {
 QgsApplication app(argc,argv,true); QgsApplication::setPrefixPath("E:/extern/QGIS_32815",true); QgsApplication::initQgis(); QgsGui::instance(); QgsGui::editorWidgetRegistry()->initEditors();
 LTZK window; for(auto* timer:window.findChildren<QTimer*>()) timer->stop();
 auto* tabs=window.findChild<QTabWidget*>("workspaceSideTabs"); if(!tabs || tabs->count()<2) return 2;
 QWidget* data=tabs->widget(0); QWidget* layers=tabs->widget(1); tabs->setCurrentWidget(layers);
 gCurrentUserSession.isGuest=true; bool pass=true;
 auto activate=[&](const QString& title){ (window.*access(ActivateTag{}))(title); };
 const QString manage=QStringLiteral("\u5730\u56fe\u6210\u679c\u7ba1\u7406");
 const QString integrated=QStringLiteral("\u5730\u56fe\u667a\u80fd\u7efc\u5408");
 const QString quality=QStringLiteral("\u5730\u56fe\u6210\u679c\u8d28\u68c0");
 activate(manage); pass=check(tabs->currentWidget()==data,"product management opens data list")&&pass;
 tabs->setCurrentWidget(layers); pass=check(tabs->currentWidget()==layers,"manual layer selection remains available")&&pass;
 activate(integrated); pass=check(tabs->currentWidget()==layers,"other visible-catalog module retains layer selection")&&pass;
 activate(manage); pass=check(tabs->currentWidget()==data,"returning to product management opens data list")&&pass;
 activate(quality); pass=check(tabs->currentWidget()==layers && !tabs->isTabVisible(tabs->indexOf(data)),"quality module hides catalog and displays layers")&&pass;
 gCurrentUserSession.clear(); return pass?0:1;
}
