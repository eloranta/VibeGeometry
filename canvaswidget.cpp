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
      storagePath(storagePath) {
    setMinimumSize(320, 240);
}

bool CanvasWidget::addPoint(const QPointF &point, const QString &label, bool selectNew) {
    if (hasPoint(point)) {
        return false;
    }
    points.append(Point(point, label));
    if (selectNew) {
        int newIndex = points.size() - 1;
        selectedPointIndices.insert(newIndex);
        pointSelectionOrder.removeAll(newIndex);
        pointSelectionOrder.append(newIndex);
    }
    update();
    return true;
}

bool CanvasWidget::hasPoint(const QPointF &point) const {
    for (const auto &p : points) {
        if (qFuzzyCompare(p.positiom.x(), point.x()) && qFuzzyCompare(p.positiom.y(), point.y())) {
            return true;
        }
    }
    return false;
}

int CanvasWidget::pointCount() const {
    return points.size();
}

int CanvasWidget::selectedCount() const {
    return selectedPointIndices.size();
}

int CanvasWidget::selectedLineCount() const {
    return selectedLineIndices.size();
}

int CanvasWidget::selectedCircleCount() const {
    return selectedCircleIndices.size();
}

QString CanvasWidget::nextPointLabel() const {
    return QString("P%1").arg(points.size() + 1);
}

QString CanvasWidget::nextLineLabel() const {
    return QString("L%1").arg(lines.size() + 1);
}

QString CanvasWidget::suggestedLineLabel() const {
    return nextLineLabel();
}

QString CanvasWidget::nextCircleLabel() const {
    return QString("C%1").arg(circles.size() + 1);
}

void CanvasWidget::addIntersectionPoint(const QPointF &pt) {
    if (!hasPoint(pt)) {
        addPoint(pt, QString(), false);
    }
}

bool CanvasWidget::setLabelForSelection(const QString &label) {
    int totalSelections = selectedPointIndices.size() + selectedLineIndices.size() +
                          selectedExtendedLineIndices.size() + selectedCircleIndices.size();
    if (totalSelections != 1) {
        return false;
    }
    bool changed = false;
    if (!selectedPointIndices.isEmpty()) {
        int idx = *selectedPointIndices.constBegin();
        if (idx >= 0 && idx < points.size()) {
            points[idx].label = label;
            changed = true;
        }
    } else if (!selectedLineIndices.isEmpty()) {
        int idx = *selectedLineIndices.constBegin();
        if (idx >= 0 && idx < lines.size()) {
            lines[idx].label = label;
            changed = true;
        }
    } else if (!selectedExtendedLineIndices.isEmpty()) {
        int idx = *selectedExtendedLineIndices.constBegin();
        if (idx >= 0 && idx < extendedLines.size()) {
            extendedLines[idx].label = label;
            changed = true;
        }
    } else if (!selectedCircleIndices.isEmpty()) {
        int idx = *selectedCircleIndices.constBegin();
        if (idx >= 0 && idx < circles.size()) {
            circles[idx].label = label;
            changed = true;
        }
    }
    if (changed) {
        update();
    }
    return changed;
}

std::pair<QPointF, QPointF> CanvasWidget::lineEndpoints(const Line &line) const {
    QPointF p1 = points[line.a].positiom;
    QPointF p2 = points[line.b].positiom;
    return {p1, p2};
}

std::pair<QPointF, QPointF> CanvasWidget::extendedLineEndpoints(const ExtendedLine &line) const {
    return {line.a, line.b};
}

bool CanvasWidget::selectedPoint(QPointF &point) const {
    if (selectedPointIndices.isEmpty()) {
        return false;
    }
    QList<int> indices = selectedPointIndices.values();
    std::sort(indices.begin(), indices.end());
    int idx = indices.first();
    if (idx < 0 || idx >= points.size()) {
        return false;
    }
    point = points[idx].positiom;
    return true;
}

