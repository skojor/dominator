#include "dishsettingsdialog.h"

#include "dishsettings.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>

DishSettingsDialog::DishSettingsDialog(DishSettings *settings, QWidget *parent)
    : QDialog(parent)
    , settings_(settings)
    , tcpPortInput_(new QSpinBox(this))
    , usernameInput_(new QLineEdit(settings->username(), this))
    , passwordInput_(new QLineEdit(this))
{
    setWindowTitle(tr("Dish connection settings"));
    setModal(true);

    tcpPortInput_->setRange(1, 65535);
    tcpPortInput_->setValue(settings_->tcpPort());
    passwordInput_->setEchoMode(QLineEdit::Password);

    QString password;
    QString error;
    if (!settings_->loadPassword(password, error)) {
        QMessageBox::warning(this, tr("Saved password unavailable"), error);
    } else {
        passwordInput_->setText(password);
    }

    auto *formLayout = new QFormLayout;
    formLayout->addRow(tr("TCP port:"), tcpPortInput_);
    formLayout->addRow(tr("Username:"), usernameInput_);
    formLayout->addRow(tr("Password:"), passwordInput_);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save
                                               | QDialogButtonBox::Cancel,
                                           this);
    connect(buttonBox, &QDialogButtonBox::accepted,
            this, &DishSettingsDialog::saveSettings);
    connect(buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(formLayout);
    layout->addWidget(buttonBox);
}

void DishSettingsDialog::saveSettings()
{
    QString error;
    if (!settings_->save(tcpPortInput_->value(), usernameInput_->text(),
                         passwordInput_->text(), error)) {
        QMessageBox::critical(this, tr("Could not save settings"), error);
        return;
    }

    accept();
}
