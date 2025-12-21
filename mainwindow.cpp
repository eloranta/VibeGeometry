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
#include <QInputDialog>
#include <QPushButton>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QStyle>
#include <QTimer>
#include <QEventLoop>
#include <QVBoxLayout>
#include <QWidget>
#include <QPointF>
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

    canvas_ = new CanvasWidget(QString(), central);

    layout->addWidget(canvas_, 1);
    pointCounter_ = canvas_->pointCount() + 1;

    // Menu bar with File -> Print
    QMenu *fileMenu = menuBar()->addMenu(tr("File"));
    QAction *openAction = fileMenu->addAction(tr("Open..."));
    QAction *saveAsAction = fileMenu->addAction(tr("Save As..."));
    QAction *openMacroAction = fileMenu->addAction(tr("Open Macro..."));
    QAction *saveMacroAction = fileMenu->addAction(tr("Save Macro..."));
    fileMenu->addSeparator();
    QAction *printAction = fileMenu->addAction(tr("Print..."));
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFileClicked);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::onSaveAsClicked);
    connect(openMacroAction, &QAction::triggered, this, &MainWindow::onOpenMacroClicked);
    connect(saveMacroAction, &QAction::triggered, this, &MainWindow::onSaveMacroClicked);
    connect(printAction, &QAction::triggered, this, &MainWindow::onPrintClicked);

    auto *controls = new QHBoxLayout();
    controls->setSpacing(8);
    auto *addLineBtn = new QPushButton("Connect", central);
    auto *extendLineBtn = new QPushButton("Extend", central);
    auto *addCircleBtn = new QPushButton("Circle", central);
    auto *intersectBtn = new QPushButton("Normal", central);
    auto *intersectionsBtn = new QPushButton("Intersect", central);
    auto *labelBtn = new QPushButton("Label", central);
    recordBtn_ = new QPushButton("Record", central);
    recordBtn_->setCheckable(true);
    recordBtn_->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton));
    runBtn_ = new QPushButton("Run", central);
    runBtn_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    auto *deleteBtn = new QPushButton("Delete", central);
    auto *deleteAllBtn = new QPushButton("Delete All", central);
    controls->addWidget(addLineBtn);
    controls->addWidget(extendLineBtn);
    controls->addWidget(addCircleBtn);
    controls->addWidget(intersectBtn);
    controls->addWidget(intersectionsBtn);
    controls->addWidget(labelBtn);
    controls->addWidget(recordBtn_);
    controls->addWidget(runBtn_);
    controls->addWidget(deleteBtn);
    controls->addWidget(deleteAllBtn);
    controls->addStretch(1);
    layout->addLayout(controls);

    connect(addLineBtn, &QPushButton::clicked, this, &MainWindow::onAddLineClicked);
    connect(extendLineBtn, &QPushButton::clicked, this, &MainWindow::onExtendLineClicked);
    connect(addCircleBtn, &QPushButton::clicked, this, &MainWindow::onAddCircleClicked);
    connect(intersectBtn, &QPushButton::clicked, this, &MainWindow::onIntersectClicked);
    connect(intersectionsBtn, &QPushButton::clicked, this, &MainWindow::onIntersectionsClicked);
    connect(labelBtn, &QPushButton::clicked, this, &MainWindow::onEditLabelClicked);
    connect(recordBtn_, &QPushButton::clicked, this, &MainWindow::onRecordClicked);
    connect(runBtn_, &QPushButton::clicked, this, &MainWindow::onRunClicked);
    connect(deleteBtn, &QPushButton::clicked, this, &MainWindow::onDeleteClicked);
    connect(deleteAllBtn, &QPushButton::clicked, this, &MainWindow::onDeleteAllClicked);
    connect(canvas_, &CanvasWidget::pointAdded, this, &MainWindow::onPointAdded);

    setCentralWidget(central);
}