bool CanvasWidget::addLineBetweenSelected(const QString &label) {
    if (selectedPointIndices.size() < 2) {
        return false;
    }
    QList<int> indices = selectedPointIndices.values();
    std::sort(indices.begin(), indices.end());
    int a = indices[0];
    int b = indices[1];
    if (a == b) return false;

    // Avoid duplicates (order-insensitive)
    for (const auto &line : lines) {
        if ((line.a == a && line.b == b) || (line.a == b && line.b == a)) {
            return false;
        }
    }
    lines.append(Line(a, b, label));
    update();
    return true;
}

bool CanvasWidget::extendSelectedLines() {
    bool changed = false;
    QVector<int> toRemove;
    for (int idx : selectedLineIndices) {
        if (idx >= 0 && idx < lines.size()) {
            // Compute intersections with bounding box [-5,5] x [-5,5] to extend across the canvas.
            const QPointF p1 = points[lines[idx].a].positiom;
            const QPointF p2 = points[lines[idx].b].positiom;
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
            // Remove duplicates
            QVector<QPointF> uniqueHits;
            auto isClose = [](const QPointF &a, const QPointF &b) {
                return std::hypot(a.x() - b.x(), a.y() - b.y()) < 1e-6;
            };
            for (const auto &h : hits) {
                bool dup = false;
                for (const auto &u : uniqueHits) {
                    if (isClose(h, u)) { dup = true; break; }
                }
                if (!dup) uniqueHits.append(h);
            }
            QPointF aPoint = p1;
            QPointF bPoint = p2;
            if (uniqueHits.size() >= 2) {
                QVector<std::pair<double, QPointF>> proj;
                if (std::abs(dx) >= std::abs(dy)) {
                    for (const auto &h : uniqueHits) proj.append({ (h.x() - p1.x()) / dx, h });
                } else {
                    for (const auto &h : uniqueHits) proj.append({ (h.y() - p1.y()) / dy, h });
                }
                std::sort(proj.begin(), proj.end(), [](const auto &a, const auto &b){ return a.first < b.first; });
                aPoint = proj.front().second;
                bPoint = proj.back().second;
            }
            extendedLines.append(ExtendedLine(aPoint, bPoint, lines[idx].label));
            toRemove.append(idx);
            changed = true;
        }
    }
    if (!toRemove.isEmpty()) {
        QVector<Line> newLines;
        for (int i = 0; i < lines.size(); ++i) {
            if (!toRemove.contains(i)) {
                newLines.append(lines[i]);
            }
        }
        lines.swap(newLines);
        selectedLineIndices.clear();
    }
    if (changed) {
        update();
    }
    return changed;
}

bool CanvasWidget::addCircle(const QPointF &center, double radius) {
    if (radius <= 0.0) {
        return false;
    }
    circles.append(Circle(center, radius, QString()));
    update();
    return true;
}

bool CanvasWidget::addNormalAtPoint(int lineIndex, const QPointF &point) {
    if (lineIndex < 0 || lineIndex >= lines.size()) return false;
    auto [p1, p2] = lineEndpoints(lines[lineIndex]);
    QPointF d = p2 - p1;
    if (std::abs(d.x()) < 1e-9 && std::abs(d.y()) < 1e-9) return false;
    QPointF perp(-d.y(), d.x());
    double len = std::hypot(perp.x(), perp.y());
    if (len < 1e-9) return false;
    QPointF dir = QPointF(perp.x() / len, perp.y() / len);
    const double span = 20.0;  // enough to cross the -5..5 box
    QPointF a = point + dir * span;
    QPointF b = point - dir * span;
    extendedLines.append(ExtendedLine(a, b, QString()));
    update();
    return true;
}

