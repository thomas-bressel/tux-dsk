#include "icons.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPolygonF>

namespace tuxdsk {

QIcon discIcon()
{
    const QColor ink = QApplication::palette().color(QPalette::WindowText);
    // On a grid of 20, at twice the size a button shows it.
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(2, 2);
    QPen pen(ink, 1.5);
    pen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(pen);
    // The case, a corner cut off; the shutter at the top and the label
    // under it.
    painter.drawPolygon(QPolygonF({{3.5, 2.5}, {14.0, 2.5}, {16.5, 5.0}, {16.5, 17.5}, {3.5, 17.5}}));
    painter.drawRect(QRectF(6.5, 2.5, 6.0, 4.5));
    painter.drawRect(QRectF(6.0, 10.5, 8.0, 7.0));
    // Two lines on the label: the files it lists.
    painter.drawLine(QPointF(8.0, 13.0), QPointF(12.0, 13.0));
    painter.drawLine(QPointF(8.0, 15.2), QPointF(12.0, 15.2));
    painter.setBrush(ink);
    painter.setPen(Qt::NoPen);
    painter.drawRect(QRectF(10.0, 3.5, 1.5, 2.5));
    return QIcon(pixmap);
}

}  // namespace tuxdsk