void MainWindow::onAddLineClicked() {
    if (canvas_->selectedCount() < 2) {
        QMessageBox::information(this, "Select Points", "Select at least two points (Ctrl+click to multi-select) to add a line.");
        return;
    }
    if (!canvas_->addLineBetweenSelected()) {
        QMessageBox::information(this, "Line Exists", "A line between those points already exists.");
        return;
    }
    pointCounter_ = canvas_->pointCount() + 1;
    if (recording_) {
        QList<int> indices = canvas_->selectedPointsOrdered();
        if (indices.size() < 2) {
            indices = canvas_->selectedIndices();
            std::sort(indices.begin(), indices.end());
        }
        if (indices.size() >= 2) {
            QPointF a = canvas_->pointAt(indices[0]);
            QPointF b = canvas_->pointAt(indices[1]);
            recordedCommands_.append(QStringLiteral("addLine:%1,%2|%3,%4")
                                         .arg(a.x(), 0, 'f', 8)
                                         .arg(a.y(), 0, 'f', 8)
                                         .arg(b.x(), 0, 'f', 8)
                                         .arg(b.y(), 0, 'f', 8));
        }
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
    pointCounter_ = canvas_->pointCount() + 1;
    if (recording_) recordedCommands_.append(QStringLiteral("extendLines"));
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
    if (recording_) {
        recordedCommands_.append(QStringLiteral("addCircle:%1,%2|%3,%4")
                                     .arg(center.x(), 0, 'f', 8)
                                     .arg(center.y(), 0, 'f', 8)
                                     .arg(edge.x(), 0, 'f', 8)
                                     .arg(edge.y(), 0, 'f', 8));
    }
}

void MainWindow::onDeleteClicked() {
    QString recordedCmd;
    if (recording_) {
        QStringList fields;
        auto pts = canvas_->selectedPointPositions();
        if (!pts.isEmpty()) {
            QStringList entries;
            for (const auto &p : pts) entries.append(QStringLiteral("%1,%2").arg(p.x(), 0, 'f', 8).arg(p.y(), 0, 'f', 8));
            fields.append(QStringLiteral("P=%1").arg(entries.join("|")));
        }
        auto lines = canvas_->selectedLineEndpoints();
        if (!lines.isEmpty()) {
            QStringList entries;
            for (const auto &l : lines) entries.append(QStringLiteral("%1,%2|%3,%4")
                                                           .arg(l.first.x(), 0, 'f', 8)
                                                           .arg(l.first.y(), 0, 'f', 8)
                                                           .arg(l.second.x(), 0, 'f', 8)
                                                           .arg(l.second.y(), 0, 'f', 8));
            fields.append(QStringLiteral("L=%1").arg(entries.join("#")));
        }
        auto ext = canvas_->selectedExtendedLineEndpoints();
        if (!ext.isEmpty()) {
            QStringList entries;
            for (const auto &l : ext) entries.append(QStringLiteral("%1,%2|%3,%4")
                                                         .arg(l.first.x(), 0, 'f', 8)
                                                         .arg(l.first.y(), 0, 'f', 8)
                                                         .arg(l.second.x(), 0, 'f', 8)
                                                         .arg(l.second.y(), 0, 'f', 8));
            fields.append(QStringLiteral("E=%1").arg(entries.join("#")));
        }
        auto circs = canvas_->selectedCircleData();
        if (!circs.isEmpty()) {
            QStringList entries;
            for (const auto &c : circs) entries.append(QStringLiteral("%1,%2,%3")
                                                           .arg(c.first.x(), 0, 'f', 8)
                                                           .arg(c.first.y(), 0, 'f', 8)
                                                           .arg(c.second, 0, 'f', 8));
            fields.append(QStringLiteral("C=%1").arg(entries.join("#")));
        }
        recordedCmd = QStringLiteral("deleteSelected");
        if (!fields.isEmpty()) {
            recordedCmd += QStringLiteral(";%1").arg(fields.join(";"));
        }
    }

    if (!canvas_->deleteSelected()) {
        QMessageBox::information(this, "Delete", "No selected objects to delete.");
        return;
    }
    pointCounter_ = canvas_->pointCount() + 1;
    if (recording_ && !recordedCmd.isEmpty()) recordedCommands_.append(recordedCmd);
}

void MainWindow::onDeleteAllClicked() {
    canvas_->deleteAll();
    pointCounter_ = canvas_->pointCount() + 1;
    if (recording_) recordedCommands_.append(QStringLiteral("deleteAll"));
}

void MainWindow::onOpenFileClicked() {
    QString startPath = canvas_->storageFilePath();
    QString initialDir = startPath.isEmpty() ? QDir::currentPath() : QFileInfo(startPath).absolutePath();
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open Points File"), initialDir,
                                                    tr("JSON Files (*.json);;All Files (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }
    if (!canvas_->loadFromFile(filePath)) {
        QMessageBox::warning(this, tr("Open File"), tr("Could not open or parse the selected file."));
        return;
    }
    pointCounter_ = canvas_->pointCount() + 1;
    if (recording_) recordedCommands_.append(QStringLiteral("open:%1").arg(filePath));
}

void MainWindow::onSaveAsClicked() {
    QString startPath = canvas_->storageFilePath();
    if (startPath.isEmpty()) {
        startPath = QDir::currentPath();
    }
    QString filePath = QFileDialog::getSaveFileName(this, tr("Save Points As"), startPath,
                                                    tr("JSON Files (*.json);;All Files (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }
    if (!filePath.endsWith(".json", Qt::CaseInsensitive)) {
        filePath += ".json";
    }
    if (!canvas_->saveToFile(filePath)) {
        QMessageBox::warning(this, tr("Save File"), tr("Could not save to the selected location."));
    } else if (recording_) {
        recordedCommands_.append(QStringLiteral("save:%1").arg(filePath));
    }
}

void MainWindow::onOpenMacroClicked() {
    QString initial = lastScriptPath_.isEmpty() ? QDir::currentPath() : QFileInfo(lastScriptPath_).absolutePath();
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open Macro"), initial,
                                                    tr("Macro Files (*.txt *.macro);;All Files (*.*)"));
    if (filePath.isEmpty()) return;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Open Macro"), tr("Could not open the macro file."));
        return;
    }
    QStringList lines;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }
    file.close();
    recordedCommands_ = lines;
    lastScriptPath_ = filePath;
}

void MainWindow::onSaveMacroClicked() {
    QString initial = lastScriptPath_.isEmpty() ? QDir::currentPath() : lastScriptPath_;
    QString filePath = QFileDialog::getSaveFileName(this, tr("Save Macro"), initial,
                                                    tr("Macro Files (*.txt *.macro);;All Files (*.*)"));
    if (filePath.isEmpty()) return;
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Save Macro"), tr("Could not save the macro file."));
        return;
    }
    QTextStream out(&file);
    for (const auto &cmd : recordedCommands_) {
        out << cmd << "\n";
    }
    file.close();
    lastScriptPath_ = filePath;
}

