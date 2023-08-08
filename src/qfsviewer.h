#ifndef QFSVIEWER_H
#define QFSVIEWER_H

#include <QTreeView>
#include <QFile>
#include <QCloseEvent>
#include <QApplication>
#include <QDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDateTime>
#include <QDate>
#include <QTime>
#include <QMenu>
#include <QFileDialog>

#include "fsviewmodel.h"
#include "treemodel.h"
#include "qfonticon.h"
#include "configFile.h"

const static QString VERSION = APP_VERSION;
const static QString GIT_TAG =
#include "git_tag.inc"
;

class FSViewWindow : public QTreeView
{
    Q_OBJECT
public:
 explicit FSViewWindow(ConfigFile *configFile = nullptr, QWidget *parent = nullptr) :
        QTreeView(nullptr) {
        mode = new TreeModel(this);
        m_configFile = configFile;
        setModel(mode);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
        resetView();
        setWindowTitle(tr("FSView"));
        setAnimated(true);
        setColumnWidth(0,400);
        setColumnWidth(1,80);
        setColumnWidth(2,80);
        setColumnWidth(3,150);
        resize(QSize(800,600));
        QRect screen = QGuiApplication::screenAt(this->mapToGlobal(QPoint(this->width()/2,0)))->geometry();
        QRect size = this->geometry();
        this->move((screen.width() - size.width()) / 2, (screen.height() - size.height()) / 2);
        m_parent = parent;
    }

    ~FSViewWindow() {
        resetView();
        delete mode;
    }

    enum {
        LIBPARTMBR_STATUS_NON_BOOTABLE     = 0x00,
        LIBPARTMBR_STATUS_NON_BOOTABLE_LBA = 0x01,
        LIBPARTMBR_STATUS_BOOTABLE         = 0x80,
        LIBPARTMBR_STATUS_BOOTABLE_LBA     = 0x81,
    };

    enum {
        LIBPARTMBR_TYPE_EMPTY             = 0x00,	    // empty
        LIBPARTMBR_TYPE_FAT12_32MB        = 0x01,	    // FAT12 within the first 32MB, or anywhere in logical drivve
        LIBPARTMBR_TYPE_XENIX_ROOT        = 0x02,	    // XENIX root
        LIBPARTMBR_TYPE_XENIX_USR         = 0x03,	    // XENIX usr
        LIBPARTMBR_TYPE_FAT16_32MB        = 0x04,	    // FAT16 within the first 32MB, less than 65536 sectors, or anywhere in logical drive
        LIBPARTMBR_TYPE_EXTENDED_CHS      = 0x05,	    // extended partition (CHS mapping)
        LIBPARTMBR_TYPE_FAT16B_8GB        = 0x06,	    // FAT16B (CHS) within the first 8GB, 65536 or more sectors, or FAT12/FAT16 outside first 32MB, or in type 0x05 extended part
        LIBPARTMBR_TYPE_NTFS_HPFS         = 0x07,	    // OS/2 IFS/HPFS, Windows NT NTFS, Windows CE exFAT
        LIBPARTMBR_TYPE_LOGSECT_FAT16     = 0x08,	    // Logically sectored FAT12/FAT16 (larger sectors to overcome limits)

        LIBPARTMBR_TYPE_FAT32_CHS         = 0x0B,	    // FAT32 (CHS)
        LIBPARTMBR_TYPE_FAT32_LBA         = 0x0C,	    // FAT32 (LBA)

        LIBPARTMBR_TYPE_FAT16B_LBA        = 0x0E,		// FAT16B (LBA)
        LIBPARTMBR_TYPE_EXTENDED_LBA      = 0x0F,		// extended partition (LBA mapping)

        LIBPARTMBR_TYPE_FAT12_32MB_HIDDEN = 0x11,	    // hidden version of type 0x01

        LIBPARTMBR_TYPE_FAT16_32MB_HIDDEN = 0x14,		// hidden version of type 0x04