bool CanvasWidget::deleteSelected() {
    bool changed = false;
    QSet<int> removePoints = selectedPointIndices;
    QVector<int> indexMap(points.size(), -1);
    QVector<Point> newPoints;
    if (!removePoints.isEmpty()) {
        for (int i = 0; i < points.size(); ++i) {
            if (removePoints.contains(i)) {
                continue;
            }
            indexMap[i] = newPoints.size();
            newPoints.append(points[i]);
        }
    } else {
        for (int i = 0; i < points.size(); ++i) {
            indexMap[i] = i;
        }
    }

    QVector<Line> newLines;
    for (int i = 0; i < lines.size(); ++i) {
        const auto &line = lines[i];
        if (selectedLineIndices.contains(i)) {
            changed = true;
            continue;
        }
        if (removePoints.contains(line.a) || removePoints.contains(line.b)) {
            changed = true;
            continue;
        }
        if (line.a < 0 || line.b < 0 || line.a >= indexMap.size() || line.b >= indexMap.size()) {
            changed = true;
            continue;
        }
        int na = indexMap[line.a];
        int nb = indexMap[line.b];
        if (na < 0 || nb < 0) {
            changed = true;
            continue;
        }
        newLines.append(Line(na, nb, line.label));
    }

    QVector<ExtendedLine> newExtended;
    for (int i = 0; i < extendedLines.size(); ++i) {
        if (selectedExtendedLineIndices.contains(i)) {
            changed = true;
            continue;
        }
        newExtended.append(extendedLines[i]);
    }

    QVector<Circle> newCircles;
    for (int i = 0; i < circles.size(); ++i) {
        if (selectedCircleIndices.contains(i)) {
            changed = true;
            continue;
        }
        newCircles.append(circles[i]);
    }

    if (!removePoints.isEmpty()) {
        points.swap(newPoints);
        changed = true;
    }
    if (selectedCircleIndices.size() > 0) {
        circles.swap(newCircles);
    } else if (newCircles.size() != circles.size()) {
        circles.swap(newCircles);
    }
    if (changed) {
        lines.swap(newLines);
        extendedLines.swap(newExtended);
        selectedPointIndices.clear();
        selectedLineIndices.clear();
        selectedExtendedLineIndices.clear();
        selectedCircleIndices.clear();
        update();
    }
    return changed;
}

void CanvasWidget::deleteAll() {
    if (points.isEmpty() && lines.isEmpty() && circles.isEmpty()) {
        return;
    }
    points.clear();
    lines.clear();
    extendedLines.clear();
    circles.clear();
    selectedPointIndices.clear();
    selectedLineIndices.clear();
    selectedExtendedLineIndices.clear();
    selectedCircleIndices.clear();
    update();
}

void CanvasWidget::findIntersectionsForLine(int lineIndex) {
    if (lineIndex < 0 || lineIndex >= lines.size()) return;
    auto [a1, a2] = lineEndpoints(lines[lineIndex]);

    // With other lines
    for (int i = 0; i < lines.size(); ++i) {
        if (i == lineIndex) continue;
        auto [b1, b2] = lineEndpoints(lines[i]);
        QPointF hit;
        if (segmentIntersection(a1, a2, b1, b2, hit)) {
            addIntersectionPoint(hit);
        }
    }
    // With extended lines
    for (int i = 0; i < extendedLines.size(); ++i) {
        auto [b1, b2] = extendedLineEndpoints(extendedLines[i]);
        QPointF hit;
        if (segmentIntersection(a1, a2, b1, b2, hit)) {
            addIntersectionPoint(hit);
        }
    }
    // With circles
    for (const auto &circle : circles) {
        auto hits = segmentCircleIntersections(a1, a2, circle.center, circle.radius);
        for (const auto &h : hits) {
            addIntersectionPoint(h);
        }
    }
}

void CanvasWidget::findIntersectionsForExtendedLine(int lineIndex) {
    if (lineIndex < 0 || lineIndex >= extendedLines.size()) return;
    auto [a1, a2] = extendedLineEndpoints(extendedLines[lineIndex]);

    // With finite lines
    for (int i = 0; i < lines.size(); ++i) {
        auto [b1, b2] = lineEndpoints(lines[i]);
        QPointF hit;
        if (segmentIntersection(a1, a2, b1, b2, hit)) {
            addIntersectionPoint(hit);
        }
    }
    // With other extended lines
    for (int i = 0; i < extendedLines.size(); ++i) {
        if (i == lineIndex) continue;
        auto [b1, b2] = extendedLineEndpoints(extendedLines[i]);
        QPointF hit;
        if (segmentIntersection(a1, a2, b1, b2, hit)) {
            addIntersectionPoint(hit);
        }
    }
    // With circles
    for (const auto &circle : circles) {
        auto hits = segmentCircleIntersections(a1, a2, circle.center, circle.radius);
        for (const auto &h : hits) {
            addIntersectionPoint(h);
        }
    }
}

