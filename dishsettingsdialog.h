#ifndef DISHSETTINGSDIALOG_H
#define DISHSETTINGSDIALOG_H

#include <QDialog>

class DishSettings;
class QLineEdit;
class QSpinBox;

class DishSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DishSettingsDialog(DishSettings *settings, QWidget *parent = nullptr);

private slots:
    void saveSettings();

private:
    DishSettings *settings_;
    QSpinBox *tcpPortInput_;
    QLineEdit *usernameInput_;
    QLineEdit *passwordInput_;
};

#endif // DISHSETTINGSDIALOG_H