void MainWindow::onRecordClicked() {
    recording_ = recordBtn_ ? recordBtn_->isChecked() : !recording_;
    if (recordBtn_) {
        recordBtn_->setText(recording_ ? tr("Stop Record") : tr("Record"));
        recordBtn_->setIcon(style()->standardIcon(recording_ ? QStyle::SP_MediaStop : QStyle::SP_DialogYesButton));
    }
    if (recording_) {
        recordedCommands_.clear();
    }
}

void MainWindow::onRunClicked() {
    if (recording_) {
        recording_ = false;
        if (recordBtn_) {
            recordBtn_->setChecked(false);
            recordBtn_->setText(tr("Record"));
            recordBtn_->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton));
        }
    }
    if (recordedCommands_.isEmpty()) {
        QMessageBox::information(this, tr("Run"), tr("No recorded commands to run."));
        return;
    }

    // Prevent re-recording during playback
    const bool wasRecording = recording_;
    recording_ = false;
    for (int i = 0; i < recordedCommands_.size(); ++i) {
        const QString &cmd = recordedCommands_[i];
        if (cmd == "extendLines") {
            onExtendLineClicked();
        } else if (cmd == "addCircle") {
            onAddCircleClicked();
        } else if (cmd.startsWith("deleteSelected")) {
            canvas_->clearSelection();
            QStringList parts = cmd.split(';');
            if (parts.size() > 1) {
                auto toPoint = [](const QString &s, bool &okOut) {
                    const QStringList coords = s.split(',');
                    bool ok1 = false, ok2 = false;
                    double x = coords.value(0).toDouble(&ok1);
                    double y = coords.value(1).toDouble(&ok2);
                    okOut = ok1 && ok2;
                    return QPointF(x, y);
                };
                for (int idx = 1; idx < parts.size(); ++idx) {
                    const QString &field = parts[idx];
                    if (field.startsWith("P=")) {
                        const QStringList items = field.mid(2).split('|', Qt::SkipEmptyParts);
                        for (const QString &it : items) {
                            bool ok = false;
                            QPointF p = toPoint(it, ok);
                            if (ok) canvas_->selectPointByPosition(p, true);
                        }
                    } else if (field.startsWith("L=")) {
                        const QStringList items = field.mid(2).split('#', Qt::SkipEmptyParts);
                        for (const QString &it : items) {
                            const QStringList pair = it.split('|');
                            if (pair.size() == 2) {
                                bool okA = false, okB = false;
                                QPointF a = toPoint(pair[0], okA);
                                QPointF b = toPoint(pair[1], okB);
                                if (okA && okB) canvas_->selectLineByEndpoints(a, b, true);
                            }
                        }
                    } else if (field.startsWith("E=")) {
                        const QStringList items = field.mid(2).split('#', Qt::SkipEmptyParts);
                        for (const QString &it : items) {
                            const QStringList pair = it.split('|');
                            if (pair.size() == 2) {
                                bool okA = false, okB = false;
                                QPointF a = toPoint(pair[0], okA);
                                QPointF b = toPoint(pair[1], okB);
                                if (okA && okB) canvas_->selectExtendedLineByEndpoints(a, b, true);
                            }
                        }
                    } else if (field.startsWith("C=")) {
                        const QStringList items = field.mid(2).split('#', Qt::SkipEmptyParts);
                        for (const QString &it : items) {
                            const QStringList partsC = it.split(',');
                            if (partsC.size() == 3) {
                                bool ok1 = false, ok2 = false, ok3 = false;
                                double cx = partsC.value(0).toDouble(&ok1);
                                double cy = partsC.value(1).toDouble(&ok2);
                                double r = partsC.value(2).toDouble(&ok3);
                                if (ok1 && ok2 && ok3) {
                                    canvas_->selectCircleByCenterRadius(QPointF(cx, cy), r, true);
                                }
                            }
                        }
                    }
                }
            }
            canvas_->deleteSelected();
            pointCounter_ = canvas_->pointCount() + 1;
        } else if (cmd == "deleteAll") {
            onDeleteAllClicked();
        } else if (cmd == "addNormal") {
            onIntersectClicked();
        } else if (cmd == "intersections") {
            onIntersectionsClicked();
        } else if (cmd.startsWith("addPoint:")) {
            const QString coords = cmd.mid(QStringLiteral("addPoint:").size());
            const QStringList parts = coords.split(',');
            if (parts.size() == 2) {
                bool okX = false, okY = false;
                double x = parts[0].toDouble(&okX);
                double y = parts[1].toDouble(&okY);
                if (okX && okY) {
                    canvas_->addPoint(QPointF(x, y), QString(), true);
                    pointCounter_ = canvas_->pointCount() + 1;
                }
            }
        } else if (cmd.startsWith("setLabel:")) {
            const QString label = cmd.mid(QStringLiteral("setLabel:").size());
            canvas_->setLabelForSelection(label);
        } else if (cmd.startsWith("open:")) {
            const QString path = cmd.mid(QStringLiteral("open:").size());
            canvas_->loadFromFile(path);
            pointCounter_ = canvas_->pointCount() + 1;
        } else if (cmd.startsWith("save:")) {
            const QString path = cmd.mid(QStringLiteral("save:").size());
            canvas_->saveToFile(path);
        } else if (cmd.startsWith("addNormal:")) {
            const QString payload = cmd.mid(QStringLiteral("addNormal:").size());
            const QStringList parts = payload.split(';');
            if (parts.size() == 2) {
                const QStringList lineParts = parts[0].split('|');
                if (lineParts.size() == 2) {
                    const auto toPt = [](const QString &s, bool &okOut) {
                        const QStringList coords = s.split(',');
                        bool ok1 = false, ok2 = false;
                        double x = coords.value(0).toDouble(&ok1);
                        double y = coords.value(1).toDouble(&ok2);
                        okOut = ok1 && ok2;
                        return QPointF(x, y);
                    };
                    bool okA = false, okB = false, okP = false;
                    QPointF a = toPt(lineParts[0], okA);
                    QPointF b = toPt(lineParts[1], okB);
                    QPointF p = toPt(parts[1], okP);
                    if (okA && okB && okP) {
                        canvas_->clearSelection();
                        bool selLine = canvas_->selectLineByEndpoints(a, b, false);
                        bool selPoint = canvas_->selectPointByPosition(p, true);
                        if (selLine && selPoint) {
                            onIntersectClicked();
                            pointCounter_ = canvas_->pointCount() + 1;
                        }
                    }
                }
            }
        } else if (cmd.startsWith("addLine:")) {
            const QString coords = cmd.mid(QStringLiteral("addLine:").size());
            const QStringList pair = coords.split('|');
            if (pair.size() == 2) {
                const auto toPt = [](const QString &s, bool &okOut) {
                    const QStringList parts = s.split(',');
                    bool ok1 = false, ok2 = false;
                    double x = parts.value(0).toDouble(&ok1);
                    double y = parts.value(1).toDouble(&ok2);
                    okOut = ok1 && ok2;
                    return QPointF(x, y);
                };
                bool okA = false, okB = false;
                QPointF a = toPt(pair[0], okA);
                QPointF b = toPt(pair[1], okB);
                if (okA && okB) {
                    canvas_->clearSelection();
                    bool selA = canvas_->selectPointByPosition(a, false);
                    if (!selA) {
                        canvas_->addPoint(a, QString(), false);
                        selA = canvas_->selectPointByPosition(a, false);
                    }
                    bool selB = canvas_->selectPointByPosition(b, true);
                    if (!selB) {
                        canvas_->addPoint(b, QString(), true);
                        selB = canvas_->selectPointByPosition(b, true);
                    }
                    if (selA && selB) {
                        canvas_->addLineBetweenSelected();
                        pointCounter_ = canvas_->pointCount() + 1;
                    }
                }
            }
        } else if (cmd.startsWith("addCircle:")) {
            const QString coords = cmd.mid(QStringLiteral("addCircle:").size());
            const QStringList pair = coords.split('|');
            if (pair.size() == 2) {
                const auto toPt = [](const QString &s, bool &okOut) {
                    const QStringList parts = s.split(',');
                    bool ok1 = false, ok2 = false;
                    double x = parts.value(0).toDouble(&ok1);
                    double y = parts.value(1).toDouble(&ok2);
                    okOut = ok1 && ok2;
                    return QPointF(x, y);
                };
                bool okA = false, okB = false;
                QPointF a = toPt(pair[0], okA);
                QPointF b = toPt(pair[1], okB);
                if (okA && okB) {
                    canvas_->clearSelection();
                    bool selA = canvas_->selectPointByPosition(a, false);
                    bool selB = canvas_->selectPointByPosition(b, true);
                    if (selA && selB) {
                        onAddCircleClicked();
                        pointCounter_ = canvas_->pointCount() + 1;
                    }
                }
            }
        }
        // 1s delay between commands during playback
        if (i + 1 < recordedCommands_.size()) {
            QEventLoop loop;
            QTimer::singleShot(1000, &loop, &QEventLoop::quit);
            loop.exec();
        }
    }
    recording_ = wasRecording;
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
        if (recording_) {
            QPointF a, b;
            if (canvas_->lineEndpointsAt(lineIdx, a, b)) {
                recordedCommands_.append(QStringLiteral("addNormal:%1,%2|%3,%4;%5,%6")
                                             .arg(a.x(), 0, 'f', 8)
                                             .arg(a.y(), 0, 'f', 8)
                                             .arg(b.x(), 0, 'f', 8)
                                             .arg(b.y(), 0, 'f', 8)
                                             .arg(p.x(), 0, 'f', 8)
                                             .arg(p.y(), 0, 'f', 8));
            }
        }
    }
}

