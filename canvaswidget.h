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
    QList<int> selectedIndices() const { return selectedIndices_.values(); }
    QList<int> selectedPointsOrdered() const { return pointSelectionOrder_; }
    int selectedLineIndex() const { return selectedLineIndices_.isEmpty() ? -1 : *selectedLineIndices_.constBegin(); }
    QPointF pointAt(int index) const { return points_.at(index).pos; }
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
        explicit Object(const QString &l = QString()) : label(l) {}
        virtual ~Object() = default;
    };
    struct Point : public Object {
        QPointF pos;
        Point() = default;
        Point(const QPointF &p, const QString &l) : Object(l), pos(p) {}
    };
    struct Line : public Object {
        int a = -1;
        int b = -1;
        bool custom = false;
        QPointF customA;
        QPointF customB;
        Line() = default;
        Line(int aIn, int bIn, const QString &lab, bool customIn = false, const QPointF &ca = QPointF(), const QPointF &cb = QPointF())
            : Object(lab), a(aIn), b(bIn), custom(customIn), customA(ca), customB(cb) {}
    };
    struct ExtendedLine : public Line {
        ExtendedLine() = default;
        ExtendedLine(int aIn, int bIn, const QString &lab, bool customIn = false, const QPointF &ca = QPointF(), const QPointF &cb = QPointF())
            : Line(aIn, bIn, lab, customIn, ca, cb) {}
    };
    struct Circle : public Object {
        QPointF center;
        double radius = 0.0;
        Circle() = default;
        Circle(const QPointF &c, double r, const QString &lab = QString()) : Object(lab), center(c), radius(r) {}
    };

    QVector<Point> points_;
    QVector<Line> lines_;
    QVector<Circle> circles_;
    QString storagePath_;
    QSet<int> selectedIndices_;
    QSet<int> selectedLineIndices_;
    QSet<int> selectedCircleIndices_;
    QList<int> pointSelectionOrder_;

    void loadPointsFromFile();
    void savePointsToFile() const;
    void addIntersectionPoint(const QPointF &pt);
    QString nextPointLabel() const;
    QString nextLineLabel() const;
    QString nextCircleLabel() const;
    std::pair<QPointF, QPointF> lineEndpoints(const Line &line) const;
    void findIntersectionsForLine(int lineIndex);
    void findIntersectionsForCircle(int circleIndex);
};
