/*
 * This file is part of the https://github.com/QQxiaoming/QFSViewer.git
 * project.
 *
 * Copyright (C) 2023 Quard <2014500726@smail.xtu.edu.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
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
    void do_list_fs(const QString &imgFile, uint64_t offset, uint64_t size);

private slots:
    void on_pushButton_clicked();
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();

    void on_actionHelp_triggered();
    void on_actionAbout_triggered();
    void on_actionAboutQt_triggered();

    void on_actionEnglish_triggered();
    void on_actionChinese_triggered();
    void on_actionJapanese_triggered();
    void on_actionDark_triggered();
    void on_actionLight_triggered();

private:
    FSViewWindow *fsView;
    QMap<QString, QRadioButton *> fsTypeMap;
    ConfigFile *QFSViewerConfigFile;
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
