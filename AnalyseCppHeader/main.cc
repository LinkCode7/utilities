#include "main.h"

#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>

#include "WriteToEmbind.h"
#include "analyse.h"

#define CONFIG_PATH "setting.json"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    MainWindow window;
    window.show();
    return app.exec();
}

MainWindow::MainWindow(QWidget *parent) : QWidget(parent), _setting(std::make_shared<Setting>())
{
    std::string str;
    sindy::readContents(CONFIG_PATH, str);

    if (!str.empty())
        iguana::from_json(*_setting, str);

    _pathEdit = new QLineEdit();
    _pathEdit->setText(_setting->_directory.c_str());
    _pathEdit->setGeometry(100, 100, 2000, 30);

    auto pLabel = new QLabel("The root name of the output:");
    _rootEdit   = new QLineEdit();
    _rootEdit->setText(_setting->_rootName.c_str());
    QObject::connect(_rootEdit, &QLineEdit::textChanged, this, &MainWindow::onRootTextChanged);

    QPushButton *dir = new QPushButton("dir");
    QObject::connect(dir, &QPushButton::clicked, this, &MainWindow::onClickSelectDir);

    QPushButton *file = new QPushButton("file");
    QObject::connect(file, &QPushButton::clicked, this, &MainWindow::onClickSelectFile);

    _runButton = new QPushButton("run");
    _runButton->setGeometry(100, 100, 50, 25);
    QObject::connect(_runButton, &QPushButton::clicked, this, &MainWindow::onClickRun);

    QHBoxLayout *layoutH = new QHBoxLayout;
    layoutH->addWidget(_pathEdit);
    layoutH->addWidget(dir);
    layoutH->addWidget(file);

    QHBoxLayout *layoutH2 = new QHBoxLayout;
    layoutH2->addWidget(pLabel);
    layoutH2->addWidget(_rootEdit);

    QVBoxLayout *layoutV = new QVBoxLayout;
    layoutV->addWidget(_runButton);

    QGridLayout *topLayout = new QGridLayout;
    topLayout->addLayout(layoutH, 0, 0);
    topLayout->addLayout(layoutH2, 1, 0);
    topLayout->addLayout(layoutV, 2, 0);

    this->setLayout(topLayout);
}

void MainWindow::onRootTextChanged()
{
    _setting->_rootName = _rootEdit->text().toStdString();
    _setting->_modify   = true;
}
void MainWindow::onClickSelectDir()
{
    QString dir =
        QFileDialog::getExistingDirectory(this, "select directory", _setting->_directory.c_str(), QFileDialog::ShowDirsOnly);
    if (dir.isEmpty())
        return;

    _pathEdit->setText(dir);
    _setting->_modify = true;
}
void MainWindow::onClickSelectFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "select file", _setting->_directory.c_str(), "files(*.h)");
    if (fileName.isEmpty())
        return;

    _pathEdit->setText(fileName);
    _setting->_modify = true;
}

void MainWindow::onClickRun()
{
    auto strPath = _pathEdit->text().toStdString();
    if (_setting->_modify)
    {
        _setting->_directory = strPath;

        std::string json;
        iguana::to_json(*_setting, json);
        sindy::writeContents(CONFIG_PATH, json);
    }

    _runButton->setText("running...");
    _runButton->setEnabled(false);

    auto msg = WriteToEmbind(_setting).execute(strPath);
    if (!msg.empty())
    {
        QMessageBox messageBox(this);
        messageBox.setText(msg.c_str());
        messageBox.exec();
    }

    _runButton->setText("run");
    _runButton->setEnabled(true);
}
