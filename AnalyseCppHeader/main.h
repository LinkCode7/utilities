#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

class QLineEdit;

#include <QMainWindow>
#include <QWidget>

#include "setting.h"
class QPushButton;

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);

private:
    void onClickRun();
    void onClickSelectDir();
    void onClickSelectFile();
    void onRootTextChanged();

private:
    QLineEdit*   _pathEdit  = nullptr;
    QLineEdit*   _rootEdit  = nullptr;
    QPushButton* _runButton = nullptr;

    std::shared_ptr<Setting> _setting = nullptr;
};

#endif // MAIN_WINDOW_H
