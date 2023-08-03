#ifndef QFSVIEWER_H
#define QFSVIEWER_H

#include <QTreeView>
#include <QFile>
#include <QCloseEvent>
#include <QApplication>
#include <QDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDateTime>
#include <QDate>
#include <QTime>
#include <QMenu>
#include <QFileDialog>

#include "jffs2extract.h"
#include "ff_port.h"
#include "lwext4_port.h"
#include "treemodel.h"

#include "qfonticon.h"

const static QString VERSION = APP_VERSION;
const static QString GIT_TAG =
#include "git_tag.inc"
;

class FSViewWindow : public QTreeView
{
    Q_OBJECT
public:
 explicit FSViewWindow(QWidget *parent = nullptr) :
        QTreeView(nullptr) {
        mode = new TreeModel(this);
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
        bool read_only = true;
        resetView();
        m_idle = false;
        m_fsType = 0;
        setWindowTitle(rootFSImgPath);
        QFileInfo fi(rootFSImgPath);
        mode->set_root_timestamp((uint32_t)fi.birthTime().toUTC().toSecsSinceEpoch());
        QFile fs_img(rootFSImgPath);
        fs_img.open(read_only?QIODevice::ReadOnly:QIODevice::ReadWrite);
        uint8_t *addr = fs_img.map(offset,size);
        if(addr[0x438] != 0x53 || addr[0x439] != 0xEF) {
            int ret = QMessageBox::warning(this,"Warning","Maybe not a ext4 filesystem. Do you want to force the execution, this may cause the program to crash.", QMessageBox::Yes, QMessageBox::No);
            if(ret == QMessageBox::No) {
                m_parent->show();
                this->hide();
                return;
            }
        }
        lwext_init(addr,size);
        struct ext4_blockdev * bd = ext4_blockdev_get();
        ext4_device_register(bd, "ext4_fs");
        ext4_mount("ext4_fs", "/", read_only);
        if(!read_only) {
            ext4_recover("/");
            ext4_journal_start("/");
            ext4_cache_write_back("/", 1);
        }
        listExt4FSAll("/",rootIndex);
        if(!read_only) {
            ext4_cache_write_back("/", 0);
            ext4_journal_stop("/");
        }
        ext4_umount("/");
        ext4_device_unregister("ext4_fs");
        fs_img.unmap(addr);
        fs_img.close();
        m_idle = true;
    }

    void setFatFSImgView(QString rootFSImgPath,uint64_t offset, uint64_t size) {
        resetView();
        m_idle = false;
        m_fsType = 1;
        setWindowTitle(rootFSImgPath);
        QFileInfo fi(rootFSImgPath);
        mode->set_root_timestamp((uint32_t)fi.birthTime().toUTC().toSecsSinceEpoch());
        QFile fs_img(rootFSImgPath);
        fs_img.open(QIODevice::ReadOnly);
        uint8_t *addr = fs_img.map(offset,size);
        if(addr[0x1FE] != 0x55 || addr[0x1FF] != 0xAA) {
            int ret = QMessageBox::warning(this,"Warning","Maybe not a fat filesystem. Do you want to force the execution, this may cause the program to crash.", QMessageBox::Yes, QMessageBox::No);
            if(ret == QMessageBox::No) {
                m_parent->show();
                this->hide();
                return;
            }
        }
        ff_init(addr,size);
        FATFS FatFs;
        f_mount(&FatFs,"",0);
        listFatFSAll("/",rootIndex);
        f_mount(NULL,"",0);
        fs_img.unmap(addr);
        fs_img.close();
        m_idle = true;
    }

