#pragma once

#include <QMainWindow>
#include <QPointF>

class CanvasWidget;
class QPushButton;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    CanvasWidget *canvas_ = nullptr;
    int pointCounter_ = 1;
    bool recording_ = false;
    QPushButton *recordBtn_ = nullptr;
    QPushButton *runBtn_ = nullptr;
    QString lastScriptPath_;
    QStringList recordedCommands_;
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
    void onRecordClicked();
    void onRunClicked();
    void onOpenMacroClicked();
    void onSaveMacroClicked();
    void onPointAdded(const QPointF &pt);
    void onPrintClicked();
};
