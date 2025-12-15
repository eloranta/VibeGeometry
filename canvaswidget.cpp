#include "canvaswidget.h"

#include <QPainter>
#include <QtMath>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include <QMouseEvent>
#include <limits>
#include <algorithm>
#include <QList>

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

int CanvasWidget::selectedCount() const {
    return selectedIndices_.size();
}

bool CanvasWidget::addLineBetweenSelected() {
    if (selectedIndices_.size() < 2) {
        return false;
    }
    QList<int> indices = selectedIndices_.values();
    std::sort(indices.begin(), indices.end());
    int a = indices[0];
    int b = indices[1];
    if (a == b) return false;

    // Avoid duplicates (order-insensitive)
    for (const auto &line : lines_) {
        if ((line.a == a && line.b == b) || (line.a == b && line.b == a)) {
            return false;
        }
    }
    lines_.append({a, b});
    savePointsToFile();
    update();
    return true;
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
        if (line.a < 0 || line.b < 0 || line.a >= points_.size() || line.b >= points_.size()) continue;
        QPointF p1 = points_[line.a].pos;
        QPointF p2 = points_[line.b].pos;
        painter.drawLine(map(p1.x(), p1.y()), map(p2.x(), p2.y()));
    }

    const double radiusPixels = 4.0;
    painter.setFont([&]{
        QFont f = painter.font();
        f.setPointSizeF(9.0);
        return f;
    }());
    for (int i = 0; i < points_.size(); ++i) {
        const auto &entry = points_[i];
        QPointF mapped = map(entry.pos.x(), entry.pos.y());
        bool selected = selectedIndices_.contains(i);
        painter.setBrush(selected ? Qt::yellow : Qt::red);
        painter.setPen(QPen(selected ? Qt::darkYellow : Qt::red, selected ? 3 : 2));
        painter.drawEllipse(mapped, selected ? radiusPixels + 2 : radiusPixels, selected ? radiusPixels + 2 : radiusPixels);
        painter.setPen(Qt::black);
        painter.drawText(mapped + QPointF(6, -6), entry.label);
    }
}

void CanvasWidget::mousePressEvent(QMouseEvent *event) {
    const int padding = 16;
    QRectF area = rect().adjusted(padding, padding, -padding, -padding);
    const double span = 10.0;
    const double scale = std::min(area.width(), area.height()) / span;
    QPointF origin(area.left() + area.width() / 2.0, area.top() + area.height() / 2.0);

    auto map = [&](const QPointF &p) -> QPointF {
        return QPointF(origin.x() + p.x() * scale, origin.y() - p.y() * scale);
    };

    int hitIndex = -1;
    double bestDist2 = std::numeric_limits<double>::max();
    const double tolerancePx = 8.0;
    const double tol2 = tolerancePx * tolerancePx;
    for (int i = 0; i < points_.size(); ++i) {
        QPointF screen = map(points_[i].pos);
        double dx = screen.x() - event->position().x();
        double dy = screen.y() - event->position().y();
        double d2 = dx * dx + dy * dy;
        if (d2 <= tol2 && d2 < bestDist2) {
            bestDist2 = d2;
            hitIndex = i;
        }
    }
    if (hitIndex >= 0) {
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            if (selectedIndices_.contains(hitIndex)) {
                selectedIndices_.remove(hitIndex);
            } else {
                selectedIndices_.insert(hitIndex);
            }
        } else {
            selectedIndices_.clear();
            selectedIndices_.insert(hitIndex);
        }
    } else if (!event->modifiers().testFlag(Qt::ControlModifier)) {
        selectedIndices_.clear();
    }
    update();
    QWidget::mousePressEvent(event);
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
    if (!doc.isObject()) {
        return;
    }
    points_.clear();
    lines_.clear();
    QJsonObject root = doc.object();
    QJsonArray pointsArr = root.value("points").toArray();
    for (const auto &value : pointsArr) {
        if (!value.isObject()) continue;
        const auto obj = value.toObject();
        double x = obj.value("x").toDouble();
        double y = obj.value("y").toDouble();
        QString label = obj.value("label").toString();
        if (label.isEmpty()) label = QStringLiteral("P");
        points_.append({QPointF(x, y), label});
    }
    QJsonArray linesArr = root.value("lines").toArray();
    for (const auto &value : linesArr) {
        if (!value.isObject()) continue;
        const auto obj = value.toObject();
        int a = obj.value("a").toInt(-1);
        int b = obj.value("b").toInt(-1);
        if (a >= 0 && b >= 0) {
            lines_.append({a, b});
        }
    }
}

void CanvasWidget::savePointsToFile() const {
    if (storagePath_.isEmpty()) {
        return;
    }
    QJsonArray pointsArr;
    for (const auto &entry : points_) {
        QJsonObject obj;
        obj.insert("x", entry.pos.x());
        obj.insert("y", entry.pos.y());
        obj.insert("label", entry.label);
        pointsArr.append(obj);
    }
    QJsonArray linesArr;
    for (const auto &line : lines_) {
        QJsonObject obj;
        obj.insert("a", line.a);
        obj.insert("b", line.b);
        linesArr.append(obj);
    }
    QJsonObject root;
    root.insert("points", pointsArr);
    root.insert("lines", linesArr);
    QJsonDocument doc(root);

    QDir().mkpath(QFileInfo(storagePath_).absolutePath());
    QFile file(storagePath_);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}
