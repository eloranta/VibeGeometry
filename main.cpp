#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    MainWindow window;
    window.setWindowTitle("VibeGeometry");
    window.resize(420, 320);
    window.show();

    return app.exec();
}