    void setJffs2FSImgView(QString rootFSImgPath,uint64_t offset, uint64_t size) {
        resetView();
        m_idle = false;
        m_fsType = 2;
        setWindowTitle(rootFSImgPath);
        QFileInfo fi(rootFSImgPath);
        mode->set_root_timestamp((uint32_t)fi.birthTime().toUTC().toSecsSinceEpoch());
        QFile fs_img(rootFSImgPath);
        fs_img.open(QIODevice::ReadOnly);
        uint8_t *addr = fs_img.map(offset,size);
        if(addr[0x0] != 0x85 || addr[0x1] != 0x19) {
            int ret = QMessageBox::warning(this,"Warning","Maybe not a jffs2 filesystem. Do you want to force the execution, this may cause the program to crash.", QMessageBox::Yes, QMessageBox::No);
            if(ret == QMessageBox::No) {
                m_parent->show();
                this->hide();
                return;
            }
        }
        jffs2_init(addr,size);
        listJffs2FSAll("/",rootIndex);
        fs_img.unmap(addr);
        fs_img.close();
        m_idle = true;
    }

    void resetView(void) {
        m_idle = true;
        m_fsType = 0;
        mode->removeTree(rootIndex);
        mode->set_root_timestamp(0);
        rootIndex = mode->addTree("/", 0, 0, 0, QModelIndex());
        expand(rootIndex);
    }

    int exportExt4FSImg(QString rootFSImgPath,uint64_t offset, uint64_t size, QString input, QString output) {
        bool read_only = true;
        QFile fs_img(rootFSImgPath);
        fs_img.open(read_only?QIODevice::ReadOnly:QIODevice::ReadWrite);
        uint8_t *addr = fs_img.map(offset,size);
        lwext_init(addr,size);
        struct ext4_blockdev * bd = ext4_blockdev_get();
        ext4_device_register(bd, "ext4_fs");
        ext4_mount("ext4_fs", "/", read_only);
        if(!read_only) {
            ext4_recover("/");
            ext4_journal_start("/");
            ext4_cache_write_back("/", 1);
        }
        ext4_file f;
        ext4_fopen(&f, input.toStdString().c_str(), "rb");
        QFile w(output);
        w.open(QIODevice::WriteOnly|QIODevice::Truncate);
        uint8_t *buf = new uint8_t[4096];
        do {
            size_t byte = 0;
            ext4_fread(&f, buf, 4096, &byte);
            if(byte == 0) {
                break;
            } else {
                w.write((const char*)buf,byte);
            }
        } while(1);
        delete[] buf;
        w.close();
        ext4_fclose(&f);
        if(!read_only) {
            ext4_cache_write_back("/", 0);
            ext4_journal_stop("/");
        }
        ext4_umount("/");
        ext4_device_unregister("ext4_fs");
        fs_img.unmap(addr);
        fs_img.close();
        return 0;
    }

    int exportFatFSImg(QString rootFSImgPath,uint64_t offset, uint64_t size, QString input, QString output) {
        QFile fs_img(rootFSImgPath);
        fs_img.open(QIODevice::ReadOnly);
        uint8_t *addr = fs_img.map(offset,size);
        ff_init(addr,size);
        FATFS FatFs;
        f_mount(&FatFs,"",0);
        FIL f;
        f_open(&f, input.toStdString().c_str(), FA_READ);
        QFile w(output);
        w.open(QIODevice::WriteOnly|QIODevice::Truncate);
        uint8_t *buf = new uint8_t[4096];
        do {
            UINT byte = 0;
            f_read(&f, buf, 4096, &byte);
            if(byte == 0) {
                break;
            } else {
                w.write((const char*)buf,byte);
            }
        } while(1);
        delete[] buf;
        w.close();
        f_close(&f);
        f_mount(NULL,"",0);
        fs_img.unmap(addr);
        fs_img.close();
        return 0;
    }
    
