#include "mainwindow.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QPointF>

#include "canvaswidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    canvas_ = new CanvasWidget(central);

    layout->addWidget(canvas_, 1);
    auto *controls = new QHBoxLayout();
    controls->setSpacing(8);
    auto *addLineBtn = new QPushButton("Add Line", central);
    auto *addCircleBtn = new QPushButton("Add Circle", central);
    controls->addStretch(1);
    controls->addWidget(addLineBtn);
    controls->addWidget(addCircleBtn);
    controls->addStretch(1);
    layout->addLayout(controls);

    connect(addLineBtn, &QPushButton::clicked, this, &MainWindow::showAddLineDialog);

    setCentralWidget(central);
}

void MainWindow::showAddLineDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("Add Line");

    auto *form = new QFormLayout(&dialog);
    auto *x1 = new QDoubleSpinBox(&dialog);
    auto *y1 = new QDoubleSpinBox(&dialog);
    auto *x2 = new QDoubleSpinBox(&dialog);
    auto *y2 = new QDoubleSpinBox(&dialog);

    const double min = -1000.0;
    const double max = 1000.0;
    for (auto *spin : {x1, y1, x2, y2}) {
        spin->setRange(min, max);
        spin->setDecimals(3);
        spin->setSingleStep(0.1);
    }

    x1->setValue(-1.0);
    y1->setValue(0.0);
    x2->setValue(1.0);
    y2->setValue(0.0);

    form->addRow("Start X (-5..5):", x1);
    form->addRow("Start Y (-5..5):", y1);
    form->addRow("End X (-5..5):", x2);
    form->addRow("End Y (-5..5):", y2);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        canvas_->addLine(QPointF(x1->value(), y1->value()), QPointF(x2->value(), y2->value()));
    }
}
