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

void MainWindow::on_pushButton_clicked()
{
    QString imgType = "jffs2";
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

    if(ui->radioButton_jffs2->isChecked()) {
        imgType = "jffs2";
    } else if(ui->radioButton_ext4->isChecked()) {
        imgType = "ext4";
    } else if(ui->radioButton_vfat->isChecked()) {
        imgType = "vfat";
    } else if(ui->radioButton_exfat->isChecked()) {
        imgType = "exfat";
    }

    this->hide();
    fsView->show();
    if(imgType == "jffs2") {
        fsView->setJffs2FSImgView(imgFile,0,info.size());
    } else if(imgType == "ext4") {
        fsView->setExt4FSImgView(imgFile,0,info.size());
    } else if(imgType == "vfat" ) {
        fsView->setFatFSImgView(imgFile,0,info.size());
    } else if(imgType == "exfat" ) {
        fsView->setFatFSImgView(imgFile,0,info.size());
    }
}


void MainWindow::on_buttonBox_accepted()
{
    QString imgType = "jffs2";
    QString imgFile = ui->lineEdit->text();
    QFileInfo info(imgFile);
    if(!info.exists() || !info.isFile()) {
        QMessageBox::warning(this, "Error", "File not exist!");
        return;
    }

    if(ui->radioButton_jffs2->isChecked()) {
        imgType = "jffs2";
    } else if(ui->radioButton_ext4->isChecked()) {
        imgType = "ext4";
    } else if(ui->radioButton_vfat->isChecked()) {
        imgType = "vfat";
    } else if(ui->radioButton_exfat->isChecked()) {
        imgType = "exfat";
    }

    this->hide();
    fsView->show();
    if(imgType == "jffs2") {
        fsView->setJffs2FSImgView(imgFile,0,info.size());
    } else if(imgType == "ext4") {
        fsView->setExt4FSImgView(imgFile,0,info.size());
    } else if(imgType == "vfat" ) {
        fsView->setFatFSImgView(imgFile,0,info.size());
    } else if(imgType == "exfat" ) {
        fsView->setFatFSImgView(imgFile,0,info.size());
    }
}


void MainWindow::on_buttonBox_rejected()
{
    qApp->exit();
}