void MainWindow::onIntersectionsClicked() {
    canvas_->recomputeSelectedIntersections();
    pointCounter_ = canvas_->pointCount() + 1;
    if (recording_) recordedCommands_.append(QStringLiteral("intersections"));
}

void MainWindow::onEditLabelClicked() {
    int totalSelections = canvas_->selectedCount() + canvas_->selectedLineCount() +
                          canvas_->selectedExtendedLineCount() + canvas_->selectedCircleCount();
    if (totalSelections != 1) {
        QMessageBox::information(this, "Label", "Select exactly one item to edit its label.");
        return;
    }
    bool ok = false;
    QString text = QInputDialog::getText(this, "Edit Label", "Label:", QLineEdit::Normal, QString(), &ok);
    if (!ok) return;
    if (!canvas_->setLabelForSelection(text)) {
        QMessageBox::information(this, "Label", "Could not update the label.");
    } else if (recording_) {
        recordedCommands_.append(QStringLiteral("setLabel:%1").arg(text));
    }
}

void MainWindow::onPointAdded(const QPointF &pt) {
    if (!recording_) return;
    recordedCommands_.append(QStringLiteral("addPoint:%1,%2").arg(pt.x(), 0, 'f', 8).arg(pt.y(), 0, 'f', 8));
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