    int exportJFFS2Img(QString rootFSImgPath,uint64_t offset, uint64_t size, QString input, QString output) {
        QFile fs_img(rootFSImgPath);
        QFileInfo input_info(input);
        QString input_path = input_info.absolutePath();
        QString input_name = input_info.fileName();
        fs_img.open(QIODevice::ReadOnly);
        uint8_t *addr = fs_img.map(offset,size);
        QFile w(output);
        w.open(QIODevice::WriteOnly|QIODevice::Truncate);
        jffs2_init(addr,size);

        struct jffs2_raw_dirent *dd;
        struct dir *d = NULL;
        uint32_t ino;
        dd = resolvepath(1, input_path.toStdString().c_str(), &ino);
        if (ino == 0 || (dd == NULL && ino == 0))
            qWarning("No such file or directory");
        else if ((dd == NULL && ino != 0) || (dd != NULL && dd->type == 4)) {
            d = collectdir( ino, d);
            struct jffs2_raw_inode *ri, *tmpi;
            while (d != NULL) {
                ri = find_raw_inode( d->ino, 0);
                if (!ri) {
                    qWarning("bug: raw_inode missing!");
                    d = d->next;
                    continue;
                }

                tmpi = ri;
                while (tmpi) {
                    tmpi = find_raw_inode(d->ino, je32_to_cpu(tmpi->version));
                }
                QString filename(QByteArray(d->name,d->nsize));
                if(d->type == 8) {
                    if(filename == input_name) {
                        while(ri) {
                            size_t sz;
                            uint8_t *buf = new uint8_t[16384];
                            putblock((char *)buf, 16384, &sz, ri);
                            w.write((const char*)buf,sz);
                            delete[] buf;
                            ri = find_raw_inode(d->ino, je32_to_cpu(ri->version));
                        }
                    }
                }
                d = d->next;
            }
            freedir(d);
        }
        w.close();
        fs_img.unmap(addr);
        fs_img.close();
        return 0;
    }

    int importExt4FSImg(QString rootFSImgPath,uint64_t offset, uint64_t size, QString output, QString input) {
        QFile fs_img(rootFSImgPath);
        fs_img.open(QIODevice::ReadWrite);
        uint8_t *addr = fs_img.map(offset,size);
        lwext_init(addr,size);
        struct ext4_blockdev * bd = ext4_blockdev_get();
        ext4_device_register(bd, "ext4_fs");
        ext4_mount("ext4_fs", "/", 0);
        ext4_recover("/");
        ext4_journal_start("/");
        ext4_cache_write_back("/", 1);
        ext4_file f;
        ext4_fopen(&f, output.toStdString().c_str(), "wb+");
        QFile r(input);
        r.open(QIODevice::ReadOnly);
        uint8_t *buf = new uint8_t[4096];
        do {
            size_t byte = 0;
            byte = r.read((char*)buf,4096);
            if(byte == 0) {
                break;
            } else {
                ext4_fwrite(&f, buf, byte, &byte);
            }
        } while(1);
        delete[] buf;
        r.close();
        ext4_fclose(&f);
        ext4_cache_write_back("/", 0);
        ext4_journal_stop("/");
        ext4_umount("/");
        ext4_device_unregister("ext4_fs");
        fs_img.unmap(addr);
        fs_img.close();
        return 0;
    }

    int importFatFSImg(QString rootFSImgPath,uint64_t offset, uint64_t size, QString output, QString input) {
        QFile fs_img(rootFSImgPath);
        fs_img.open(QIODevice::ReadWrite);
        uint8_t *addr = fs_img.map(offset,size);
        ff_init(addr,size);
        FATFS FatFs;
        f_mount(&FatFs,"",0);
        FIL f;
        f_open(&f, output.toStdString().c_str(), FA_WRITE|FA_CREATE_ALWAYS);
        QFile r(input);
        r.open(QIODevice::ReadOnly);
        uint8_t *buf = new uint8_t[4096];
        do {
            UINT byte = 0;
            byte = r.read((char*)buf,4096);
            if(byte == 0) {
                break;
            } else {
                f_write(&f, buf, byte, &byte);
            }
        } while(1);
        delete[] buf;
        r.close();
        f_close(&f);
        f_mount(NULL,"",0);
        fs_img.unmap(addr);
        fs_img.close();
        return 0;
    }
    
