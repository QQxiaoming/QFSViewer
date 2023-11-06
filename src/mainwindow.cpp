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
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QApplication>
#include <QString>
#include <QTranslator>
#include <iostream>

#include "filedialog.h"
#include "qfonticon.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"


const static QString VERSION = APP_VERSION;
const static QString GIT_TAG =
#include "git_tag.inc"
;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("FSView");

    QLocale locale;
    QLocale::Language lang = locale.language();
    switch(lang) {
    case QLocale::Chinese:
        ui->actionChinese->setChecked(true);
        break;
    case QLocale::Japanese:
        ui->actionJapanese->setChecked(true);
        break;
    default:
    case QLocale::English:
        ui->actionEnglish->setChecked(true);
        break;
    }
   
    int text_hsv_value = QPalette().color(QPalette::WindowText).value();
    int bg_hsv_value = QPalette().color(QPalette::Window).value();
    bool isDarkTheme = text_hsv_value > bg_hsv_value?true:false;
    QString themeName;
    if(isDarkTheme) {
        ui->actionDark->setChecked(true);
    } else {
        ui->actionLight->setChecked(true);
    }
    
    QRect screen = QGuiApplication::screenAt(this->mapToGlobal(QPoint(this->width()/2,0)))->geometry();
    QRect size = this->geometry();
    this->move((screen.width() - size.width()) / 2, (screen.height() - size.height()) / 2);

    fsTypeMap = {
        {"jffs2", ui->radioButton_jffs2},
        {"fatX", ui->radioButton_fat},
        {"exfat", ui->radioButton_exfat},
        {"ext4", ui->radioButton_ext4},
        {"ext3", ui->radioButton_ext3},
        {"ext2", ui->radioButton_ext2},
    };

    QFileInfo dir(QDir::homePath()+"/.QFSViewer");
    if(!dir.isDir()) {
        if(!dir.isFile()) {
            QDir mkdir(QDir::homePath());
            mkdir.mkdir(".QFSViewer");
        }
    }

    QFSViewerConfigFile = new ConfigFile(QDir::homePath()+"/.QFSViewer/QFSViewer.xml");
    if(QFSViewerConfigFile->config_dict.lastPath.isEmpty()) {
        ui->lineEdit->setText(QDir::homePath());
    } else {
        ui->lineEdit->setText(QFSViewerConfigFile->config_dict.lastPath);
    }
    if(QFSViewerConfigFile->config_dict.offset.isEmpty()) {
        ui->lineEdit_offset->setText("");
    } else {
        ui->lineEdit_offset->setText(QFSViewerConfigFile->config_dict.offset);
    }
    if(QFSViewerConfigFile->config_dict.size.isEmpty()) {
        ui->lineEdit_size->setText("");
    } else {
        ui->lineEdit_size->setText(QFSViewerConfigFile->config_dict.size);
    }

    fsView = new FSViewWindow(QFSViewerConfigFile, this);

    foreach (QString key, fsTypeMap.keys()) {
        if(QFSViewerConfigFile->config_dict.fsType == key) {
            fsTypeMap[key]->setChecked(true);
        } else {
            fsTypeMap[key]->setChecked(false);
        }
    }
}

MainWindow::~MainWindow()
{
    delete QFSViewerConfigFile;
    delete fsView;
    delete ui;
}

void MainWindow::do_list_fs(const QString &imgFile, uint64_t offset, uint64_t size)
{
    QString imgType;
    foreach (QString key, fsTypeMap.keys()) {
        if(fsTypeMap[key]->isChecked()) {
            imgType = key;
            break;
        }
    }

    QFSViewerConfigFile->config_dict.lastPath = imgFile;
    QFSViewerConfigFile->config_dict.fsType = imgType;
    QFSViewerConfigFile->config_dict.offset = ui->lineEdit_offset->text();
    QFSViewerConfigFile->config_dict.size = ui->lineEdit_size->text();

    this->hide();
    fsView->show();
    int ret = -1;
    if(imgType == "jffs2") {
        ret = fsView->setJffs2FSImgView(imgFile,offset,size);
    } else if((imgType == "fatX") || (imgType == "exfat")) {
        ret = fsView->setFatFSImgView(imgFile,offset,size);
    } else if((imgType == "ext4") || (imgType == "ext3") || (imgType == "ext2")) {
        ret = fsView->setExt4FSImgView(imgFile,offset,size);
    }
    if(ret != 0) {
        QMessageBox::warning(this, tr("Error"), tr("Load file system failed!"));
        fsView->hide();
        this->show();
    }
}

