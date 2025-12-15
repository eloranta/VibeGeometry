#pragma once

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QString>
class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    explicit CanvasWidget(const QString &storagePath = QString(), QWidget *parent = nullptr);
    bool addPoint(const QPointF &point, const QString &label);
    bool hasPoint(const QPointF &point) const;
    int pointCount() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct PointEntry {
        QPointF pos;
        QString label;
    };
    QVector<PointEntry> points_;
    QString storagePath_;

    void loadPointsFromFile();
    void savePointsToFile() const;
};
