#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLabel;
class LocalIpcServer;
class PositionReporter;
class ScanController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void updateDishPosition(double azimuthDegrees, double elevationDegrees);

private:
    LocalIpcServer *ipcServer_ = nullptr;
    PositionReporter *positionReporter_ = nullptr;
    ScanController *scanController_ = nullptr;
    QLabel *ipcStatusLabel_ = nullptr;
    QLabel *scanStateLabel_ = nullptr;
    QLabel *azimuthLabel_ = nullptr;
    QLabel *elevationLabel_ = nullptr;
    QLabel *signalLevelLabel_ = nullptr;
};
#endif // MAINWINDOW_H
