#include "mainwindow.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QPointF>
#include <QCoreApplication>
#include <QDir>
#include <QtMath>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>

#include "canvaswidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    QDir appDir(QCoreApplication::applicationDirPath());
    const QString storagePath = appDir.filePath("points.json");
    canvas_ = new CanvasWidget(storagePath, central);

    layout->addWidget(canvas_, 1);
    pointCounter_ = canvas_->pointCount() + 1;

    // Menu bar with File -> Print
    QMenu *fileMenu = menuBar()->addMenu(tr("File"));
    QAction *printAction = fileMenu->addAction(tr("Print..."));
    connect(printAction, &QAction::triggered, this, &MainWindow::onPrintClicked);

    auto *controls = new QHBoxLayout();
    controls->setSpacing(8);
    auto *addLineBtn = new QPushButton("Connect", central);
    auto *extendLineBtn = new QPushButton("Extend", central);
    auto *addCircleBtn = new QPushButton("Circle", central);
    auto *intersectBtn = new QPushButton("Normal", central);
    auto *intersectionsBtn = new QPushButton("Intersect", central);
    auto *deleteBtn = new QPushButton("Delete", central);
    auto *deleteAllBtn = new QPushButton("Delete All", central);
    controls->addWidget(addLineBtn);
    controls->addWidget(extendLineBtn);
    controls->addWidget(addCircleBtn);
    controls->addWidget(intersectBtn);
    controls->addWidget(intersectionsBtn);
    controls->addWidget(deleteBtn);
    controls->addWidget(deleteAllBtn);
    controls->addStretch(1);
    layout->addLayout(controls);

    connect(addLineBtn, &QPushButton::clicked, this, &MainWindow::onAddLineClicked);
    connect(extendLineBtn, &QPushButton::clicked, this, &MainWindow::onExtendLineClicked);
    connect(addCircleBtn, &QPushButton::clicked, this, &MainWindow::onAddCircleClicked);
    connect(intersectBtn, &QPushButton::clicked, this, &MainWindow::onIntersectClicked);
    connect(intersectionsBtn, &QPushButton::clicked, this, &MainWindow::onIntersectionsClicked);
    connect(deleteBtn, &QPushButton::clicked, this, &MainWindow::onDeleteClicked);
    connect(deleteAllBtn, &QPushButton::clicked, this, &MainWindow::onDeleteAllClicked);

    setCentralWidget(central);
}

void MainWindow::onAddLineClicked() {
    if (canvas_->selectedCount() < 2) {
        QMessageBox::information(this, "Select Points", "Select at least two points (Ctrl+click to multi-select) to add a line.");
        return;
    }
    if (!canvas_->addLineBetweenSelected()) {
        QMessageBox::information(this, "Line Exists", "A line between those points already exists.");
    }
    pointCounter_ = canvas_->pointCount() + 1;
}

void MainWindow::onExtendLineClicked() {
    if (canvas_->selectedLineCount() < 1) {
        QMessageBox::information(this, "Select Line", "Select at least one line to extend (click or Ctrl+click).");
        return;
    }
    if (!canvas_->extendSelectedLines()) {
        QMessageBox::information(this, "Extend Line", "No lines were extended (they may already be extended).");
    }
    pointCounter_ = canvas_->pointCount() + 1;
}

void MainWindow::onAddCircleClicked() {
    if (canvas_->selectedCount() != 2) {
        QMessageBox::information(this, "Select Points", "Select exactly two points (Ctrl+click) to define center and radius.");
        return;
    }
    QList<int> indices = canvas_->selectedPointsOrdered();
    if (indices.size() != 2) {
        indices = canvas_->selectedIndices();
        std::sort(indices.begin(), indices.end());
    }
    QPointF center = canvas_->pointAt(indices[0]);
    QPointF edge = canvas_->pointAt(indices[1]);
    double r = std::hypot(center.x() - edge.x(), center.y() - edge.y());
    if (r <= 0.0) {
        QMessageBox::information(this, "Invalid Radius", "The two points must not be identical.");
        return;
    }
    canvas_->addCircle(center, r);
    pointCounter_ = canvas_->pointCount() + 1;
}

void MainWindow::onDeleteClicked() {
    if (!canvas_->deleteSelected()) {
        QMessageBox::information(this, "Delete", "No selected objects to delete.");
        return;
    }
    pointCounter_ = canvas_->pointCount() + 1;
}

void MainWindow::onDeleteAllClicked() {
    canvas_->deleteAll();
    pointCounter_ = canvas_->pointCount() + 1;
}

void MainWindow::onIntersectClicked() {
    if (canvas_->selectedLineCount() != 1 || canvas_->selectedCount() != 1) {
        QMessageBox::information(this, "Select Line and Point", "Select exactly one line and one point.");
        return;
    }
    int lineIdx = canvas_->selectedLineIndex();
    int pointIdx = canvas_->selectedIndices().first();
    if (lineIdx < 0 || pointIdx < 0) {
        QMessageBox::information(this, "Selection", "Invalid selection.");
        return;
    }
    QPointF p = canvas_->pointAt(pointIdx);
    if (!canvas_->addNormalAtPoint(lineIdx, p)) {
        QMessageBox::information(this, "Intersect", "Could not add normal line.");
    } else {
        pointCounter_ = canvas_->pointCount() + 1;
    }
}

void MainWindow::onIntersectionsClicked() {
    canvas_->recomputeSelectedIntersections();
    pointCounter_ = canvas_->pointCount() + 1;
}

void MainWindow::onPrintClicked() {
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Print Canvas"));
    if (dialog.exec() == QDialog::Accepted) {
        QPainter painter(&printer);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QRectF pageRect = printer.pageRect(QPrinter::DevicePixel);
        QSizeF widgetSize = canvas_->size();
        double xScale = pageRect.width() / widgetSize.width();
        double yScale = pageRect.height() / widgetSize.height();
        double scale = std::min(xScale, yScale);
        painter.translate(pageRect.x() + (pageRect.width() - widgetSize.width() * scale) / 2.0,
                          pageRect.y() + (pageRect.height() - widgetSize.height() * scale) / 2.0);
        painter.scale(scale, scale);
        canvas_->render(&painter);
    }
}
