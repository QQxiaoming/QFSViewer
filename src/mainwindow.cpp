#include <QFileInfo>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

#include "mainwindow.h"
#include "ui_mainwindow.h"

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
    QMessageBox::question(this, "Help", 
        "1.主界面选择数据参数。\n"
        "2.点击打开文件或文件夹将进行固件映像数据解析并显示解析结果。\n",
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

