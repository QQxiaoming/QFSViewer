#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QRadioButton>


#include "qfsviewer.h"
#include "configFile.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
private:
    void do_list_fs(const QString &imgFile);

private slots:
    void on_pushButton_clicked();
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();

    void on_actionHelp_triggered();
    void on_actionAbout_triggered();
    void on_actionAboutQt_triggered();

private:
    FSViewWindow *fsView;
    QMap<QString, QRadioButton *> fsTypeMap;
    ConfigFile *QFSViewerConfigFile;
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
