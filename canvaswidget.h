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
    bool addLineBetweenSelected();
    bool extendSelectedLines();
    bool addCircle(const QPointF &center, double radius);
    bool selectedPoint(QPointF &point) const;
    int selectedCount() const;
    int selectedLineCount() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    struct PointEntry {
        QPointF pos;
        QString label;
    };
    struct LineEntry {
        int a;
        int b;
        bool extended = false;
    };
    struct CircleEntry {
        QPointF center;
        double radius = 0.0;
    };
    QVector<PointEntry> points_;
    QVector<LineEntry> lines_;
    QVector<CircleEntry> circles_;
    QString storagePath_;
    QSet<int> selectedIndices_;
    QSet<int> selectedLineIndices_;

    void loadPointsFromFile();
    void savePointsToFile() const;
    void addIntersectionPoint(const QPointF &pt);
    QString nextPointLabel() const;
    std::pair<QPointF, QPointF> lineEndpoints(const LineEntry &line) const;
    void findIntersectionsForLine(int lineIndex);
    void findIntersectionsForCircle(int circleIndex);
};
