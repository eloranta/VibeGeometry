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
#include <cmath>
#include <vector>

namespace {
bool segmentIntersection(const QPointF &p, const QPointF &p2, const QPointF &q, const QPointF &q2, QPointF &out) {
    QPointF r = p2 - p;
    QPointF s = q2 - q;
    double denom = r.x() * s.y() - r.y() * s.x();
    if (std::abs(denom) < 1e-9) {
        return false;  // parallel or colinear
    }
    QPointF qp = q - p;
    double t = (qp.x() * s.y() - qp.y() * s.x()) / denom;
    double u = (qp.x() * r.y() - qp.y() * r.x()) / denom;
    if (t >= -1e-9 && t <= 1.0 + 1e-9 && u >= -1e-9 && u <= 1.0 + 1e-9) {
        out = p + t * r;
        return true;
    }
    return false;
}

std::vector<QPointF> segmentCircleIntersections(const QPointF &p1, const QPointF &p2, const QPointF &c, double r) {
    std::vector<QPointF> hits;
    QPointF d = p2 - p1;
    double A = d.x() * d.x() + d.y() * d.y();
    if (A < 1e-12) return hits;
    QPointF f = p1 - c;
    double B = 2.0 * (f.x() * d.x() + f.y() * d.y());
    double C = f.x() * f.x() + f.y() * f.y() - r * r;
    double disc = B * B - 4 * A * C;
    if (disc < 0.0) return hits;
    double sqrtDisc = std::sqrt(std::max(0.0, disc));
    double t1 = (-B - sqrtDisc) / (2 * A);
    double t2 = (-B + sqrtDisc) / (2 * A);
    auto addIf = [&](double t) {
        if (t >= -1e-9 && t <= 1.0 + 1e-9) {
            hits.push_back(p1 + t * d);
        }
    };
    addIf(t1);
    if (disc > 1e-12) addIf(t2);
    return hits;
}

std::vector<QPointF> circleCircleIntersections(const QPointF &c0, double r0, const QPointF &c1, double r1) {
    std::vector<QPointF> hits;
    double dx = c1.x() - c0.x();
    double dy = c1.y() - c0.y();
    double d = std::hypot(dx, dy);
    if (d < 1e-9 || d > r0 + r1 || d < std::abs(r0 - r1)) {
        return hits;
    }
    double a = (r0 * r0 - r1 * r1 + d * d) / (2 * d);
    double h2 = r0 * r0 - a * a;
    if (h2 < 0.0) return hits;
    double h = std::sqrt(std::max(0.0, h2));
    QPointF p2(c0.x() + a * dx / d, c0.y() + a * dy / d);
    double rx = -dy * (h / d);
    double ry = dx * (h / d);
    hits.push_back(QPointF(p2.x() + rx, p2.y() + ry));
    if (h > 1e-9) hits.push_back(QPointF(p2.x() - rx, p2.y() - ry));
    return hits;
}
}  // namespace

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

int CanvasWidget::selectedLineCount() const {
    return selectedLineIndices_.size();
}

int CanvasWidget::selectedCircleCount() const {
    return selectedCircleIndices_.size();
}

QString CanvasWidget::nextPointLabel() const {
    return QString("P%1").arg(points_.size() + 1);
}

QString CanvasWidget::nextLineLabel() const {
    return QString("L%1").arg(lines_.size() + 1);
}

QString CanvasWidget::suggestedLineLabel() const {
    return nextLineLabel();
}

void CanvasWidget::addIntersectionPoint(const QPointF &pt) {
    if (!hasPoint(pt)) {
        addPoint(pt, nextPointLabel());
    }
}

