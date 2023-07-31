#include <QFileInfo>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("FSView");

    fsView = new FSViewWindow(this);
    ui->radioButton_jffs2->setChecked(true);
    ui->radioButton_ext4->setChecked(false);
    ui->radioButton_vfat->setChecked(false);
    ui->radioButton_exfat->setChecked(false);
    ui->lineEdit->setText("/home/qqm/Downloads/work/qfsviewer/test");
}

MainWindow::~MainWindow()
{
    delete fsView;
    delete ui;
}

void MainWindow::do_list_fs(const QString &imgFile)
{
    QMap<QString, QRadioButton *> fsTypeMap = {
        {"jffs2", ui->radioButton_jffs2},
        {"vfat", ui->radioButton_vfat},
        {"exfat", ui->radioButton_exfat},
        {"ext4", ui->radioButton_ext4},
        {"ext3", ui->radioButton_ext3},
        {"ext2", ui->radioButton_ext2},
    };
    QString imgType;
    foreach (QString key, fsTypeMap.keys()) {
        if(fsTypeMap[key]->isChecked()) {
            imgType = key;
            break;
        }
    }

    QFileInfo info(imgFile);
    this->hide();
    fsView->show();
    if(imgType == "jffs2") {
        fsView->setJffs2FSImgView(imgFile,0,info.size());
    } else if((imgType == "vfat") || (imgType == "exfat")) {
        fsView->setFatFSImgView(imgFile,0,info.size());
    } else if((imgType == "ext4") || (imgType == "ext3") || (imgType == "ext2")) {
        fsView->setExt4FSImgView(imgFile,0,info.size());
    }
}

void MainWindow::on_pushButton_clicked()
{
    QString originFile = ui->lineEdit->text();
    QString imgFile = QFileDialog::getOpenFileName(this, "Select image file", originFile.isEmpty()?QDir::homePath():originFile, "Image Files (*.data *.raw *.img *.bin *.img.gz *.bin.gz *.ext4 *.ext3 *.ext2 *.jffs2 *.vfat *.exfat);;All Files (*)" );
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

