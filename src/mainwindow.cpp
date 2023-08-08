#include <QFileInfo>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QString>
#include <QTranslator>
#include <iostream>

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

void MainWindow::do_list_fs(const QString &imgFile)
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

    QFileInfo info(imgFile);
    this->hide();
    fsView->show();
    if(imgType == "jffs2") {
        fsView->setJffs2FSImgView(imgFile,0,info.size());
    } else if((imgType == "fatX") || (imgType == "exfat")) {
        fsView->setFatFSImgView(imgFile,0,info.size());
    } else if((imgType == "ext4") || (imgType == "ext3") || (imgType == "ext2")) {
        fsView->setExt4FSImgView(imgFile,0,info.size());
    }
}

void MainWindow::on_pushButton_clicked()
{
    QString originFile = ui->lineEdit->text();
    QString imgFile = QFileDialog::getOpenFileName(this, "Select image file", originFile.isEmpty()?QDir::homePath():originFile, "Image Files (*.data *.raw *.img *.bin *.img.gz *.bin.gz *.ext4 *.ext3 *.ext2 *.jffs2 *.fat* *.exfat);;All Files (*)" );
    if(imgFile.isEmpty()) {
        return;
    }
    QFileInfo info(imgFile);
    if(!info.exists() || !info.isFile()) {
        QMessageBox::warning(this, "Error", "File not exist!");
        return;
    }
    ui->lineEdit->setText(imgFile);

    do_list_fs(imgFile);
}


void MainWindow::on_buttonBox_accepted()
{
    QString imgFile = ui->lineEdit->text();
    QFileInfo info(imgFile);
    if(!info.exists() || !info.isFile()) {
        QMessageBox::warning(this, "Error", "File not exist!");
        return;
    }
    do_list_fs(imgFile);
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

static QTranslator qtTranslator;
static QTranslator qtbaseTranslator;
static QTranslator appTranslator;

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
    QApplication application(argc, argv);

    QApplication::setApplicationName("QFSViewer");
    QApplication::setOrganizationName("Copyright (c) 2023 Quard(QiaoQiming)");
    QApplication::setOrganizationDomain("https://github.com/QQxiaoming/QFSViewer");
    QApplication::setApplicationVersion(VERSION+" "+GIT_TAG);

    QLocale locale;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QString qlibpath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    QString qlibpath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
    QLocale::Language lang = locale.language();
    switch(lang) {
    case QLocale::Chinese:
        if(qtTranslator.load("qt_zh_CN.qm",qlibpath))
            application.installTranslator(&qtTranslator);
        if(qtbaseTranslator.load("qtbase_zh_CN.qm",qlibpath))
            application.installTranslator(&qtbaseTranslator);
        if(appTranslator.load(":/lang/lang/qfsviewer_zh_CN.qm"))
            application.installTranslator(&appTranslator);
        break;
    case QLocale::Japanese:
        if(qtTranslator.load("qt_ja.qm",qlibpath))
            application.installTranslator(&qtTranslator);
        if(qtbaseTranslator.load("qtbase_ja.qm",qlibpath))
            application.installTranslator(&qtbaseTranslator);
        if(appTranslator.load(":/lang/lang/qfsviewer_ja_JP.qm"))
            application.installTranslator(&appTranslator);
        break;
    default:
    case QLocale::English:
        if(qtTranslator.load("qt_en.qm",qlibpath))
            application.installTranslator(&qtTranslator);
        if(qtbaseTranslator.load("qtbase_en.qm",qlibpath))
            application.installTranslator(&qtbaseTranslator);
        if(appTranslator.load(":/lang/lang/qfsviewer_en_US.qm"))
            application.installTranslator(&appTranslator);
        break;
    }

    int text_hsv_value = QPalette().color(QPalette::WindowText).value();
    int bg_hsv_value = QPalette().color(QPalette::Window).value();
    bool isDarkTheme = text_hsv_value > bg_hsv_value?true:false;

    QFontIcon::addFont(":/img/fontawesome-webfont.ttf");
    QFontIcon::instance()->setColor(isDarkTheme?Qt::white:Qt::black);

    MainWindow window;
    window.show();

    return application.exec();
}