std::pair<QPointF, QPointF> CanvasWidget::lineEndpoints(const LineEntry &line) const {
    QPointF p1 = points_[line.a].pos;
    QPointF p2 = points_[line.b].pos;
    if (!line.extended) {
        return {p1, p2};
    }
    const double xmin = -5.0, xmax = 5.0, ymin = -5.0, ymax = 5.0;
    const double dx = p2.x() - p1.x();
    const double dy = p2.y() - p1.y();
    QVector<QPointF> hits;
    auto addIfInside = [&](double x, double y) {
        if (x >= xmin - 1e-9 && x <= xmax + 1e-9 && y >= ymin - 1e-9 && y <= ymax + 1e-9) {
            hits.append(QPointF(x, y));
        }
    };
    if (std::abs(dx) > 1e-9) {
        double t1 = (xmin - p1.x()) / dx;
        addIfInside(xmin, p1.y() + t1 * dy);
        double t2 = (xmax - p1.x()) / dx;
        addIfInside(xmax, p1.y() + t2 * dy);
    }
    if (std::abs(dy) > 1e-9) {
        double t3 = (ymin - p1.y()) / dy;
        addIfInside(p1.x() + t3 * dx, ymin);
        double t4 = (ymax - p1.y()) / dy;
        addIfInside(p1.x() + t4 * dx, ymax);
    }
    QVector<QPointF> uniqueHits;
    auto isClose = [](const QPointF &a, const QPointF &b) {
        return std::hypot(a.x() - b.x(), a.y() - b.y()) < 1e-6;
    };
    for (const auto &h : hits) {
        bool dup = false;
        for (const auto &u : uniqueHits) {
            if (isClose(h, u)) {
                dup = true;
                break;
            }
        }
        if (!dup) uniqueHits.append(h);
    }
    if (uniqueHits.size() >= 2) {
        QVector<std::pair<double, QPointF>> proj;
        if (std::abs(dx) >= std::abs(dy)) {
            for (const auto &h : uniqueHits) proj.append({ (h.x() - p1.x()) / dx, h });
        } else {
            for (const auto &h : uniqueHits) proj.append({ (h.y() - p1.y()) / dy, h });
        }
        std::sort(proj.begin(), proj.end(), [](const auto &a, const auto &b){ return a.first < b.first; });
        return {proj.front().second, proj.back().second};
    }
    return {p1, p2};
}

bool CanvasWidget::selectedPoint(QPointF &point) const {
    if (selectedIndices_.isEmpty()) {
        return false;
    }
    QList<int> indices = selectedIndices_.values();
    std::sort(indices.begin(), indices.end());
    int idx = indices.first();
    if (idx < 0 || idx >= points_.size()) {
        return false;
    }
    point = points_[idx].pos;
    return true;
}

bool CanvasWidget::addLineBetweenSelected(const QString &label) {
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
    lines_.append({a, b, false, label});
    findIntersectionsForLine(lines_.size() - 1);
    savePointsToFile();
    update();
    return true;
}

bool CanvasWidget::extendSelectedLines() {
    bool changed = false;
    for (int idx : selectedLineIndices_) {
        if (idx >= 0 && idx < lines_.size()) {
            if (!lines_[idx].extended) {
                lines_[idx].extended = true;
                changed = true;
                findIntersectionsForLine(idx);
            }
        }
    }
    if (changed) {
        savePointsToFile();
        update();
    }
    return changed;
}

bool CanvasWidget::addCircle(const QPointF &center, double radius) {
    if (radius <= 0.0) {
        return false;
    }
    circles_.append({center, radius});
    findIntersectionsForCircle(circles_.size() - 1);
    savePointsToFile();
    update();
    return true;
}

bool CanvasWidget::addNormalAtPoint(int lineIndex, const QPointF &point) {
    if (lineIndex < 0 || lineIndex >= lines_.size() || points_.isEmpty()) return false;
    auto [p1, p2] = lineEndpoints(lines_[lineIndex]);
    QPointF d = p2 - p1;
    if (std::abs(d.x()) < 1e-9 && std::abs(d.y()) < 1e-9) return false;
    // Direction vector perpendicular to d: (-dy, dx)
    QPointF perp(-d.y(), d.x());
    QPointF newEnd = point + perp;  // arbitrary length; intersects will be handled
    // Add a point for the second endpoint
    if (!hasPoint(newEnd)) {
        addPoint(newEnd, nextPointLabel());
    }
    // Ensure the point exists and get indices
    int aIndex = -1;
    int bIndex = -1;
    for (int i = 0; i < points_.size(); ++i) {
        if (qFuzzyCompare(points_[i].pos.x(), point.x()) && qFuzzyCompare(points_[i].pos.y(), point.y())) {
            aIndex = i;
        }
        if (qFuzzyCompare(points_[i].pos.x(), newEnd.x()) && qFuzzyCompare(points_[i].pos.y(), newEnd.y())) {
            bIndex = i;
        }
    }
    if (aIndex == -1 || bIndex == -1 || aIndex == bIndex) return false;
    lines_.append({aIndex, bIndex, false, nextLineLabel()});
    findIntersectionsForLine(lines_.size() - 1);
    savePointsToFile();
    update();
    return true;
}