void CanvasWidget::recomputeAllIntersections() {
    // Rebuild points by keeping existing named points and re-adding intersection points.
    // For simplicity, we keep current points and just add any missing intersections.
    for (int i = 0; i < lines.size(); ++i) {
        findIntersectionsForLine(i);
    }
    for (int i = 0; i < extendedLines.size(); ++i) {
        findIntersectionsForExtendedLine(i);
    }
    for (int i = 0; i < circles.size(); ++i) {
        findIntersectionsForCircle(i);
    }
    update();
}

void CanvasWidget::recomputeSelectedIntersections() {
    // Only compute intersections between the selected combination of two objects.
    if (selectedPointIndices.size() + selectedLineIndices.size() + selectedExtendedLineIndices.size() + selectedCircleIndices.size() != 2) {
        return;
    }
    // Collect selected objects
    QVector<int> pointSel = selectedPointIndices.values().toVector();
    QVector<int> lineSel = selectedLineIndices.values().toVector();
    QVector<int> extLineSel = selectedExtendedLineIndices.values().toVector();
    QVector<int> circleSel = selectedCircleIndices.values().toVector();

    auto addPt = [&](const QPointF &pt) {
        addIntersectionPoint(pt);
    };

    // Cases:
    if (lineSel.size() == 2) {
        auto [a1, a2] = lineEndpoints(lines[lineSel[0]]);
        auto [b1, b2] = lineEndpoints(lines[lineSel[1]]);
        QPointF hit;
        if (segmentIntersection(a1, a2, b1, b2, hit)) addPt(hit);
    } else if (lineSel.size() == 1 && circleSel.size() == 1) {
        auto [p1, p2] = lineEndpoints(lines[lineSel[0]]);
        auto hits = segmentCircleIntersections(p1, p2, circles[circleSel[0]].center, circles[circleSel[0]].radius);
        for (const auto &h : hits) addPt(h);
    } else if (extLineSel.size() == 2) {
        auto [a1, a2] = extendedLineEndpoints(extendedLines[extLineSel[0]]);
        auto [b1, b2] = extendedLineEndpoints(extendedLines[extLineSel[1]]);
        QPointF hit;
        if (segmentIntersection(a1, a2, b1, b2, hit)) addPt(hit);
    } else if (extLineSel.size() == 1 && lineSel.size() == 1) {
        auto [a1, a2] = extendedLineEndpoints(extendedLines[extLineSel[0]]);
        auto [b1, b2] = lineEndpoints(lines[lineSel[0]]);
        QPointF hit;
        if (segmentIntersection(a1, a2, b1, b2, hit)) addPt(hit);
    } else if (extLineSel.size() == 1 && circleSel.size() == 1) {
        auto [p1, p2] = extendedLineEndpoints(extendedLines[extLineSel[0]]);
        auto hits = segmentCircleIntersections(p1, p2, circles[circleSel[0]].center, circles[circleSel[0]].radius);
        for (const auto &h : hits) addPt(h);
    } else if (circleSel.size() == 2) {
        auto hits = circleCircleIntersections(circles[circleSel[0]].center, circles[circleSel[0]].radius,
                                              circles[circleSel[1]].center, circles[circleSel[1]].radius);
        for (const auto &h : hits) addPt(h);
    } else if ((lineSel.size() == 1 || extLineSel.size() == 1) && pointSel.size() == 1) {
        QPointF p1, p2;
        if (extLineSel.size() == 1) {
            std::tie(p1, p2) = extendedLineEndpoints(extendedLines[extLineSel[0]]);
        } else {
            std::tie(p1, p2) = lineEndpoints(lines[lineSel[0]]);
        }
        QPointF pt = points[pointSel[0]].positiom;
        // Add projection if within segment (or infinite if extended)
        QPointF d = p2 - p1;
        double len2 = d.x() * d.x() + d.y() * d.y();
        if (len2 > 1e-12) {
            double t = ((pt.x() - p1.x()) * d.x() + (pt.y() - p1.y()) * d.y()) / len2;
            if (lineSel.size() == 1) {
                if (t < -1e-9 || t > 1.0 + 1e-9) t = std::clamp(t, 0.0, 1.0);
            }
            QPointF proj(p1.x() + t * d.x(), p1.y() + t * d.y());
            addPt(proj);
        }
    } else if (circleSel.size() == 1 && pointSel.size() == 1) {
        // Add point if it's on the circle (within small epsilon)
        const auto &c = circles[circleSel[0]];
        double dist = std::hypot(pointSel[0] < points.size() ? points[pointSel[0]].positiom.x() - c.center.x() : 0.0,
                                 pointSel[0] < points.size() ? points[pointSel[0]].positiom.y() - c.center.y() : 0.0);
        if (std::abs(dist - c.radius) < 1e-6) {
            addPt(points[pointSel[0]].positiom);
        }
    }
    update();
}

