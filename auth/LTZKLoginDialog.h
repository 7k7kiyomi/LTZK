#pragma once

#include <QDialog>

#include "LTZKAuthManager.h"

class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;
class QVBoxLayout;

class LTZKLoginDialog final : public QDialog
{
public:
    explicit LTZKLoginDialog(QWidget* parent = nullptr);

    UserSession session() const;

private:
    void resizeEvent(QResizeEvent* event) override;

    void authenticateUser();
    void updateAdaptiveLayout();
    double uiScale() const;
    int scaled(int value) const;

    QLabel* mTitleLabel = nullptr;
    QLineEdit* mUserNameEdit = nullptr;
    QLineEdit* mPasswordEdit = nullptr;
    QPushButton* mLoginButton = nullptr;
    QLabel* mStatusLabel = nullptr;
    QVBoxLayout* mMainLayout = nullptr;
    QGridLayout* mFormLayout = nullptr;
    QHBoxLayout* mButtonLayout = nullptr;
    UserSession mSession;
};