    int importJFFS2Img(QString rootFSImgPath,uint64_t offset, uint64_t size, QString output, QString input) {
        //TODO: implement
        Q_UNUSED(rootFSImgPath);
        Q_UNUSED(offset);
        Q_UNUSED(size);
        Q_UNUSED(output);
        Q_UNUSED(input);
        return -1;
    }

private:
    enum fs_entity_type {
        FSView_UNKNOWN = 0,
        FSView_REG_FILE,
        FSView_DIR,
        FSView_CHARDEV,
        FSView_BLOCKDEV,
        FSView_FIFO,
        FSView_SOCKET,
        FSView_SYMLINK,
        FSView_LAST
    };
    void listExt4FSAll(QString path, QModelIndex index) {
	    const ext4_direntry *de;
	    ext4_dir d;
        ext4_dir_open(&d, path.toStdString().c_str());
        de = ext4_dir_entry_next(&d);
        while (de) {
            uint32_t timestamp = 0;
            QString filename(QByteArray((const char*)de->name,de->name_length));
            if(filename == "." || filename == "..") {
                de = ext4_dir_entry_next(&d);
                continue;
            }
            QString filePath = (path!="/")?(path+"/"+filename):("/"+filename);
            ext4_ctime_get(filePath.toStdString().c_str(),&timestamp);
            switch(de->inode_type) {
                case FSView_REG_FILE:
                {
                    ext4_file fd;
	                ext4_fopen(&fd, filePath.toStdString().c_str(), "rb");
                    uint32_t size = ext4_fsize(&fd);
                    ext4_fclose(&fd);
                    mode->addTree(filename, de->inode_type, size, timestamp, index);
                    break;
                }
                case FSView_FIFO:
                case FSView_CHARDEV:
                case FSView_BLOCKDEV:
                case FSView_SYMLINK:
                case FSView_SOCKET:
                default:
                    mode->addTree(filename, de->inode_type, 0, timestamp, index);
                    break;
                case FSView_DIR:
                    QModelIndex modelIndex = mode->addTree(filename, de->inode_type, 0, timestamp, index);
                    listExt4FSAll(filePath, modelIndex);
                    break;
            }

            de = ext4_dir_entry_next(&d);
        }
        ext4_dir_close(&d);
        qApp->processEvents();
    }
    
    void listFatFSAll(QString path, QModelIndex index) {
        FRESULT res; 
        DIR dir;
        FILINFO fno;
        char *fn;

        res = f_opendir(&dir, path.toStdString().c_str());
        if (res == FR_OK) {
            while (1) {
                res = f_readdir(&dir, &fno);
                if (res != FR_OK || fno.fname[0] == 0) 
                    break;
                fn = fno.fname;
                int year = ((fno.fdate & 0b1111111000000000) >> 9) + 1980;
                int month = (fno.fdate & 0b0000000111100000) >> 5;
                int day =  fno.fdate & 0b0000000000011111;
                int hour = (fno.ftime & 0b111110000000000) >> 11;
                int minute = (fno.ftime & 0b0000011111100000) >> 5;
                int second = (fno.ftime & 0b0000000000011111) * 2;
                QDateTime dt(QDate(year, month, day), QTime(hour, minute, second));

                QString filename(QByteArray(fn,strlen(fn)));
                if (fno.fattrib & AM_DIR) { 
                    QModelIndex modelIndex = mode->addTree(filename, FSView_DIR, fno.fsize, dt.toSecsSinceEpoch(), index);
                    QString filePath = (path!="/")?(path+"/"+filename):("/"+filename);
                    listFatFSAll(filePath, modelIndex);
                } else {
                    mode->addTree(filename, FSView_REG_FILE, fno.fsize, dt.toSecsSinceEpoch(), index);
                }
            }
            f_closedir(&dir);
        }
        qApp->processEvents();
    }