        LIBPARTMBR_TYPE_FAT16B_8GB_HIDDEN = 0x16,	    // hidden version of type 0x06
        LIBPARTMBR_TYPE_NTFS_HPFS_HIDDEN  = 0x17,		// hidden version of type 0x07

        LIBPARTMBR_TYPE_FAT32_CHS_HIDDEN  = 0x1B,		// hidden version of type 0x0B
        LIBPARTMBR_TYPE_FAT32_LBA_HIDDEN  = 0x1C,		// hidden version of type 0x0C

        LIBPARTMBR_TYPE_FAT16B_LBA_HIDDEN = 0x1E,		// hidden version of type 0x0E

        LIBPARTMBR_TYPE_LINUX_SWAP        = 0x82,		// Linux swap
        LIBPARTMBR_TYPE_LINUX_NATIVE      = 0x83,		// Linux native partition

        LIBPARTMBR_TYPE_GPT               = 0xEE,	    // GPT protective partition

        LIBPARTMBR_TYPE_LINUX_RAID        = 0xFD,		// Linux RAID partition
    };

    const static uint16_t MBR_COPY_PROTECTED = 0x5A5A;
    const static uint16_t MBR_BOOT_SIGNATURE = 0xAA55;

    #pragma pack(1)
    typedef struct {
        uint8_t   status;
        struct {
            uint8_t   h;
            uint16_t  cs;
        } start_chs;
        uint8_t   type;
        struct {
            uint8_t   h;
            uint16_t  cs;
        } end_chs;
        uint32_t  starting_lba;
        uint32_t  number_of_sectors;
    } partition_t;
    typedef struct {
        uint8_t              bootstrap_code[440];
        uint32_t             disk_signiture;
        uint16_t             copy_protected;
        partition_t          partition_table[4];
        uint16_t             boot_signature;
    } mbr_t;
    #pragma pack()

    static mbr_t get_mbr(QString rootFSImgPath) {
        mbr_t mbr;
        QFile fs_img(rootFSImgPath);
        fs_img.open(QIODevice::ReadOnly);
        fs_img.seek(0);
        fs_img.read((char*)&mbr,sizeof(mbr));
        fs_img.close();
        return mbr;
    }

    void setExt4FSImgView(QString rootFSImgPath,uint64_t offset, uint64_t size) {
        setFSImgView(rootFSImgPath,offset,size,"Ext4");
    }

    void setFatFSImgView(QString rootFSImgPath,uint64_t offset, uint64_t size) {
        setFSImgView(rootFSImgPath,offset,size,"FatFS");
    }

    void setJffs2FSImgView(QString rootFSImgPath,uint64_t offset, uint64_t size) {
        setFSImgView(rootFSImgPath,offset,size,"Jffs2");
    }

private:
    void resetView(void) {
        m_idle = true;
        mode->removeTree(rootIndex);
        mode->set_root_timestamp(0);
        rootIndex = mode->addTree("/", 0, 0, 0, QModelIndex());
        expand(rootIndex);
    }

    void expand_recursive(QString path) {
        QStringList list = path.split("/");
        qDebug() << list;
        QModelIndex pIndexes = rootIndex;
        foreach (QString item, list) {
            if(item == "")  {
                expand(pIndexes);
                continue;
            }
            QModelIndex cIndexes = mode->findItems(item,pIndexes);
            if(cIndexes.isValid()) {
                expand(cIndexes);
                pIndexes = cIndexes;
            } else {
                break;
            }
        }
    }

