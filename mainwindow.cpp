#include "mainwindow.h"

#include <QVBoxLayout>
#include <QWidget>

#include "canvaswidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    canvas_ = new CanvasWidget(central);

    layout->addWidget(canvas_, 1);

    setCentralWidget(central);
}
