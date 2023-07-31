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

#include "jffs2extract.h"
#include "ff.h"
#include "blockdev_port.h"
#include "treemodel.h"

class FSViewWindow : public QTreeView
{
    Q_OBJECT
public:
 explicit FSViewWindow(QDialog *parent = nullptr) :
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
        resetView();
        setWindowTitle(rootFSImgPath);
        QFileInfo fi(rootFSImgPath);
        mode->set_root_timestamp((uint32_t)fi.birthTime().toUTC().toSecsSinceEpoch());
        QFile fs_img(rootFSImgPath);
        fs_img.open(QIODevice::ReadOnly);
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
        ext4_mount("ext4_fs", "/", true);
	    ext4_recover("/");
        ext4_journal_start("/");
        ext4_cache_write_back("/", 1);
        listExt4FSAll("/",rootIndex);
        ext4_cache_write_back("/", 0);
        ext4_journal_stop("/");
        ext4_umount("/");
        ext4_device_unregister("ext4_fs");
        fs_img.unmap(addr);
        fs_img.close();
    }

    void setFatFSImgView(QString rootFSImgPath,uint64_t offset, uint64_t size) {
        resetView();
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
    }

    void setJffs2FSImgView(QString rootFSImgPath,uint64_t offset, uint64_t size) {
        resetView();
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
    }

    void resetView(void) {
        mode->removeTree(rootIndex);
        mode->set_root_timestamp(0);
        rootIndex = mode->addTree("/", 0, 0, 0, QModelIndex());
        expand(rootIndex);
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
            QString filePath;
            if(path != "/")
                filePath = path + "/" + filename;
            else
                filePath = "/" + filename;
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
                    if(path != "/")
                        listExt4FSAll(path + "/" + filename, modelIndex);
                    else
                        listExt4FSAll("/" + filename, modelIndex);
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
                if (res != FR_OK || fno.fname[0] == 0) break;
                fn = fno.fname;
                int year = ((fno.fdate & 0b1111111000000000) >> 9) + 1980;
                int month = (fno.fdate & 0b0000000111100000) >> 5;
                int day =  fno.fdate & 0b0000000000011111;
                int hour = (fno.ftime & 0b111110000000000) >> 11;
                int minute = (fno.ftime & 0b0000011111100000) >> 5;
                int second = (fno.ftime & 0b0000000000011111) * 2;
                QDateTime dt(QDate(year, month, day), QTime(hour, minute, second));

                if (fno.fattrib & AM_DIR) { 
                    QString filename(QByteArray(fn,strlen(fn)));
                    QModelIndex modelIndex = mode->addTree(filename, FSView_DIR, fno.fsize, dt.toSecsSinceEpoch(), index);
                    if(path != "/")
                        listFatFSAll(path + "/" + filename, modelIndex);
                    else
                        listFatFSAll("/" + filename, modelIndex);
                } else {
                    QString filename(QByteArray(fn,strlen(fn)));
                    uint64_t size = fno.fsize;
                    mode->addTree(filename, FSView_REG_FILE, size, dt.toSecsSinceEpoch(), index);
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
            qDebug("No such file or directory");
        else if ((dd == NULL && ino != 0) || (dd != NULL && dt2fsv[dd->type] == FSView_DIR)) {
            d = collectdir( ino, d);
            struct jffs2_raw_inode *ri, *tmpi;
            while (d != NULL) {
                ri = find_raw_inode( d->ino, 0);
                if (!ri) {
                    qDebug("bug: raw_inode missing!");
                    d = d->next;
                    continue;
                }
                /* Search for newer versions of the inode */
                uint32_t len = 0;
                tmpi = ri;
                while (tmpi) {
                    len = je32_to_cpu(tmpi->dsize) + je32_to_cpu(tmpi->offset);
                    tmpi = find_raw_inode(d->ino, je32_to_cpu(tmpi->version));
                }
                uint32_t timestamp = je32_to_cpu(ri->ctime);
                switch (dt2fsv[d->type]) {
                    case FSView_REG_FILE:
                    case FSView_FIFO:
                    case FSView_CHARDEV:
                    case FSView_BLOCKDEV:
                    case FSView_SYMLINK:
                    case FSView_SOCKET:
                    default:
                    {
                        QString filename(QByteArray(d->name,d->nsize));
                        mode->addTree(filename, dt2fsv[d->type], len, timestamp, index);
                        break;
                    }
                    case FSView_DIR:
                        break;
                }

                if (dt2fsv[d->type] == FSView_DIR) {
                    QString filename(QByteArray(d->name,d->nsize));
                    QModelIndex modelIndex = mode->addTree(filename, dt2fsv[d->type], 0, timestamp, index);
                    if(path != "/")
                        listJffs2FSAll(path + "/" + filename, modelIndex);
                    else
                        listJffs2FSAll("/" + filename, modelIndex);
                }

                d = d->next;
            }
            freedir(d);
        }
        qApp->processEvents();
    }

protected:
    void closeEvent(QCloseEvent *event) {
        m_parent->show();
        this->hide();
        event->ignore();
    }

private:
    TreeModel *mode;
    QDialog *m_parent;
    QModelIndex rootIndex;
};

#endif // QFSVIEWER_H