    void setFSImgView(QString rootFSImgPath,uint64_t offset, uint64_t size,QString type) {
        resetView();
        m_idle = false;
        setWindowTitle(rootFSImgPath);
        if(fsView) delete fsView;
        if(type == "Ext4") {
            fsView = new Ext4FSViewModel(mode,rootFSImgPath,offset,size,this);
        } else if(type == "FatFS") {
            fsView = new FatFSFSViewModel(mode,rootFSImgPath,offset,size,this);
        } else if(type == "Jffs2") {
            fsView = new Jffs2FSViewModel(mode,rootFSImgPath,offset,size,this);
        }
        fsView->setFSImgView(rootIndex);
        m_idle = true;
    }

protected:
    void contextMenuEvent(QContextMenuEvent *event) override {
        QModelIndex tIndex = indexAt(viewport()->mapFromGlobal(event->globalPos()));
        if (tIndex.isValid() && m_idle) {
            int type = FSViewModel::FSView_UNKNOWN;
            uint64_t size = 0;
            QString name;
            mode->info(tIndex, type, name, size);
            if((type == FSViewModel::FSView_REG_FILE) || (type == FSViewModel::FSView_DIR)) {
                //TODO: why this way crash?
                //QMenu *contextMenu = new QMenu(this); 
                //contextMenu->setAttribute(Qt::WA_DeleteOnClose); 
                // Now we renew contextMenu, because use Qt::WA_DeleteOnClose can't work
                if(contextMenu) delete contextMenu;
                contextMenu = new QMenu(this); 

                QAction *pExport= new QAction(tr("Export"), this);
                pExport->setIcon(QIcon(QFontIcon::icon(QChar(0xf019))));
                contextMenu->addAction(pExport);
                connect(pExport,&QAction::triggered,this,
                    [&,tIndex](void)
                    {
                        QString name;
                        uint64_t size = 0;
                        int type = FSViewModel::FSView_UNKNOWN;
                        mode->info(tIndex, type, name, size);
                        QString path = name;
                        std::function<QModelIndex(QModelIndex,QString &)> get_parent = [&](QModelIndex index, QString &name) -> QModelIndex {
                            if(index.isValid() && index.parent().isValid()) {
                                QString pname;
                                uint64_t size = 0;
                                int type = FSViewModel::FSView_UNKNOWN;
                                QModelIndex parent = index.parent();
                                mode->info(parent, type, pname, size);
                                name = (pname == "/")?(pname + name):(pname + "/" + name);
                                return get_parent(parent, name);
                            } else {
                                return QModelIndex();
                            }
                        };
                        get_parent(tIndex, path);
                        if(type == FSViewModel::FSView_DIR) {
                            QMessageBox::critical(this, tr("Error"), tr("Exporting dirs is not currently supported!"));
                            return;
                        } else if(type == FSViewModel::FSView_REG_FILE) {
                            QString filename = QFileDialog::getSaveFileName(this, tr("Save File"),
                                    (m_configFile?m_configFile->config_dict.lastFilePath:QDir::homePath())+"/"+name);
                            if (filename.isEmpty())
                                return;
                            QFileInfo info(this->windowTitle());
                            int ret = -1;
                            if(fsView) ret = fsView->exportFSImg(path, filename);
                            if(ret == 0) {
                                QFileInfo savePath(filename);
                                if(m_configFile) m_configFile->config_dict.lastFilePath = savePath.absolutePath();
                                QMessageBox::information(this, tr("Information"), tr("Export file success!"));
                            } else {
                                QMessageBox::critical(this, tr("Error"), tr("Can't export file!"));
                            }
                        } else {
                            QMessageBox::critical(this, tr("Error"), tr("Can't export file!"));
                            return;
                        }
                    }
                );

                QAction *pImport= new QAction(tr("Import"), this);
                pImport->setIcon(QIcon(QFontIcon::icon(QChar(0xf093))));
                contextMenu->addAction(pImport);
                connect(pImport,&QAction::triggered,this,
                    [&,tIndex](void)
                    {
                        int wret = QMessageBox::warning(this,"Warning","In principle, this software does not provide the function of modifying the disk image. If you use this function, please remember to back up your files, and this software does not guarantee the strict correctness of the import.\nPlease choose whether to continue.", QMessageBox::Yes, QMessageBox::No);
                        if(wret == QMessageBox::No) {
                            return;
                        }

                        QString name;
                        uint64_t size = 0;
                        int type = FSViewModel::FSView_UNKNOWN;
                        mode->info(tIndex, type, name, size);
                        QString path = name;
                        std::function<QModelIndex(QModelIndex,QString &)> get_parent = [&](QModelIndex index, QString &name) -> QModelIndex {
                            if(index.isValid() && index.parent().isValid()) {
                                QString pname;
                                uint64_t size = 0;
                                int type = FSViewModel::FSView_UNKNOWN;
                                QModelIndex parent = index.parent();
                                mode->info(parent, type, pname, size);
                                name = (pname == "/")?(pname + name):(pname + "/" + name);
                                return get_parent(parent, name);
                            } else {
                                return QModelIndex();
                            }
                        };
                        get_parent(tIndex, path);
                        if(type == FSViewModel::FSView_REG_FILE) {
                            QFileInfo input_info(path);
                            path = input_info.absolutePath();
                        #if defined(Q_OS_WIN)
                            path.replace("C:/","");
                        #endif
                        }
                        QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), 
                            m_configFile? m_configFile->config_dict.lastFilePath:QDir::homePath());
                        if (filePath.isEmpty())
                            return;
                        QFileInfo input(filePath);
                        if(!input.isFile()) {
                            QMessageBox::critical(this, tr("Error"), tr("Can't import file!"));
                            return;
                        }
                        path = (path=="/")?(path+input.fileName()):(path+"/"+input.fileName());
                        QFileInfo info(this->windowTitle());
                        int ret = -1;
                        if(fsView) {
                            ret = fsView->importFSImg(path, filePath);
                            if(ret == 0) {
                                resetView();
                                m_idle = false;
                                fsView->setFSImgView(rootIndex);
                                expand_recursive(path);
                                m_idle = true;
                            }
                        }
                        if(ret == 0) {
                            QFileInfo savePath(filePath);
                            if(m_configFile) m_configFile->config_dict.lastFilePath = savePath.absolutePath();
                            QMessageBox::information(this, tr("Information"), tr("Import file success!"));
                        } else {
                            QMessageBox::critical(this, tr("Error"), tr("Unsupported operation!"));
                        }
                    }
                );

                QAction *pCreate= new QAction(tr("Create"), this);
                pCreate->setIcon(QIcon(QFontIcon::icon(QChar(0xf0f6))));
                contextMenu->addAction(pCreate);
                connect(pCreate,&QAction::triggered,this,
                [&,tIndex](void)
                    {
                        int wret = QMessageBox::warning(this,"Warning","In principle, this software does not provide the function of modifying the disk image. If you use this function, please remember to back up your files, and this software does not guarantee the strict correctness of the import.\nPlease choose whether to continue.", QMessageBox::Yes, QMessageBox::No);
                        if(wret == QMessageBox::No) {
                            return;
                        }

                        QString name;
                        uint64_t size = 0;
                        int type = FSViewModel::FSView_UNKNOWN;
                        mode->info(tIndex, type, name, size);
                        QString path = name;
                        std::function<QModelIndex(QModelIndex,QString &)> get_parent = [&](QModelIndex index, QString &name) -> QModelIndex {
                            if(index.isValid() && index.parent().isValid()) {
                                QString pname;
                                uint64_t size = 0;
                                int type = FSViewModel::FSView_UNKNOWN;
                                QModelIndex parent = index.parent();
                                mode->info(parent, type, pname, size);
                                name = (pname == "/")?(pname + name):(pname + "/" + name);
                                return get_parent(parent, name);
                            } else {
                                return QModelIndex();
                            }
                        };
                        get_parent(tIndex, path);
                        if(type == FSViewModel::FSView_REG_FILE) {
                            QFileInfo input_info(path);
                            path = input_info.absolutePath();
                        #if defined(Q_OS_WIN)
                            path.replace("C:/","");
                        #endif
                        }
                        bool isOK = false;
                        QString fileName = QInputDialog::getText(this, tr("Enter Dir Name"), tr("Name"), QLineEdit::Normal, "", &isOK);
                        if (fileName.isEmpty()) {
                            if(isOK) {
                                QMessageBox::critical(this, tr("Error"), tr("Can't create dir!"));
                            }
                            return;
                        }
                        path = (path=="/")?(path+fileName):(path+"/"+fileName);
                        QFileInfo info(this->windowTitle());
                        int ret = -1;
                        if(fsView) {
                            ret = fsView->createDirFSImg(path);
                            if(ret == 0) {
                                resetView();
                                m_idle = false;
                                fsView->setFSImgView(rootIndex);
                                expand_recursive(path);
                                m_idle = true;
                            }
                        }
                        if(ret == 0) {
                            QMessageBox::information(this, tr("Information"), tr("Create dir success!"));
                        } else {
                            QMessageBox::critical(this, tr("Error"), tr("Unsupported operation!"));
                        }
                    }
                );
                QAction *pDelete= new QAction(tr("Delete"), this);
                pDelete->setIcon(QIcon(QFontIcon::icon(QChar(0xf014))));
                contextMenu->addAction(pDelete);
                connect(pDelete,&QAction::triggered,this,
                [&,tIndex](void)
                    {
                        int wret = QMessageBox::warning(this,"Warning","In principle, this software does not provide the function of modifying the disk image. If you use this function, please remember to back up your files, and this software does not guarantee the strict correctness of the import.\nPlease choose whether to continue.", QMessageBox::Yes, QMessageBox::No);
                        if(wret == QMessageBox::No) {
                            return;
                        }

                        QString name;
                        uint64_t size = 0;
                        int type = FSViewModel::FSView_UNKNOWN;
                        mode->info(tIndex, type, name, size);
                        if(type != FSViewModel::FSView_DIR && type != FSViewModel::FSView_REG_FILE) {
                            QMessageBox::critical(this, tr("Error"), tr("Unsupported operation!"));
                            return;
                        }
                        if(type == FSViewModel::FSView_DIR && size != 0) {
                            QMessageBox::critical(this, tr("Error"), tr("Now only support delete empty dir!"));
                            return;
                        }
                        QString path = name;
                        std::function<QModelIndex(QModelIndex,QString &)> get_parent = [&](QModelIndex index, QString &name) -> QModelIndex {
                            if(index.isValid() && index.parent().isValid()) {
                                QString pname;
                                uint64_t size = 0;
                                int type = FSViewModel::FSView_UNKNOWN;
                                QModelIndex parent = index.parent();
                                mode->info(parent, type, pname, size);
                                name = (pname == "/")?(pname + name):(pname + "/" + name);
                                return get_parent(parent, name);
                            } else {
                                return QModelIndex();
                            }
                        };
                        get_parent(tIndex, path);
                        QFileInfo info(this->windowTitle());
                        int ret = -1;
                        if(fsView) {
                            if(type == FSViewModel::FSView_DIR ) {
                                ret = fsView->removeDirFSImg(path);
                            } else if(type == FSViewModel::FSView_REG_FILE) {
                                ret = fsView->removeFileFSImg(path);
                            }
                            if(ret == 0) {
                                resetView();
                                m_idle = false;
                                fsView->setFSImgView(rootIndex);
                                expand_recursive(path);
                                m_idle = true;
                            }
                        }
                        if(ret == 0) {
                            QMessageBox::information(this, tr("Information"), tr("Delete success!"));
                        } else {
                            QMessageBox::critical(this, tr("Error"), tr("Unsupported operation!"));
                        }
                    }
                );
                if(!contextMenu->isEmpty()) {
                    contextMenu->move(cursor().pos());
                    contextMenu->show();
                }
            }
        }
        event->accept();
    }

    void closeEvent(QCloseEvent *event) override {
        if(!m_idle) {
            QMessageBox::information(this, tr("Information"), tr("Loading, please wait..."));
            event->ignore();
        } else {
            m_parent->show();
            this->hide();
            event->ignore();
        }
    }

private:
    TreeModel *mode;
    FSViewModel *fsView = nullptr;
    ConfigFile *m_configFile;
    bool m_idle;
    QWidget *m_parent;
    QMenu *contextMenu = nullptr;
    QModelIndex rootIndex;
};

#endif // QFSVIEWER_H

