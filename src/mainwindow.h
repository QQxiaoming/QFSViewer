#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDialog>

#include "qfsviewer.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QDialog
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

private:
    FSViewWindow *fsView;
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
