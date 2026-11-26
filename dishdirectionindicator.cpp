#include "dishdirectionindicator.h"

#include <QColor>
#include <QFont>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QRadialGradient>
#include <QSizePolicy>

#include <cmath>

namespace {
constexpr double Pi = 3.14159265358979323846;

QPointF bearingVector(double azimuthDegrees)
{
    const double radians = azimuthDegrees * Pi / 180.0;
    return QPointF(std::sin(radians), -std::cos(radians));
}
}

DishDirectionIndicator::DishDirectionIndicator(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(250, 250);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setAccessibleName(tr("Dish direction compass"));
    setAccessibleDescription(
        tr("Top view compass showing dish azimuth measured clockwise from true north."));
}

bool DishDirectionIndicator::setAzimuthDegrees(double azimuthDegrees)
{
    if (!std::isfinite(azimuthDegrees))
        return false;

    azimuthDegrees_ = std::fmod(azimuthDegrees, 360.0);
    if (azimuthDegrees_ < 0.0)
        azimuthDegrees_ += 360.0;
    hasAzimuth_ = true;
    update();
    return true;
}

void DishDirectionIndicator::clearAzimuth()
{
    hasAzimuth_ = false;
    update();
}

bool DishDirectionIndicator::hasAzimuth() const
{
    return hasAzimuth_;
}

double DishDirectionIndicator::azimuthDegrees() const
{
    return azimuthDegrees_;
}

QSize DishDirectionIndicator::sizeHint() const
{
    return QSize(320, 320);
}

void DishDirectionIndicator::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF bounds = rect().adjusted(1.0, 1.0, -1.0, -1.0);
    const QColor background = palette().color(QPalette::Base);
    const QColor foreground = palette().color(QPalette::Text);
    const QColor muted = palette().color(QPalette::Mid);
    const QColor accent = palette().color(QPalette::Highlight);

    painter.setPen(QPen(palette().color(QPalette::Mid), 1.0));
    painter.setBrush(background);
    painter.drawRoundedRect(bounds, 8.0, 8.0);

    const QPointF center(bounds.center().x(), bounds.center().y() + 2.0);
    const double radius = qMax(1.0, qMin(bounds.width(), bounds.height()) * 0.34);

    painter.setPen(QPen(muted, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius, radius);
    painter.drawEllipse(center, radius * 0.76, radius * 0.76);

    for (int degree = 0; degree < 360; degree += 5) {
        const QPointF direction = bearingVector(degree);
        const bool cardinal = degree % 90 == 0;
        const bool major = degree % 30 == 0;
        const double innerRadius = radius - (cardinal ? 15.0 : major ? 11.0 : 6.0);

        painter.setPen(QPen(cardinal ? foreground : muted,
                            cardinal ? 2.0 : major ? 1.3 : 0.8));
        painter.drawLine(center + direction * radius,
                         center + direction * innerRadius);
    }

    QFont degreeFont = painter.font();
    degreeFont.setPointSizeF(qMax(7.0, degreeFont.pointSizeF() - 1.0));
    painter.setFont(degreeFont);
    painter.setPen(muted);
    for (int degree = 30; degree < 360; degree += 30) {
        if (degree % 90 == 0)
            continue;

        const QPointF position = center + bearingVector(degree) * (radius + 14.0);
        const QRectF textBounds(position.x() - 18.0, position.y() - 8.0, 36.0, 16.0);
        painter.drawText(textBounds, Qt::AlignCenter,
                         QStringLiteral("%1°").arg(degree, 3, 10, QLatin1Char('0')));
    }

    QFont cardinalFont = painter.font();
    cardinalFont.setBold(true);
    cardinalFont.setPointSizeF(cardinalFont.pointSizeF() + 1.0);
    painter.setFont(cardinalFont);
    painter.setPen(foreground);
    const QList<QPair<int, QString>> cardinals{
        {0, tr("N")}, {90, tr("E")}, {180, tr("S")}, {270, tr("W")}
    };
    for (const auto &cardinal : cardinals) {
        const QPointF position = center + bearingVector(cardinal.first) * (radius + 34.0);
        const QRectF textBounds(position.x() - 12.0, position.y() - 11.0, 24.0, 22.0);
        painter.drawText(textBounds, Qt::AlignCenter, cardinal.second);
    }

    const double dishRadius = radius * 0.22;
    QRadialGradient dishGradient(center - QPointF(dishRadius * 0.35, dishRadius * 0.4),
                                 dishRadius * 2.2);
    dishGradient.setColorAt(0.0, background.lighter(145));
    dishGradient.setColorAt(1.0, background.darker(118));
    painter.setPen(QPen(muted, 1.5));
    painter.setBrush(dishGradient);
    painter.drawEllipse(center, dishRadius, dishRadius);

    if (hasAzimuth_) {
        const QPointF direction = bearingVector(azimuthDegrees_);
        const QPointF start = center + direction * (dishRadius * 0.8);
        const QPointF tip = center + direction * (radius * 0.72);
        const QPointF perpendicular(-direction.y(), direction.x());
        const QPointF base = tip - direction * 12.0;
        QPainterPath arrow;
        arrow.moveTo(tip);
        arrow.lineTo(base + perpendicular * 5.0);
        arrow.lineTo(base - perpendicular * 5.0);
        arrow.closeSubpath();

        painter.setPen(QPen(accent, 3.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(start, tip - direction * 2.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawPath(arrow);

        painter.setPen(QPen(background, 1.0));
        painter.setBrush(accent);
        painter.drawEllipse(center, 3.5, 3.5);
    } else {
        painter.setPen(QPen(muted, 2.0));
        painter.setBrush(muted);
        painter.drawEllipse(center, 3.0, 3.0);
    }
}
