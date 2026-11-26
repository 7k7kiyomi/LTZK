#pragma once

#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QPointer>
#include <QStackedWidget>
#include <QVBoxLayout>

// Owns the workspace content only; the platform keeps its common top bar.
class MapServiceWorkspace : public QStackedWidget
{
public:
    explicit MapServiceWorkspace(QWidget* mapControls, QWidget* parent = nullptr)
        : QStackedWidget(parent)
        , m_mapControls(mapControls)
        , m_servicePage(new QWidget(this))
    {
        setObjectName(QStringLiteral("workspaceContentStack"));
        addWidget(mapControls);
        m_servicePage->setObjectName(QStringLiteral("mapServiceWorkspacePage"));
        QVBoxLayout* layout = new QVBoxLayout(m_servicePage);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        m_openHint = new QLabel(QStringLiteral("请点击上方“地图服务管理”打开服务界面。"), m_servicePage);
        m_openHint->setAlignment(Qt::AlignCenter);
        layout->addWidget(m_openHint);
        addWidget(m_servicePage);
    }

    void setServiceMode(bool serviceMode)
    {
        setCurrentWidget(serviceMode ? m_servicePage : m_mapControls);
        if (serviceMode && m_dialog)
        {
            m_dialog->show();
        }
    }

    QDialog* serviceDialog() const { return m_dialog.data(); }

    bool embedServiceDialog(QDialog* dialog)
    {
        if (!dialog || (m_dialog && m_dialog != dialog))
        {
            return false;
        }
        if (m_dialog == dialog)
        {
            dialog->show();
            return true;
        }
        m_dialog = dialog;
        dialog->hide();
        dialog->setParent(m_servicePage, Qt::Widget);
        dialog->setAttribute(Qt::WA_DontShowOnScreen, false);
        dialog->setWindowModality(Qt::NonModal);
        dialog->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        dialog->installEventFilter(this);
        m_servicePage->layout()->addWidget(dialog);
        m_openHint->hide();
        // The hint can be destroyed before the dialog during host teardown.
        // Use the hint as connection context so Qt disconnects it first.
        connect(dialog, &QObject::destroyed, m_openHint,
            [hint = m_openHint]() { hint->show(); });
        dialog->show();
        return true;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        // Esc must not dismiss the subsystem's main content as a popup dialog.
        if (watched == m_dialog && event->type() == QEvent::KeyPress
            && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)
        {
            return true;
        }
        return QStackedWidget::eventFilter(watched, event);
    }

private:
    QWidget* m_mapControls;
    QWidget* m_servicePage;
    QLabel* m_openHint;
    QPointer<QDialog> m_dialog;
};