bool CanvasWidget::deleteSelected() {
    bool changed = false;
    QSet<int> removePoints = selectedIndices_;
    QVector<int> indexMap(points_.size(), -1);
    QVector<PointEntry> newPoints;
    if (!removePoints.isEmpty()) {
        for (int i = 0; i < points_.size(); ++i) {
            if (removePoints.contains(i)) {
                continue;
            }
            indexMap[i] = newPoints.size();
            newPoints.append(points_[i]);
        }
    } else {
        for (int i = 0; i < points_.size(); ++i) {
            indexMap[i] = i;
        }
    }

    QVector<LineEntry> newLines;
    for (int i = 0; i < lines_.size(); ++i) {
        const auto &line = lines_[i];
        if (selectedLineIndices_.contains(i)) {
            changed = true;
            continue;
        }
        if (removePoints.contains(line.a) || removePoints.contains(line.b)) {
            changed = true;
            continue;
        }
        int na = indexMap[line.a];
        int nb = indexMap[line.b];
        if (na < 0 || nb < 0) {
            changed = true;
            continue;
        }
        newLines.append({na, nb, line.extended, line.label});
    }

    QVector<CircleEntry> newCircles;
    for (int i = 0; i < circles_.size(); ++i) {
        if (selectedCircleIndices_.contains(i)) {
            changed = true;
            continue;
        }
        newCircles.append(circles_[i]);
    }

    if (!removePoints.isEmpty()) {
        points_.swap(newPoints);
        changed = true;
    }
    if (selectedCircleIndices_.size() > 0) {
        circles_.swap(newCircles);
    } else if (newCircles.size() != circles_.size()) {
        circles_.swap(newCircles);
    }
    if (changed) {
        lines_.swap(newLines);
        selectedIndices_.clear();
        selectedLineIndices_.clear();
        selectedCircleIndices_.clear();
        savePointsToFile();
        update();
    }
    return changed;
}

void CanvasWidget::deleteAll() {
    if (points_.isEmpty() && lines_.isEmpty() && circles_.isEmpty()) {
        return;
    }
    points_.clear();
    lines_.clear();
    circles_.clear();
    selectedIndices_.clear();
    selectedLineIndices_.clear();
    selectedCircleIndices_.clear();
    savePointsToFile();
    update();
}

void CanvasWidget::findIntersectionsForLine(int lineIndex) {
    if (lineIndex < 0 || lineIndex >= lines_.size()) return;
    auto [a1, a2] = lineEndpoints(lines_[lineIndex]);

    // With other lines
    for (int i = 0; i < lines_.size(); ++i) {
        if (i == lineIndex) continue;
        auto [b1, b2] = lineEndpoints(lines_[i]);
        QPointF hit;
        if (segmentIntersection(a1, a2, b1, b2, hit)) {
            addIntersectionPoint(hit);
        }
    }
    // With circles
    for (const auto &circle : circles_) {
        auto hits = segmentCircleIntersections(a1, a2, circle.center, circle.radius);
        for (const auto &h : hits) {
            addIntersectionPoint(h);
        }
    }
}

