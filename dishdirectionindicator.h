#ifndef DISHDIRECTIONINDICATOR_H
#define DISHDIRECTIONINDICATOR_H

#include <QWidget>

class DishDirectionIndicator : public QWidget
{
    Q_OBJECT

public:
    explicit DishDirectionIndicator(QWidget *parent = nullptr);

    bool setAzimuthDegrees(double azimuthDegrees);
    bool hasAzimuth() const;
    double azimuthDegrees() const;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool hasAzimuth_ = false;
    double azimuthDegrees_ = 0.0;
};

#endif // DISHDIRECTIONINDICATOR_H