void MainWindow::on_pushButton_clicked()
{
    QString originFile = ui->lineEdit->text();
    QString imgFile = FileDialog::getOpenFileName(this, "Select image file", originFile.isEmpty()?QDir::homePath():originFile, "Image Files (*.data *.raw *.img *.bin *.img.gz *.bin.gz *.ext4 *.ext3 *.ext2 *.jffs2 *.fat* *.exfat);;All Files (*)" );
    if(imgFile.isEmpty()) {
        return;
    }
    QFileInfo info(imgFile);
    if(!info.exists() || !info.isFile()) {
        QMessageBox::warning(this, "Error", "File not exist!");
        return;
    }

    QString offset = ui->lineEdit_offset->text();
    uint64_t offsetNum = 0;
    if(!offset.isEmpty()) {
        bool isNum = false;
        offsetNum = offset.toULongLong(&isNum);
        if(!isNum) {
            QMessageBox::warning(this, "Error", "Offset must be a number!");
            return;
        }
    }

    QString size = ui->lineEdit_size->text();
    uint64_t sizeNum = info.size() - offsetNum;
    if(!size.isEmpty()) {
        bool isNum = false;
        sizeNum = size.toULongLong(&isNum);
        if(!isNum) {
            QMessageBox::warning(this, "Error", "Size must be a number!");
            return;
        }
    }
    sizeNum = qMin(sizeNum, info.size() - offsetNum);
    if(sizeNum == 0) {
        QMessageBox::warning(this, "Error", "Size must be greater than 0!");
        return;
    }
    if(!size.isEmpty()) {
        ui->lineEdit_size->setText(QString::number(sizeNum));
    }

    ui->lineEdit->setText(imgFile);

    do_list_fs(imgFile, offsetNum, sizeNum);
}


void MainWindow::on_buttonBox_accepted()
{
    QString imgFile = ui->lineEdit->text();
    QFileInfo info(imgFile);
    if(!info.exists() || !info.isFile()) {
        QMessageBox::warning(this, "Error", "File not exist!");
        return;
    }

    QString offset = ui->lineEdit_offset->text();
    uint64_t offsetNum = 0;
    if(!offset.isEmpty()) {
        bool isNum = false;
        offsetNum = offset.toULongLong(&isNum);
        if(!isNum) {
            QMessageBox::warning(this, "Error", "Offset must be a number!");
            return;
        }
    }

    QString size = ui->lineEdit_size->text();
    uint64_t sizeNum = info.size() - offsetNum;
    if(!size.isEmpty()) {
        bool isNum = false;
        sizeNum = size.toULongLong(&isNum);
        if(!isNum) {
            QMessageBox::warning(this, "Error", "Size must be a number!");
            return;
        }
    }
    sizeNum = qMin(sizeNum, info.size() - offsetNum);
    if(sizeNum == 0) {
        QMessageBox::warning(this, "Error", "Size must be greater than 0!");
        return;
    }
    if(!size.isEmpty()) {
        ui->lineEdit_size->setText(QString::number(sizeNum));
    }

    do_list_fs(imgFile, offsetNum, sizeNum);
}


void MainWindow::on_buttonBox_rejected()
{
    qApp->exit();
}

void MainWindow::on_actionHelp_triggered()
{
    QMessageBox::question(this, "Help", tr(
        "1.Select the path where the file system raw image file to be opened is located.\n"
        "2.Click the confirm button to complete the loading and display the file system contents.\n"
        "3.Right-click on the file to export the file.\n"
        "4.Right-click the file/directory, we can import files, create a new directory, delete a directory (these functions are experimental, because the original image file may be destroyed, please make sure to back it up before use).\n"),
         QMessageBox::StandardButtons(QMessageBox::Ok));
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, tr("About"),
        tr(
            "<p>Version</p>"
            "<p>&nbsp;%1</p>"
            "<p>Commit</p>"
            "<p>&nbsp;%2</p>"
            "<p>Author</p>"
            "<p>&nbsp;qiaoqm@aliyun.com</p>"
            "<p>Website</p>"
            "<p>&nbsp;<a href='https://github.com/QQxiaoming/QFSViewer'>https://github.com/QQxiaoming</p>"
            "<p>&nbsp;<a href='https://gitee.com/QQxiaoming/QFSViewer'>https://gitee.com/QQxiaoming</a></p>"
        ).arg(VERSION,GIT_TAG)
    );
}

void MainWindow::on_actionAboutQt_triggered()
{
    QMessageBox::aboutQt(this);
}