void CanvasWidget::findIntersectionsForCircle(int circleIndex) {
    if (circleIndex < 0 || circleIndex >= circles_.size()) return;
    const auto &c = circles_[circleIndex];
    // Circle with lines
    for (const auto &line : lines_) {
        auto [p1, p2] = lineEndpoints(line);
        auto hits = segmentCircleIntersections(p1, p2, c.center, c.radius);
        for (const auto &h : hits) {
            addIntersectionPoint(h);
        }
    }
    // Circle with other circles
    for (int i = 0; i < circles_.size(); ++i) {
        if (i == circleIndex) continue;
        const auto &other = circles_[i];
        auto hits = circleCircleIntersections(c.center, c.radius, other.center, other.radius);
        for (const auto &h : hits) {
            addIntersectionPoint(h);
        }
    }
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
    for (int i = 0; i < lines_.size(); ++i) {
        const auto &line = lines_[i];
        if (line.a < 0 || line.b < 0 || line.a >= points_.size() || line.b >= points_.size()) continue;
        auto [p1, p2] = lineEndpoints(line);
        bool selected = selectedLineIndices_.contains(i);
        painter.setPen(QPen(selected ? Qt::darkBlue : Qt::blue, selected ? 4 : 2));
        painter.drawLine(map(p1.x(), p1.y()), map(p2.x(), p2.y()));
        // Label at midpoint
        QPointF mid = (p1 + p2) / 2.0;
        painter.setPen(Qt::black);
        painter.setFont([&]{
            QFont f = painter.font();
            f.setPointSizeF(9.0);
            return f;
        }());
        painter.drawText(map(mid.x(), mid.y()) + QPointF(6, -6), line.label);
    }

    painter.setPen(QPen(Qt::darkGreen, 2));
    for (int i = 0; i < circles_.size(); ++i) {
        const auto &circle = circles_[i];
        bool selected = selectedCircleIndices_.contains(i);
        painter.setPen(QPen(selected ? Qt::darkGreen : Qt::darkGreen, selected ? 3 : 2, selected ? Qt::DashLine : Qt::SolidLine));
        QPointF topLeft = map(circle.center.x() - circle.radius, circle.center.y() + circle.radius);
        QPointF bottomRight = map(circle.center.x() + circle.radius, circle.center.y() - circle.radius);
        painter.drawEllipse(QRectF(topLeft, bottomRight));
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
    auto unmap = [&](const QPointF &p) -> QPointF {
        return QPointF((p.x() - origin.x()) / scale, -(p.y() - origin.y()) / scale);
    };

    int hitPoint = -1;
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
            hitPoint = i;
        }
    }

    int hitLine = -1;
    double bestLineDist = tolerancePx;  // threshold in px
    auto pointToSegmentDistance = [](const QPointF &p, const QPointF &a, const QPointF &b) -> double {
        const double dx = b.x() - a.x();
        const double dy = b.y() - a.y();
        const double len2 = dx * dx + dy * dy;
        if (len2 == 0.0) {
            double dxp = p.x() - a.x();
            double dyp = p.y() - a.y();
            return std::sqrt(dxp * dxp + dyp * dyp);
        }
        double t = ((p.x() - a.x()) * dx + (p.y() - a.y()) * dy) / len2;
        t = std::clamp(t, 0.0, 1.0);
        QPointF proj(a.x() + t * dx, a.y() + t * dy);
        double dxp = p.x() - proj.x();
        double dyp = p.y() - proj.y();
        return std::sqrt(dxp * dxp + dyp * dyp);
    };
    for (int i = 0; i < lines_.size(); ++i) {
        const auto &line = lines_[i];
        if (line.a < 0 || line.b < 0 || line.a >= points_.size() || line.b >= points_.size()) continue;
        auto [pa, pb] = lineEndpoints(line);
        QPointF a = map(pa);
        QPointF b = map(pb);
        double dist = pointToSegmentDistance(event->position(), a, b);
        if (dist <= bestLineDist) {
            bestLineDist = dist;
            hitLine = i;
        }
    }
    bool lineWasSelected = (hitLine >= 0 && selectedLineIndices_.contains(hitLine));

    int hitCircle = -1;
    double bestCircleDist = tolerancePx;
    for (int i = 0; i < circles_.size(); ++i) {
        const auto &c = circles_[i];
        QPointF mappedCenter = map(c.center);
        double rpx = c.radius * (scale);  // radius in pixels
        double dist = std::abs(std::hypot(event->position().x() - mappedCenter.x(),
                                         event->position().y() - mappedCenter.y()) - rpx);
        if (dist <= bestCircleDist) {
            bestCircleDist = dist;
            hitCircle = i;
        }
    }

    bool ctrl = event->modifiers().testFlag(Qt::ControlModifier);
    bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    if (hitPoint >= 0) {
        if (ctrl) {
            if (selectedIndices_.contains(hitPoint)) selectedIndices_.remove(hitPoint);
            else selectedIndices_.insert(hitPoint);
        } else {
            selectedIndices_.clear();
            selectedIndices_.insert(hitPoint);
            selectedLineIndices_.clear();
            selectedCircleIndices_.clear();
        }
    } else if (hitLine >= 0) {
        if (ctrl) {
            if (selectedLineIndices_.contains(hitLine)) selectedLineIndices_.remove(hitLine);
            else selectedLineIndices_.insert(hitLine);
        } else {
            selectedLineIndices_.clear();
            selectedLineIndices_.insert(hitLine);
            selectedIndices_.clear();
            selectedCircleIndices_.clear();
        }
    } else if (hitCircle >= 0) {
        if (ctrl) {
            if (selectedCircleIndices_.contains(hitCircle)) selectedCircleIndices_.remove(hitCircle);
            else selectedCircleIndices_.insert(hitCircle);
        } else {
            selectedCircleIndices_.clear();
            selectedCircleIndices_.insert(hitCircle);
            selectedIndices_.clear();
            selectedLineIndices_.clear();
        }
    } else if (!ctrl) {
        selectedIndices_.clear();
        selectedLineIndices_.clear();
        selectedCircleIndices_.clear();
    }
    update();

    // If clicking near a line that was already selected, add a point on that line near the click.
    if (lineWasSelected && hitLine >= 0) {
        auto [pa, pb] = lineEndpoints(lines_[hitLine]);
        QPointF clickLogical = unmap(event->position());
        QPointF d = pb - pa;
        double len2 = d.x() * d.x() + d.y() * d.y();
        if (len2 > 1e-12) {
            double t = ((clickLogical.x() - pa.x()) * d.x() + (clickLogical.y() - pa.y()) * d.y()) / len2;
            t = std::clamp(t, 0.0, 1.0);
            QPointF proj(pa.x() + t * d.x(), pa.y() + t * d.y());
            addPoint(proj, nextPointLabel());
        }
    }

    // Shift+click anywhere adds a point at that canvas location.
    if (shift) {
        QPointF logical = unmap(event->position());
        addPoint(logical, nextPointLabel());
    }

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
    circles_.clear();
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
        bool extended = obj.value("extended").toBool(false);
        QString label = obj.value("label").toString();
        if (label.isEmpty()) label = nextLineLabel();
        if (a >= 0 && b >= 0) {
            lines_.append({a, b, extended, label});
        }
    }
    QJsonArray circlesArr = root.value("circles").toArray();
    for (const auto &value : circlesArr) {
        if (!value.isObject()) continue;
        const auto obj = value.toObject();
        double cx = obj.value("x").toDouble();
        double cy = obj.value("y").toDouble();
        double r = obj.value("r").toDouble();
        if (r > 0.0) {
            circles_.append({QPointF(cx, cy), r});
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
        obj.insert("extended", line.extended);
        obj.insert("label", line.label);
        linesArr.append(obj);
    }
    QJsonArray circlesArr;
    for (const auto &circle : circles_) {
        QJsonObject obj;
        obj.insert("x", circle.center.x());
        obj.insert("y", circle.center.y());
        obj.insert("r", circle.radius);
        circlesArr.append(obj);
    }
    QJsonObject root;
    root.insert("points", pointsArr);
    root.insert("lines", linesArr);
    root.insert("circles", circlesArr);
    QJsonDocument doc(root);

    QDir().mkpath(QFileInfo(storagePath_).absolutePath());
    QFile file(storagePath_);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}
