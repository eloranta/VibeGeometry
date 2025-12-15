#pragma once

#include <QWidget>
#include <QVector>
#include <QLineF>

class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget *parent = nullptr);

    void addLine(const QPointF &start, const QPointF &end);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<QLineF> lines_;
};