void CanvasWidget::findIntersectionsForCircle(int circleIndex) {
    if (circleIndex < 0 || circleIndex >= circles.size()) return;
    const auto &c = circles[circleIndex];
    // Circle with lines
    for (const auto &line : lines) {
        auto [p1, p2] = lineEndpoints(line);
        auto hits = segmentCircleIntersections(p1, p2, c.center, c.radius);
        for (const auto &h : hits) {
            addIntersectionPoint(h);
        }
    }
    // Circle with extended lines
    for (const auto &line : extendedLines) {
        auto [p1, p2] = extendedLineEndpoints(line);
        auto hits = segmentCircleIntersections(p1, p2, c.center, c.radius);
        for (const auto &h : hits) {
            addIntersectionPoint(h);
        }
    }
    // Circle with other circles
    for (int i = 0; i < circles.size(); ++i) {
        if (i == circleIndex) continue;
        const auto &other = circles[i];
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

    // Axes/ticks intentionally hidden for now

    painter.setPen(QPen(Qt::blue, 2));
    for (int i = 0; i < lines.size(); ++i) {
        const auto &line = lines[i];
        if ((line.a < 0 || line.b < 0 || line.a >= points.size() || line.b >= points.size())) continue;
        auto [p1, p2] = lineEndpoints(line);
        bool selected = selectedLineIndices.contains(i);
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

    painter.setPen(QPen(Qt::darkCyan, 2, Qt::DashLine));
    for (int i = 0; i < extendedLines.size(); ++i) {
        const auto &line = extendedLines[i];
        auto [p1, p2] = extendedLineEndpoints(line);
        bool selected = selectedExtendedLineIndices.contains(i);
        painter.setPen(QPen(selected ? Qt::darkCyan : Qt::darkCyan, selected ? 4 : 2, Qt::DashLine));
        painter.drawLine(map(p1.x(), p1.y()), map(p2.x(), p2.y()));
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
    for (int i = 0; i < circles.size(); ++i) {
        const auto &circle = circles[i];
        bool selected = selectedCircleIndices.contains(i);
        painter.setPen(QPen(selected ? Qt::darkGreen : Qt::darkGreen, selected ? 3 : 2, selected ? Qt::DashLine : Qt::SolidLine));
        QPointF topLeft = map(circle.center.x() - circle.radius, circle.center.y() + circle.radius);
        QPointF bottomRight = map(circle.center.x() + circle.radius, circle.center.y() - circle.radius);
        painter.drawEllipse(QRectF(topLeft, bottomRight));
        // Label near top-right of circle
        painter.setPen(Qt::black);
        painter.setFont([&]{
            QFont f = painter.font();
            f.setPointSizeF(9.0);
            return f;
        }());
        QPointF labelPos = map(circle.center.x() + circle.radius, circle.center.y() + circle.radius);
        painter.drawText(labelPos + QPointF(4, -4), circle.label);
    }

    const double radiusPixels = 4.0;
    painter.setFont([&]{
        QFont f = painter.font();
        f.setPointSizeF(9.0);
        return f;
    }());
    for (int i = 0; i < points.size(); ++i) {
        const auto &entry = points[i];
        QPointF mapped = map(entry.positiom.x(), entry.positiom.y());
        bool selected = selectedPointIndices.contains(i);
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
    for (int i = 0; i < points.size(); ++i) {
        QPointF screen = map(points[i].positiom);
        double dx = screen.x() - event->position().x();
        double dy = screen.y() - event->position().y();
        double d2 = dx * dx + dy * dy;
        if (d2 <= tol2 && d2 < bestDist2) {
            bestDist2 = d2;
            hitPoint = i;
        }
    }

    int hitLine = -1;
    int hitExtendedLine = -1;
    double bestLineDist = tolerancePx;  // threshold in px
    auto pointToSegmentDistance = [](const QPointF &p, const QPointF &a, const QPointF &b, bool infinite) -> double {
        const double dx = b.x() - a.x();
        const double dy = b.y() - a.y();
        const double len2 = dx * dx + dy * dy;
        if (len2 == 0.0) {
            double dxp = p.x() - a.x();
            double dyp = p.y() - a.y();
            return std::sqrt(dxp * dxp + dyp * dyp);
        }
        double t = ((p.x() - a.x()) * dx + (p.y() - a.y()) * dy) / len2;
        if (!infinite) {
            t = std::clamp(t, 0.0, 1.0);
        }
        QPointF proj(a.x() + t * dx, a.y() + t * dy);
        double dxp = p.x() - proj.x();
        double dyp = p.y() - proj.y();
        return std::sqrt(dxp * dxp + dyp * dyp);
    };
    for (int i = 0; i < lines.size(); ++i) {
        const auto &line = lines[i];
        if (line.a < 0 || line.b < 0 || line.a >= points.size() || line.b >= points.size()) continue;
        auto [pa, pb] = lineEndpoints(line);
        QPointF a = map(pa);
        QPointF b = map(pb);
        double dist = pointToSegmentDistance(event->position(), a, b, false);
        if (dist <= bestLineDist) {
            bestLineDist = dist;
            hitLine = i;
        }
    }
    for (int i = 0; i < extendedLines.size(); ++i) {
        const auto &line = extendedLines[i];
        auto [pa, pb] = extendedLineEndpoints(line);
        QPointF a = map(pa);
        QPointF b = map(pb);
        double dist = pointToSegmentDistance(event->position(), a, b, true);
        if (dist <= bestLineDist) {
            bestLineDist = dist;
            hitExtendedLine = i;
            hitLine = -1;
        }
    }
    bool lineWasSelected = (hitLine >= 0 && selectedLineIndices.contains(hitLine)) ||
                           (hitExtendedLine >= 0 && selectedExtendedLineIndices.contains(hitExtendedLine));

    int hitCircle = -1;
    double bestCircleDist = tolerancePx;
    for (int i = 0; i < circles.size(); ++i) {
        const auto &c = circles[i];
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
            if (selectedPointIndices.contains(hitPoint)) selectedPointIndices.remove(hitPoint);
            else selectedPointIndices.insert(hitPoint);
            pointSelectionOrder.removeAll(hitPoint);
            pointSelectionOrder.append(hitPoint);
        } else {
            selectedPointIndices.clear();
            selectedPointIndices.insert(hitPoint);
            selectedLineIndices.clear();
            selectedExtendedLineIndices.clear();
            selectedCircleIndices.clear();
            pointSelectionOrder.clear();
            pointSelectionOrder.append(hitPoint);
        }
    } else if (hitLine >= 0) {
        if (ctrl) {
            if (selectedLineIndices.contains(hitLine)) selectedLineIndices.remove(hitLine);
            else selectedLineIndices.insert(hitLine);
        } else {
            selectedLineIndices.clear();
            selectedLineIndices.insert(hitLine);
            selectedPointIndices.clear();
            selectedExtendedLineIndices.clear();
            selectedCircleIndices.clear();
            pointSelectionOrder.clear();
        }
    } else if (hitExtendedLine >= 0) {
        if (ctrl) {
            if (selectedExtendedLineIndices.contains(hitExtendedLine)) selectedExtendedLineIndices.remove(hitExtendedLine);
            else selectedExtendedLineIndices.insert(hitExtendedLine);
        } else {
            selectedExtendedLineIndices.clear();
            selectedExtendedLineIndices.insert(hitExtendedLine);
            selectedLineIndices.clear();
            selectedPointIndices.clear();
            selectedCircleIndices.clear();
            pointSelectionOrder.clear();
        }
    } else if (hitCircle >= 0) {
        if (ctrl) {
            if (selectedCircleIndices.contains(hitCircle)) selectedCircleIndices.remove(hitCircle);
            else selectedCircleIndices.insert(hitCircle);
        } else {
            selectedCircleIndices.clear();
            selectedCircleIndices.insert(hitCircle);
            selectedPointIndices.clear();
            selectedLineIndices.clear();
            selectedExtendedLineIndices.clear();
            pointSelectionOrder.clear();
        }
    } else if (!ctrl) {
        selectedPointIndices.clear();
        selectedLineIndices.clear();
        selectedExtendedLineIndices.clear();
        selectedCircleIndices.clear();
        pointSelectionOrder.clear();
    }
    update();

    bool handledShiftPoint = false;
    // If clicking near a line that was already selected and Shift is held, add a point on that line near the click.
    if (lineWasSelected && shift) {
        QPointF pa, pb;
        if (hitLine >= 0) {
            std::tie(pa, pb) = lineEndpoints(lines[hitLine]);
        } else if (hitExtendedLine >= 0) {
            std::tie(pa, pb) = extendedLineEndpoints(extendedLines[hitExtendedLine]);
        } else {
            // fallback: use last selected line index if any
            if (!selectedLineIndices.isEmpty()) {
                int idx = *selectedLineIndices.constBegin();
                std::tie(pa, pb) = lineEndpoints(lines[idx]);
            } else if (!selectedExtendedLineIndices.isEmpty()) {
                int idx = *selectedExtendedLineIndices.constBegin();
                std::tie(pa, pb) = extendedLineEndpoints(extendedLines[idx]);
            } else {
                pa = pb = QPointF();
            }
        }
        QPointF clickLogical = unmap(event->position());
        QPointF d = pb - pa;
        double len2 = d.x() * d.x() + d.y() * d.y();
        if (len2 > 1e-12) {
            double t = ((clickLogical.x() - pa.x()) * d.x() + (clickLogical.y() - pa.y()) * d.y()) / len2;
            if (hitExtendedLine >= 0 || (!selectedLineIndices.isEmpty() && hitLine < 0 && !selectedExtendedLineIndices.isEmpty())) {
                // treat as infinite
            } else {
                t = std::clamp(t, 0.0, 1.0);
            }
            QPointF proj(pa.x() + t * d.x(), pa.y() + t * d.y());
            addPoint(proj, QString(), true);
            handledShiftPoint = true;
        }
    }

    // Shift+click anywhere adds a point at that canvas location.
    if (shift && !handledShiftPoint) {
        QPointF logical = unmap(event->position());
        addPoint(logical, QString(), true);
    }

    QWidget::mousePressEvent(event);
}

bool CanvasWidget::loadPointsFromFile(const QString &path) {
    if (path.isEmpty()) {
        return false;
    }
    QFile file(path);
    if (!file.exists()) {
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const auto data = file.readAll();
    file.close();
    auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return false;
    }
    selectedPointIndices.clear();
    selectedLineIndices.clear();
    selectedExtendedLineIndices.clear();
    selectedCircleIndices.clear();
    pointSelectionOrder.clear();
    points.clear();
    lines.clear();
    extendedLines.clear();
    circles.clear();
    QJsonObject root = doc.object();
    QJsonArray pointsArr = root.value("points").toArray();
    for (const auto &value : pointsArr) {
        if (!value.isObject()) continue;
        const auto obj = value.toObject();
        double x = obj.value("x").toDouble();
        double y = obj.value("y").toDouble();
        QString label = obj.value("label").toString();
        points.append(Point(QPointF(x, y), label));
    }
    QJsonArray linesArr = root.value("lines").toArray();
    for (const auto &value : linesArr) {
        if (!value.isObject()) continue;
        const auto obj = value.toObject();
        int a = obj.value("a").toInt(-1);
        int b = obj.value("b").toInt(-1);
        QString label = obj.value("label").toString();
        if (obj.value("custom").toBool(false)) {
            QPointF customA(obj.value("customAx").toDouble(), obj.value("customAy").toDouble());
            QPointF customB(obj.value("customBx").toDouble(), obj.value("customBy").toDouble());
            extendedLines.append(ExtendedLine(customA, customB, label));
        } else if (a >= 0 && b >= 0) {
            lines.append(Line(a, b, label));
        }
    }
    QJsonArray extArr = root.value("extendedLines").toArray();
    for (const auto &value : extArr) {
        if (!value.isObject()) continue;
        const auto obj = value.toObject();
        QString label = obj.value("label").toString();
        QPointF a(obj.value("ax").toDouble(), obj.value("ay").toDouble());
        QPointF b(obj.value("bx").toDouble(), obj.value("by").toDouble());
        extendedLines.append(ExtendedLine(a, b, label));
    }
    QJsonArray circlesArr = root.value("circles").toArray();
    for (const auto &value : circlesArr) {
        if (!value.isObject()) continue;
        const auto obj = value.toObject();
        double cx = obj.value("x").toDouble();
        double cy = obj.value("y").toDouble();
        double r = obj.value("r").toDouble();
        QString label = obj.value("label").toString();
        if (r > 0.0) {
            circles.append(Circle(QPointF(cx, cy), r, label));
        }
    }
    update();
    return true;
}

bool CanvasWidget::writePointsToPath(const QString &path) const {
    if (path.isEmpty()) {
        return false;
    }
    QJsonArray pointsArr;
    for (const auto &entry : points) {
        QJsonObject obj;
        obj.insert("x", entry.positiom.x());
        obj.insert("y", entry.positiom.y());
        obj.insert("label", entry.label);
        pointsArr.append(obj);
    }
    QJsonArray linesArr;
    for (const auto &line : lines) {
        QJsonObject obj;
        obj.insert("a", line.a);
        obj.insert("b", line.b);
        obj.insert("label", line.label);
        linesArr.append(obj);
    }
    QJsonArray extendedArr;
    for (const auto &line : extendedLines) {
        QJsonObject obj;
        obj.insert("ax", line.a.x());
        obj.insert("ay", line.a.y());
        obj.insert("bx", line.b.x());
        obj.insert("by", line.b.y());
        obj.insert("label", line.label);
        extendedArr.append(obj);
    }
    QJsonArray circlesArr;
    for (const auto &circle : circles) {
        QJsonObject obj;
        obj.insert("x", circle.center.x());
        obj.insert("y", circle.center.y());
        obj.insert("r", circle.radius);
        obj.insert("label", circle.label);
        circlesArr.append(obj);
    }
    QJsonObject root;
    root.insert("points", pointsArr);
    root.insert("lines", linesArr);
    root.insert("extendedLines", extendedArr);
    root.insert("circles", circlesArr);
    QJsonDocument doc(root);

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    bool ok = false;
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        ok = true;
    }
    return ok;
}

bool CanvasWidget::loadFromFile(const QString &path) {
    if (path.isEmpty()) {
        return false;
    }
    if (!loadPointsFromFile(path)) {
        return false;
    }
    storagePath = path;
    return true;
}

bool CanvasWidget::saveToFile(const QString &path) {
    if (path.isEmpty()) {
        return false;
    }
    if (!writePointsToPath(path)) {
        return false;
    }
    storagePath = path;
    return true;
}
