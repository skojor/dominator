#ifndef SATELLITEPREVIEWDIALOG_H
#define SATELLITEPREVIEWDIALOG_H

#include "n2yoclient.h"

#include <QDialog>
#include <QTimer>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QWidget;
class DishSettings;

class SatellitePreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SatellitePreviewDialog(DishSettings *settings, QWidget *parent = nullptr);
    void setActualPosition(double azimuthDegrees, double elevationDegrees);
    void clearActualPosition();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void startPreview();
    void stopPreview();
    void fetch();
    void showPointing();

    DishSettings *settings_;
    N2yoClient client_;
    SatellitePrediction prediction_;
    QTimer refreshTimer_;
    QTimer displayTimer_;
    QWidget *inputs_;
    QLineEdit *catalogId_;
    QLineEdit *apiKey_;
    QDoubleSpinBox *latitude_;
    QDoubleSpinBox *longitude_;
    QDoubleSpinBox *altitude_;
    QPushButton *start_;
    QPushButton *stop_;
    QLabel *status_;
    QLabel *pointing_;
    QLabel *actualPointing_;
};

#endif // SATELLITEPREVIEWDIALOG_H
