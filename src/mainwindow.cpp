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
    ui->radioButton->setChecked(true);
    ui->radioButton_2->setChecked(false);
    ui->radioButton_3->setChecked(false);
    ui->lineEdit->setText(QDir::homePath());
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
    QString imgFile = QFileDialog::getOpenFileName(this, "Select image file", originFile.isEmpty()?QDir::homePath():originFile , "image files(*.data *.raw *.img *.bin *.img.gz *.bin.gz *.jffs2 *.ext4 *.fatfs)");
    if(imgFile.isEmpty()) {
        return;
    }
    QFileInfo info(imgFile);
    if(!info.exists() || !info.isFile()) {
        QMessageBox::warning(this, "Error", "File not exist!");
        return;
    }
    ui->lineEdit->setText(imgFile);

    if(ui->radioButton->isChecked()) {
        imgType = "jffs2";
    } else if(ui->radioButton_2->isChecked()) {
        imgType = "ext4";
    } else if(ui->radioButton_3->isChecked()) {
        imgType = "fatfs";
    }

    this->hide();
    fsView->show();
    if(imgType == "jffs2") {
        fsView->setJffs2FSImgView(imgFile,0,info.size());
    } else if(imgFile == "ext4") {
        fsView->setExt4FSImgView(imgFile,0,info.size());
    } else if(imgFile == "fatfs" ) {
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

    if(ui->radioButton->isChecked()) {
        imgType = "jffs2";
    } else if(ui->radioButton_2->isChecked()) {
        imgType = "ext4";
    } else if(ui->radioButton_3->isChecked()) {
        imgType = "fatfs";
    }

    this->hide();
    fsView->show();
    if(imgType == "jffs2") {
        fsView->setJffs2FSImgView(imgFile,0,info.size());
    } else if(imgFile == "ext4") {
        fsView->setExt4FSImgView(imgFile,0,info.size());
    } else if(imgFile == "fatfs" ) {
        fsView->setFatFSImgView(imgFile,0,info.size());
    }
}


void MainWindow::on_buttonBox_rejected()
{
    qApp->exit();
}

