#include "elevationindicator.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QSizePolicy>

#include <cmath>

namespace {
constexpr double Pi = 3.14159265358979323846;

QPointF elevationVector(double elevationDegrees)
{
    const double radians = elevationDegrees * Pi / 180.0;
    return QPointF(std::cos(radians), -std::sin(radians));
}
}

ElevationIndicator::ElevationIndicator(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(250, 250);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setAccessibleName(tr("Dish elevation side view"));
    setAccessibleDescription(
        tr("Side view of dish elevation from 0 degrees at the horizon to 90 degrees overhead."));
}

bool ElevationIndicator::setElevationDegrees(double elevationDegrees)
{
    if (!std::isfinite(elevationDegrees))
        return false;

    elevationDegrees_ = elevationDegrees;
    hasElevation_ = true;
    update();
    return true;
}

bool ElevationIndicator::hasElevation() const
{
    return hasElevation_;
}

double ElevationIndicator::elevationDegrees() const
{
    return elevationDegrees_;
}

QSize ElevationIndicator::sizeHint() const
{
    return QSize(320, 320);
}

void ElevationIndicator::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF bounds = rect().adjusted(1.0, 1.0, -1.0, -1.0);
    const QColor background = palette().color(QPalette::Base);
    const QColor foreground = palette().color(QPalette::Text);
    const QColor muted = palette().color(QPalette::Mid);
    const QColor accent = palette().color(QPalette::Highlight);

    painter.setPen(QPen(muted, 1.0));
    painter.setBrush(background);
    painter.drawRoundedRect(bounds, 8.0, 8.0);

    const QPointF pivot(bounds.left() + bounds.width() * 0.22,
                        bounds.bottom() - bounds.height() * 0.20);
    const double radius = qMax(1.0, qMin(bounds.width() * 0.68,
                                         bounds.height() * 0.66));

    QPainterPath elevationArc;
    for (int degree = 0; degree <= 90; ++degree) {
        const QPointF point = pivot + elevationVector(degree) * radius;
        if (degree == 0)
            elevationArc.moveTo(point);
        else
            elevationArc.lineTo(point);
    }

    painter.setPen(QPen(muted, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(elevationArc);

    const double groundEnd = bounds.right() - 12.0;
    painter.setPen(QPen(muted, 2.0));
    painter.drawLine(QPointF(bounds.left() + 10.0, pivot.y()),
                     QPointF(groundEnd, pivot.y()));

    for (int degree = 0; degree <= 90; degree += 5) {
        const QPointF direction = elevationVector(degree);
        const bool major = degree % 10 == 0;
        const double innerRadius = radius - (major ? 12.0 : 6.0);
        painter.setPen(QPen(major ? foreground : muted, major ? 1.4 : 0.8));
        painter.drawLine(pivot + direction * radius,
                         pivot + direction * innerRadius);
    }

    QFont labelFont = painter.font();
    labelFont.setPointSizeF(qMax(7.0, labelFont.pointSizeF() - 1.0));
    painter.setFont(labelFont);
    painter.setPen(muted);
    for (int degree = 0; degree <= 90; degree += 10) {
        const QPointF position =
            pivot + elevationVector(degree) * (radius + 14.0);
        const QRectF textBounds(position.x() - 15.0, position.y() - 8.0,
                                30.0, 16.0);
        painter.drawText(textBounds, Qt::AlignCenter,
                         QStringLiteral("%1°").arg(degree));
    }

    const QPointF pedestalTop(pivot.x(), pivot.y() - 8.0);
    painter.setPen(QPen(muted, 1.5));
    painter.drawLine(pedestalTop, QPointF(pivot.x() - 9.0, pivot.y() + 2.0));
    painter.drawLine(pedestalTop, QPointF(pivot.x() + 9.0, pivot.y() + 2.0));
    painter.drawLine(QPointF(pivot.x() - 12.0, pivot.y() + 3.0),
                     QPointF(pivot.x() + 12.0, pivot.y() + 3.0));

    if (hasElevation_ && elevationDegrees_ >= 0.0 && elevationDegrees_ <= 90.0) {
        const QPointF direction = elevationVector(elevationDegrees_);
        const QPointF start = pivot + direction * 13.0;
        const QPointF tip = pivot + direction * (radius * 0.82);
        const QPointF perpendicular(-direction.y(), direction.x());
        const QPointF arrowBase = tip - direction * 11.0;

        QPainterPath arrow;
        arrow.moveTo(tip);
        arrow.lineTo(arrowBase + perpendicular * 4.5);
        arrow.lineTo(arrowBase - perpendicular * 4.5);
        arrow.closeSubpath();

        painter.setPen(QPen(accent, 3.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(start, tip - direction * 2.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawPath(arrow);

        painter.setPen(QPen(background, 1.0));
        painter.setBrush(accent);
        painter.drawEllipse(pivot, 3.5, 3.5);
    } else {
        painter.setPen(QPen(muted, 2.0));
        painter.setBrush(muted);
        painter.drawEllipse(pivot, 3.0, 3.0);
    }

    QFont rangeFont = painter.font();
    rangeFont.setBold(true);
    painter.setFont(rangeFont);
    painter.setPen(hasElevation_ && (elevationDegrees_ < 0.0 || elevationDegrees_ > 90.0)
                       ? palette().color(QPalette::Highlight)
                       : foreground);
    const QString readout = !hasElevation_
                                ? tr("Waiting for position")
                                : elevationDegrees_ < 0.0 || elevationDegrees_ > 90.0
                                      ? tr("Out of range: %1°")
                                            .arg(elevationDegrees_, 0, 'f', 1)
                                      : tr("%1°").arg(elevationDegrees_, 0, 'f', 1);
    const QRectF readoutBounds(bounds.left() + 8.0, pivot.y() + 8.0,
                               bounds.width() - 16.0,
                               bounds.bottom() - pivot.y() - 12.0);
    painter.drawText(readoutBounds, Qt::AlignCenter, readout);
}
