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
    int pointCounter_ = 1;
    void onAddLineClicked();
    void onExtendLineClicked();
    void onAddCircleClicked();
    void onIntersectClicked();
    void onIntersectionsClicked();
    void onEditLabelClicked();
    void onDeleteClicked();
    void onDeleteAllClicked();
    void onOpenFileClicked();
    void onSaveAsClicked();
    void onPrintClicked();
};