static void setAppLangeuage(QLocale::Language lang)
{
    static QTranslator *qtTranslator = nullptr;
    static QTranslator *qtbaseTranslator = nullptr;
    static QTranslator *appTranslator = nullptr;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QString qlibpath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    QString qlibpath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
    if(qtTranslator == nullptr) {
        qtTranslator = new QTranslator(qApp);
    } else {
        qApp->removeTranslator(qtTranslator);
        delete qtTranslator;
        qtTranslator = new QTranslator(qApp);
    }
    if(qtbaseTranslator == nullptr) {
        qtbaseTranslator = new QTranslator(qApp);
    } else {
        qApp->removeTranslator(qtbaseTranslator);
        delete qtbaseTranslator;
        qtbaseTranslator = new QTranslator(qApp);
    }
    if(appTranslator == nullptr) {
        appTranslator = new QTranslator(qApp);
    } else {
        qApp->removeTranslator(appTranslator);
        delete appTranslator;
        appTranslator = new QTranslator(qApp);
    }
    switch(lang) {
    case QLocale::Chinese:
        if(qtTranslator->load("qt_zh_CN.qm",qlibpath))
            qApp->installTranslator(qtTranslator);
        if(qtbaseTranslator->load("qtbase_zh_CN.qm",qlibpath))
            qApp->installTranslator(qtbaseTranslator);
        if(appTranslator->load(":/lang/lang/qfsviewer_zh_CN.qm"))
            qApp->installTranslator(appTranslator);
        break;
    case QLocale::Japanese:
        if(qtTranslator->load("qt_ja.qm",qlibpath))
            qApp->installTranslator(qtTranslator);
        if(qtbaseTranslator->load("qtbase_ja.qm",qlibpath))
            qApp->installTranslator(qtbaseTranslator);
        if(appTranslator->load(":/lang/lang/qfsviewer_ja_JP.qm"))
            qApp->installTranslator(appTranslator);
        break;
    default:
    case QLocale::English:
        if(qtTranslator->load("qt_en.qm",qlibpath))
            qApp->installTranslator(qtTranslator);
        if(qtbaseTranslator->load("qtbase_en.qm",qlibpath))
            qApp->installTranslator(qtbaseTranslator);
        if(appTranslator->load(":/lang/lang/qfsviewer_en_US.qm"))
            qApp->installTranslator(appTranslator);
        break;
    }
}

void MainWindow::on_actionEnglish_triggered()
{
    setAppLangeuage(QLocale::English);
    ui->actionChinese->setChecked(false);
    ui->actionEnglish->setChecked(true);
    ui->actionJapanese->setChecked(false);
    ui->retranslateUi(this);
}

void MainWindow::on_actionChinese_triggered()
{
    setAppLangeuage(QLocale::Chinese);
    ui->actionChinese->setChecked(true);
    ui->actionEnglish->setChecked(false);
    ui->actionJapanese->setChecked(false);
    ui->retranslateUi(this);
}

void MainWindow::on_actionJapanese_triggered()
{
    setAppLangeuage(QLocale::Japanese);
    ui->actionChinese->setChecked(false);
    ui->actionEnglish->setChecked(false);
    ui->actionJapanese->setChecked(true);
    ui->retranslateUi(this);
}

void MainWindow::on_actionDark_triggered()
{
    QFile ftheme(":/qdarkstyle/dark/darkstyle.qss");
    if (!ftheme.exists())   {
        qDebug() << "Unable to set stylesheet, file not found!";
    } else {
        ftheme.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&ftheme);
        qApp->setStyleSheet(ts.readAll());
    }

    QFontIcon::addFont(":/img/fontawesome-webfont.ttf");
    QFontIcon::instance()->setColor(Qt::white);
    ui->actionDark->setChecked(true);
    ui->actionLight->setChecked(false);
}

void MainWindow::on_actionLight_triggered()
{
    QFile ftheme(":/qdarkstyle/light/lightstyle.qss");
    if (!ftheme.exists())   {
        qDebug() << "Unable to set stylesheet, file not found!";
    } else {
        ftheme.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&ftheme);
        qApp->setStyleSheet(ts.readAll());
    }

    QFontIcon::addFont(":/img/fontawesome-webfont.ttf");
    QFontIcon::instance()->setColor(Qt::black);
    ui->actionDark->setChecked(false);
    ui->actionLight->setChecked(true);
}

int main(int argc, char *argv[])
{
    if(argc == 2) {
        if((!strncmp(argv[1],"--version",9)) || (!strncmp(argv[1],"-v",2)) ) {
            std::cout << "QFSViewer " << VERSION.toStdString() << "\n" << GIT_TAG.toStdString() << "\n";
            return 0;
        }
    }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    
    QApplication application(argc, argv);

    QApplication::setApplicationName("QFSViewer");
    QApplication::setOrganizationName("Copyright (c) 2023 Quard(QiaoQiming)");
    QApplication::setOrganizationDomain("https://github.com/QQxiaoming/QFSViewer");
    QApplication::setApplicationVersion(VERSION+" "+GIT_TAG);

    QLocale locale;
    QLocale::Language lang = locale.language();
    setAppLangeuage(lang);

    int text_hsv_value = QPalette().color(QPalette::WindowText).value();
    int bg_hsv_value = QPalette().color(QPalette::Window).value();
    bool isDarkTheme = text_hsv_value > bg_hsv_value?true:false;
    QString themeName;
    if(isDarkTheme) {
        themeName = ":/qdarkstyle/dark/darkstyle.qss";
    } else {
        themeName = ":/qdarkstyle/light/lightstyle.qss";
    }
    QFile ftheme(themeName);
    if (!ftheme.exists())   {
        qDebug() << "Unable to set stylesheet, file not found!";
    } else {
        ftheme.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&ftheme);
        qApp->setStyleSheet(ts.readAll());
    }
    QFontIcon::addFont(":/img/fontawesome-webfont.ttf");
    QFontIcon::instance()->setColor(isDarkTheme?Qt::white:Qt::black);

    MainWindow window;
    window.show();

    return application.exec();
}

