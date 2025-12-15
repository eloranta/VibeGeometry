#pragma once

#include <QMainWindow>

class CanvasWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    CanvasWidget *canvas_ = nullptr;

    void showAddLineDialog();
};
