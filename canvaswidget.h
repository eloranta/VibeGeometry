#pragma once

#include <QWidget>
#include <QVector>
#include <QLineF>
#include <QPair>

class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget *parent = nullptr);

    void addLine(const QPointF &start, const QPointF &end);
    void addCircle(const QPointF &center, double radius);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<QLineF> lines_;
    QVector<QPair<QPointF, double>> circles_;
};