    void listJffs2FSAll(QString path, QModelIndex index) {
        const static uint32_t dt2fsv[16] = {
            FSView_UNKNOWN,FSView_FIFO,FSView_CHARDEV,FSView_UNKNOWN,
            FSView_DIR,FSView_UNKNOWN,FSView_BLOCKDEV,FSView_UNKNOWN,
            FSView_REG_FILE,FSView_UNKNOWN,FSView_SYMLINK,FSView_UNKNOWN,
            FSView_SOCKET,FSView_UNKNOWN,FSView_UNKNOWN,FSView_UNKNOWN
        };
        struct jffs2_raw_dirent *dd;
        struct dir *d = NULL;

        uint32_t ino;
        dd = resolvepath(1, path.toStdString().c_str(), &ino);

        if (ino == 0 || (dd == NULL && ino == 0))
            qWarning("No such file or directory");
        else if ((dd == NULL && ino != 0) || (dd != NULL && dt2fsv[dd->type] == FSView_DIR)) {
            d = collectdir( ino, d);
            struct jffs2_raw_inode *ri, *tmpi;
            while (d != NULL) {
                ri = find_raw_inode( d->ino, 0);
                if (!ri) {
                    qWarning("bug: raw_inode missing!");
                    d = d->next;
                    continue;
                }

                uint32_t len = 0;
                tmpi = ri;
                while (tmpi) {
                    len = je32_to_cpu(tmpi->dsize) + je32_to_cpu(tmpi->offset);
                    tmpi = find_raw_inode(d->ino, je32_to_cpu(tmpi->version));
                }
                uint32_t timestamp = je32_to_cpu(ri->ctime);
                QString filename(QByteArray(d->name,d->nsize));
                switch (dt2fsv[d->type]) {
                    case FSView_REG_FILE:
                    case FSView_FIFO:
                    case FSView_CHARDEV:
                    case FSView_BLOCKDEV:
                    case FSView_SYMLINK:
                    case FSView_SOCKET:
                    default:
                    {
                        mode->addTree(filename, dt2fsv[d->type], len, timestamp, index);
                        break;
                    }
                    case FSView_DIR:
                    {
                        QString filePath = (path!="/")?(path+"/"+filename):("/"+filename);
                        QModelIndex modelIndex = mode->addTree(filename, dt2fsv[d->type], 0, timestamp, index);
                        listJffs2FSAll(filePath, modelIndex);
                        break;
                    }
                }

                d = d->next;
            }
            freedir(d);
        }
        qApp->processEvents();
    }

protected:
    void contextMenuEvent(QContextMenuEvent *event) override {
        QModelIndex tIndex = indexAt(viewport()->mapFromGlobal(event->globalPos()));
        if (tIndex.isValid() && m_idle) {
            int type = FSView_UNKNOWN;
            QString name;
            mode->info(tIndex, type, name);
            if((type == FSView_REG_FILE) || (type == FSView_DIR)) {
                //TODO: why this way crash?
                //QMenu *menu = new QMenu(this); 
                //menu->setAttribute(Qt::WA_DeleteOnClose); 
                // Now we renew menu, because use Qt::WA_DeleteOnClose can't work
                static QMenu *menu = nullptr;
                if(menu) delete menu;
                menu = new QMenu(this); 

                QAction *pExport= new QAction(tr("Export"), this);
                pExport->setIcon(QIcon(QFontIcon::icon(QChar(0xf019))));
                menu->addAction(pExport);
                connect(pExport,&QAction::triggered,this,
                    [&,tIndex](void)
                    {
                        QString name;
                        int type = FSView_UNKNOWN;
                        mode->info(tIndex, type, name);
                        QString path = name;
                        std::function<QModelIndex(QModelIndex,QString &)> get_parent = [&](QModelIndex index, QString &name) -> QModelIndex {
                            if(index.isValid() && index.parent().isValid()) {
                                QString pname;
                                int type = FSView_UNKNOWN;
                                QModelIndex parent = index.parent();
                                mode->info(parent, type, pname);
                                name = (pname == "/")?(pname + name):(pname + "/" + name);
                                return get_parent(parent, name);
                            } else {
                                return QModelIndex();
                            }
                        };
                        get_parent(tIndex, path);
                        if(type == FSView_DIR) {
                            QMessageBox::critical(this, tr("Error"), tr("Exporting dirs is not currently supported!"));
                            return;
                        } else if(type == FSView_REG_FILE) {
                            QString filename = QFileDialog::getSaveFileName(this, tr("Save File"), name);
                            if (filename.isEmpty())
                                return;
                            QFileInfo info(this->windowTitle());
                            int ret = -1;
                            switch(m_fsType) {
                                case 0:
                                    ret = exportExt4FSImg(this->windowTitle(), 0, info.size(), path, filename);
                                    break;
                                case 1:
                                    ret = exportFatFSImg(this->windowTitle(), 0, info.size(), path, filename);
                                    break;
                                case 2:
                                    ret = exportJFFS2Img(this->windowTitle(), 0, info.size(), path, filename);
                                    break;
                                default:
                                    break;
                            }
                            if(ret == 0) {
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
                menu->addAction(pImport);
                connect(pImport,&QAction::triggered,this,
                    [&,tIndex](void)
                    {
                        int wret = QMessageBox::warning(this,"Warning","In principle, this software does not provide the function of modifying the disk image. If you use this function, please remember to back up your files, and this software does not guarantee the strict correctness of the import.\nPlease choose whether to continue.", QMessageBox::Yes, QMessageBox::No);
                        if(wret == QMessageBox::No) {
                            return;
                        }

                        QString name;
                        int type = FSView_UNKNOWN;
                        mode->info(tIndex, type, name);
                        QString path = name;
                        std::function<QModelIndex(QModelIndex,QString &)> get_parent = [&](QModelIndex index, QString &name) -> QModelIndex {
                            if(index.isValid() && index.parent().isValid()) {
                                QString pname;
                                int type = FSView_UNKNOWN;
                                QModelIndex parent = index.parent();
                                mode->info(parent, type, pname);
                                name = (pname == "/")?(pname + name):(pname + "/" + name);
                                return get_parent(parent, name);
                            } else {
                                return QModelIndex();
                            }
                        };
                        get_parent(tIndex, path);
                        if(type == FSView_REG_FILE) {
                            QFileInfo input_info(path);
                            path = input_info.absolutePath();
                        }
                        QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::homePath());
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
                        switch(m_fsType) {
                            case 0:
                                ret = importExt4FSImg(this->windowTitle(), 0, info.size(), path, filePath);
                                if(ret == 0) {
                                    setExt4FSImgView(this->windowTitle(), 0, info.size());
                                }
                                break;
                            case 1:
                                ret = importFatFSImg(this->windowTitle(), 0, info.size(), path, filePath);
                                if(ret == 0) {
                                    setFatFSImgView(this->windowTitle(), 0, info.size());
                                }
                                break;
                            case 2:
                                ret = importJFFS2Img(this->windowTitle(), 0, info.size(), path, filePath);
                                if(ret == 0) {
                                    setJffs2FSImgView(this->windowTitle(), 0, info.size());
                                }
                                break;
                            default:
                                break;
                        }
                        if(ret == 0) {
                            QMessageBox::information(this, tr("Information"), tr("Import file success!"));
                        } else {
                            QMessageBox::critical(this, tr("Error"), tr("Unsupported operation!"));
                        }
                    }
                );
                if(!menu->isEmpty()) {
                    menu->move(cursor().pos());
                    menu->show();
                }
            }
        }
        event->accept();
    }

    void closeEvent(QCloseEvent *event) override {
        m_parent->show();
        this->hide();
        event->ignore();
    }

private:
    TreeModel *mode;
    int m_fsType;
    uint64_t m_idle;
    QWidget *m_parent;
    QModelIndex rootIndex;
};

#endif // QFSVIEWER_H

