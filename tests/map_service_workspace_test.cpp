#include "../MapServiceWorkspace.h"

#include <QApplication>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QPushButton>
#include <cstdio>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL at line %d: %s\n", __LINE__, #condition); return 1; \
} } while (false)

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::puts("Starting Qt workspace test.");
    QApplication app(argc, argv);
    std::puts("Qt application ready.");
    QWidget* mapControls = new QWidget;
    MapServiceWorkspace workspace(mapControls);
    workspace.resize(1200, 800);
    workspace.show();
    app.processEvents();
    CHECK(mapControls->isVisible());

    workspace.setServiceMode(true);
    app.processEvents();
    CHECK(!mapControls->isVisible());

    QDialog* service = new QDialog;
    service->setObjectName(QStringLiteral("MapServiceDialog"));
    QVBoxLayout* layout = new QVBoxLayout(service);
    QPushButton* action = new QPushButton(QStringLiteral("Service records"), service);
    layout->addWidget(action);
    int clicks = 0;
    QObject::connect(action, &QPushButton::clicked, [&clicks]() { ++clicks; });
    service->setAttribute(Qt::WA_DontShowOnScreen, true);
    service->show();
    CHECK(workspace.embedServiceDialog(service));
    CHECK(!service->testAttribute(Qt::WA_DontShowOnScreen));
    app.processEvents();
    CHECK(workspace.serviceDialog() == service);
    CHECK(!service->isWindow());
    CHECK(service->isVisible());
    CHECK(!mapControls->isVisible());
    CHECK(service->size() == workspace.currentWidget()->size());
    action->click();
    CHECK(clicks == 1);

    // Reopening the feature must reuse the same embedded dialog.
    CHECK(workspace.embedServiceDialog(service));
    CHECK(workspace.currentWidget()->findChildren<QDialog*>().size() == 1);
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(service, &escape);
    CHECK(service->isVisible());

    CHECK(service->close());
    workspace.setServiceMode(false);
    app.processEvents();
    CHECK(mapControls->isVisible());
    CHECK(!service->isVisible());
    workspace.setServiceMode(true);
    app.processEvents();
    CHECK(service->isVisible());
    CHECK(!mapControls->isVisible());
    workspace.resize(1500, 900);
    app.processEvents();
    CHECK(service->size() == workspace.currentWidget()->size());

    // Plugin unload deletes the dialog; the host must accept a fresh instance.
    delete service;
    CHECK(!workspace.serviceDialog());
    QDialog* replacement = new QDialog;
    CHECK(workspace.embedServiceDialog(replacement));
    app.processEvents();
    CHECK(replacement->isVisible());
    CHECK(!replacement->isWindow());
    std::puts("Map service workspace runtime checks passed.");
    return 0;
}
