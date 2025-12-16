#include "mainwindow.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QPointF>
#include <QCoreApplication>
#include <QDir>
#include <QtMath>

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
    auto *controls = new QHBoxLayout();
    controls->setSpacing(8);
    auto *addPointBtn = new QPushButton("Add Point", central);
    auto *addLineBtn = new QPushButton("Add Line", central);
    auto *extendLineBtn = new QPushButton("Extend Line", central);
    auto *addCircleBtn = new QPushButton("Add Circle", central);
    auto *intersectBtn = new QPushButton("Normal", central);
    auto *deleteBtn = new QPushButton("Delete", central);
    auto *deleteAllBtn = new QPushButton("Delete All", central);
    controls->addWidget(addPointBtn);
    controls->addWidget(addLineBtn);
    controls->addWidget(extendLineBtn);
    controls->addWidget(addCircleBtn);
    controls->addWidget(intersectBtn);
    controls->addWidget(deleteBtn);
    controls->addWidget(deleteAllBtn);
    controls->addStretch(1);
    layout->addLayout(controls);

    connect(addPointBtn, &QPushButton::clicked, this, &MainWindow::showAddPointDialog);
    connect(addLineBtn, &QPushButton::clicked, this, &MainWindow::onAddLineClicked);
    connect(extendLineBtn, &QPushButton::clicked, this, &MainWindow::onExtendLineClicked);
    connect(addCircleBtn, &QPushButton::clicked, this, &MainWindow::onAddCircleClicked);
    connect(intersectBtn, &QPushButton::clicked, this, &MainWindow::onIntersectClicked);
    connect(deleteBtn, &QPushButton::clicked, this, &MainWindow::onDeleteClicked);
    connect(deleteAllBtn, &QPushButton::clicked, this, &MainWindow::onDeleteAllClicked);

    setCentralWidget(central);
}

void MainWindow::showAddPointDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("Add Point");

    auto *form = new QFormLayout(&dialog);
    auto *x = new QDoubleSpinBox(&dialog);
    auto *y = new QDoubleSpinBox(&dialog);
    auto *labelEdit = new QLineEdit(&dialog);

    const double min = -1000.0;
    const double max = 1000.0;
    for (auto *spin : {x, y}) {
        spin->setRange(min, max);
        spin->setDecimals(3);
        spin->setSingleStep(0.1);
    }

    x->setValue(0.0);
    y->setValue(0.0);
    labelEdit->setText(QString("P%1").arg(pointCounter_));
    labelEdit->setMaxLength(16);

    form->addRow("X (-5..5):", x);
    form->addRow("Y (-5..5):", y);
    form->addRow("Label:", labelEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString label = labelEdit->text().trimmed();
        if (label.isEmpty()) {
            label = QString("P%1").arg(pointCounter_);
        }
        QPointF pt(x->value(), y->value());
        if (canvas_->hasPoint(pt)) {
            QMessageBox::information(this, "Point Exists", "A point at these coordinates already exists.");
            return;
        }
        if (canvas_->addPoint(pt, label)) {
            ++pointCounter_;
        }
    }
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
    QList<int> indices = canvas_->selectedIndices();
    std::sort(indices.begin(), indices.end());
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
