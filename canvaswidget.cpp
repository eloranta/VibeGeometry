#include "canvaswidget.h"

#include <QPainter>
#include <QtMath>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include <algorithm>

CanvasWidget::CanvasWidget(const QString &storagePath, QWidget *parent)
    : QWidget(parent),
      storagePath_(storagePath) {
    setMinimumSize(320, 240);
    loadPointsFromFile();
}

bool CanvasWidget::addPoint(const QPointF &point, const QString &label) {
    if (hasPoint(point)) {
        return false;
    }
    points_.append({point, label});
    savePointsToFile();
    update();
    return true;
}

bool CanvasWidget::hasPoint(const QPointF &point) const {
    for (const auto &p : points_) {
        if (qFuzzyCompare(p.pos.x(), point.x()) && qFuzzyCompare(p.pos.y(), point.y())) {
            return true;
        }
    }
    return false;
}

int CanvasWidget::pointCount() const {
    return points_.size();
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

    painter.setBrush(Qt::red);
    painter.setPen(QPen(Qt::red, 2));
    const double radiusPixels = 4.0;
    painter.setFont([&]{
        QFont f = painter.font();
        f.setPointSizeF(9.0);
        return f;
    }());
    for (const auto &entry : points_) {
        QPointF mapped = map(entry.pos.x(), entry.pos.y());
        painter.drawEllipse(mapped, radiusPixels, radiusPixels);
        painter.setPen(Qt::black);
        painter.drawText(mapped + QPointF(6, -6), entry.label);
        painter.setPen(QPen(Qt::red, 2));
    }
}

void CanvasWidget::loadPointsFromFile() {
    if (storagePath_.isEmpty()) {
        return;
    }
    QFile file(storagePath_);
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const auto data = file.readAll();
    file.close();
    auto doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return;
    }
    points_.clear();
    for (const auto &value : doc.array()) {
        if (!value.isObject()) continue;
        const auto obj = value.toObject();
        double x = obj.value("x").toDouble();
        double y = obj.value("y").toDouble();
        QString label = obj.value("label").toString();
        if (label.isEmpty()) label = QStringLiteral("P");
        points_.append({QPointF(x, y), label});
    }
}

void CanvasWidget::savePointsToFile() const {
    if (storagePath_.isEmpty()) {
        return;
    }
    QJsonArray arr;
    for (const auto &entry : points_) {
        QJsonObject obj;
        obj.insert("x", entry.pos.x());
        obj.insert("y", entry.pos.y());
        obj.insert("label", entry.label);
        arr.append(obj);
    }
    QJsonDocument doc(arr);

    QDir().mkpath(QFileInfo(storagePath_).absolutePath());
    QFile file(storagePath_);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}
