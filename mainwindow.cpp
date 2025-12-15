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
    controls->addWidget(addPointBtn);
    controls->addWidget(addLineBtn);
    controls->addWidget(extendLineBtn);
    controls->addWidget(addCircleBtn);
    controls->addStretch(1);
    layout->addLayout(controls);

    connect(addPointBtn, &QPushButton::clicked, this, &MainWindow::showAddPointDialog);
    connect(addLineBtn, &QPushButton::clicked, this, &MainWindow::onAddLineClicked);
    connect(extendLineBtn, &QPushButton::clicked, this, &MainWindow::onExtendLineClicked);
    connect(addCircleBtn, &QPushButton::clicked, this, &MainWindow::onAddCircleClicked);

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
}

void MainWindow::onExtendLineClicked() {
    if (canvas_->selectedLineCount() < 1) {
        QMessageBox::information(this, "Select Line", "Select at least one line to extend (click or Ctrl+click).");
        return;
    }
    if (!canvas_->extendSelectedLines()) {
        QMessageBox::information(this, "Extend Line", "No lines were extended (they may already be extended).");
    }
}

void MainWindow::onAddCircleClicked() {
    QPointF center;
    if (!canvas_->selectedPoint(center)) {
        QMessageBox::information(this, "Select Center", "Select a point to use as the circle center.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Add Circle");
    auto *form = new QFormLayout(&dialog);
    auto *radiusSpin = new QDoubleSpinBox(&dialog);
    radiusSpin->setRange(0.0, 10000.0);
    radiusSpin->setDecimals(3);
    radiusSpin->setSingleStep(0.1);
    radiusSpin->setValue(1.0);
    form->addRow("Radius:", radiusSpin);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        double r = radiusSpin->value();
        if (r <= 0.0) {
            QMessageBox::information(this, "Invalid Radius", "Radius must be greater than zero.");
            return;
        }
        canvas_->addCircle(center, r);
    }
}
