#pragma once

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QString>
#include <QSet>
#include <QMouseEvent>

class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    explicit CanvasWidget(const QString &storagePath = QString(), QWidget *parent = nullptr);
    bool addPoint(const QPointF &point, const QString &label);
    bool hasPoint(const QPointF &point) const;
    int pointCount() const;
    bool addLineBetweenSelected(const QString &label = QString());
    bool extendSelectedLines();
    bool addCircle(const QPointF &center, double radius);
    bool selectedPoint(QPointF &point) const;
    bool addNormalAtPoint(int lineIndex, const QPointF &point);
    QList<int> selectedIndices() const { return selectedPointIndices.values(); }
    QList<int> selectedPointsOrdered() const { return pointSelectionOrder; }
    int selectedLineIndex() const { return selectedLineIndices.isEmpty() ? -1 : *selectedLineIndices.constBegin(); }
    int selectedExtendedLineIndex() const { return selectedExtendedLineIndices.isEmpty() ? -1 : *selectedExtendedLineIndices.constBegin(); }
    int selectedExtendedLineCount() const { return selectedExtendedLineIndices.size(); }
    QPointF pointAt(int index) const { return points.at(index).positiom; }
    bool setLabelForSelection(const QString &label);
    bool deleteSelected();
    void deleteAll();
    int selectedCount() const;
    int selectedLineCount() const;
    int selectedCircleCount() const;
    QString suggestedLineLabel() const;
    void recomputeAllIntersections();
    void recomputeSelectedIntersections();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    struct Object {
        QString label;
        explicit Object(const QString &label = QString()) : label(label) {}
        virtual ~Object() = default;
    };
    struct Point : public Object {
        QPointF positiom;
        Point() = default;
        Point(const QPointF &point, const QString &label) : Object(label), positiom(point) {}
    };
    struct Line : public Object {
        int a = -1;
        int b = -1;
        Line() = default;
        Line(int a, int b, const QString &label) : Object(label), a(a), b(b) {}
    };
    struct ExtendedLine : public Object {
        QPointF a;
        QPointF b;
        ExtendedLine() = default;
        ExtendedLine(const QPointF &a, const QPointF &b, const QString &label) : Object(label), a(a), b(b) {}
    };
    struct Circle : public Object {
        QPointF center;
        double radius = 0.0;
        Circle() = default;
        Circle(const QPointF &center, double radius, const QString &label = QString()) : Object(label), center(center), radius(radius) {}
    };

    QVector<Point> points;
    QVector<Line> lines;
    QVector<ExtendedLine> extendedLines;
    QVector<Circle> circles;
    QString storagePath;
    QSet<int> selectedPointIndices;
    QSet<int> selectedLineIndices;
    QSet<int> selectedExtendedLineIndices;
    QSet<int> selectedCircleIndices;
    QList<int> pointSelectionOrder;

    void loadPointsFromFile();
    void savePointsToFile() const;
    void addIntersectionPoint(const QPointF &pt);
    QString nextPointLabel() const;
    QString nextLineLabel() const;
    QString nextCircleLabel() const;
    std::pair<QPointF, QPointF> lineEndpoints(const Line &line) const;
    std::pair<QPointF, QPointF> extendedLineEndpoints(const ExtendedLine &line) const;
    void findIntersectionsForLine(int lineIndex);
    void findIntersectionsForExtendedLine(int lineIndex);
    void findIntersectionsForCircle(int circleIndex);
};
