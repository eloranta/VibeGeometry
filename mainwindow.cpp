#include "mainwindow.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *title = new QLabel("VibeGeometry", central);
    title->setAlignment(Qt::AlignCenter);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto *subtitle = new QLabel("Starter Qt 6 widget project ready for your geometry visuals.", central);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setWordWrap(true);

    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addStretch(1);

    setCentralWidget(central);
}
