#include "canvaswidget.h"

#include <QPainter>
#include <algorithm>

CanvasWidget::CanvasWidget(QWidget *parent)
    : QWidget(parent) {
    setMinimumSize(320, 240);
}

void CanvasWidget::addLine(const QPointF &start, const QPointF &end) {
    lines_.append(QLineF(start, end));
    update();
}

void CanvasWidget::addCircle(const QPointF &center, double radius) {
    circles_.append(qMakePair(center, radius));
    update();
}

void CanvasWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int padding = 16;
    QRectF area = rect().adjusted(padding, padding, -padding, -padding);
    const double span = 10.0;  // -5 to 5 on each axis
    const double scale = std::min(area.width(), area.height()) / span;
    QPointF origin(area.left() + area.width() / 2.0, area.top() + area.height() / 2.0);

    auto map = [&](double x, double y) -> QPointF {
        return QPointF(origin.x() + x * scale, origin.y() - y * scale);
    };

    QPen axisPen(Qt::black, 2);
    painter.setPen(axisPen);
    painter.drawLine(map(-5, 0), map(5, 0));  // X axis
    painter.drawLine(map(0, -5), map(0, 5));  // Y axis

    QPen tickPen(Qt::black, 1);
    painter.setPen(tickPen);
    const double tickSize = 6.0;
    for (int i = -5; i <= 5; ++i) {
        if (i == 0) continue;  // skip origin tick overlap
        // X ticks
        QPointF p1 = map(i, -tickSize / scale / 2.0);
        QPointF p2 = map(i, tickSize / scale / 2.0);
        painter.drawLine(p1, p2);
        // Y ticks
        QPointF p3 = map(-tickSize / scale / 2.0, i);
        QPointF p4 = map(tickSize / scale / 2.0, i);
        painter.drawLine(p3, p4);
    }

    painter.setPen(QPen(Qt::blue, 2));
    for (const auto &line : lines_) {
        painter.drawLine(map(line.p1().x(), line.p1().y()), map(line.p2().x(), line.p2().y()));
    }

    painter.setPen(QPen(Qt::darkGreen, 2));
    for (const auto &circle : circles_) {
        const QPointF &c = circle.first;
        double r = circle.second;
        QPointF topLeft = map(c.x() - r, c.y() + r);
        QPointF bottomRight = map(c.x() + r, c.y() - r);
        QRectF ellipseRect(topLeft, bottomRight);
        painter.drawEllipse(ellipseRect);
    }
}
