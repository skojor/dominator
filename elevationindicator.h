#ifndef ELEVATIONINDICATOR_H
#define ELEVATIONINDICATOR_H

#include <QWidget>

class ElevationIndicator : public QWidget
{
    Q_OBJECT

public:
    explicit ElevationIndicator(QWidget *parent = nullptr);

    bool setElevationDegrees(double elevationDegrees);
    bool hasElevation() const;
    double elevationDegrees() const;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool hasElevation_ = false;
    double elevationDegrees_ = 0.0;
};

#endif // ELEVATIONINDICATOR_H
