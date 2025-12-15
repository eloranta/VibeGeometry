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
    int selectedCount() const;

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
    };
    QVector<PointEntry> points_;
    QVector<LineEntry> lines_;
    QString storagePath_;
    QSet<int> selectedIndices_;

    void loadPointsFromFile();
    void savePointsToFile() const;
};
