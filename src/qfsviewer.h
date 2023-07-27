#ifndef QFSVIEWER_H
#define QFSVIEWER_H

#include <QTreeView>
#include <QFile>
#include <QCloseEvent>
#include <QApplication>
#include <QDialog>

#include "jffs2extract.h"
#include "ext4_module.h"
#include "ff.h"
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
        QFile fs_img(rootFSImgPath);
        fs_img.open(QIODevice::ReadOnly);
        uint8_t *addr = fs_img.map(offset,size);
        ext4_init(addr,size);
        listExt4FSAll("/",rootIndex);
        ext4_close();
        fs_img.unmap(addr);
        fs_img.close();
    }

    void setFatFSImgView(QString rootFSImgPath,uint64_t offset, uint64_t size) {
        resetView();
        setWindowTitle(rootFSImgPath);
        QFile fs_img(rootFSImgPath);
        fs_img.open(QIODevice::ReadOnly);
        uint8_t *addr = fs_img.map(offset,size);
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
        QFile fs_img(rootFSImgPath);
        fs_img.open(QIODevice::ReadOnly);
        uint8_t *addr = fs_img.map(offset,size);
        jffs2_init(addr,size);
        listJffs2FSAll("/",rootIndex);
        fs_img.unmap(addr);
        fs_img.close();
    }

    void resetView(void) {
        mode->removeTree(rootIndex);
        rootIndex = mode->addTree("/", 0, 0, QModelIndex());
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
        uint64_t msize = ext4_list_contents(path.toStdString().c_str(), NULL);
        uint8_t *mdata = new uint8_t[msize];
        ext4_list_contents(path.toStdString().c_str(), mdata);
        uint8_t * p = mdata;
        while(p != (mdata + msize)) {
            struct __attribute__((packed)) ext4_ino_min_map {
                uint64_t ino;
                uint8_t type;
                uint8_t size;
                char name[1];
            };
            struct ext4_ino_min_map * mm = (struct ext4_ino_min_map *) p;
            QString filename(QByteArray(mm->name,mm->size));
            switch(mm->type) {
                case FSView_DIR :
                {
                    QModelIndex modelIndex = mode->addTree(filename, mm->type, 0, index);
                    if(path != "/")
                        listExt4FSAll(path + "/" + filename, modelIndex);
                    else
                        listExt4FSAll("/" + filename, modelIndex);
                    break;
                }
                case FSView_REG_FILE :
                default :
                {
                    uint64_t rsize = ext4_get_contents(mm->ino, NULL);
                    uint8_t *rdata = new uint8_t[rsize];
                    ext4_get_contents(mm->ino, rdata);
                    uint64_t size = *(uint64_t *)(rdata+8);
                    mode->addTree(filename, mm->type, size, index);
                    free(rdata);
                    break;
                }
            }
            
            p += sizeof(uint64_t) + 2 + mm->size;
        }
        delete [] mdata;
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
                if (fno.fattrib & AM_DIR) { 
                    QString filename(QByteArray(fn,strlen(fn)));
                    QModelIndex modelIndex = mode->addTree(filename, FSView_DIR, 0, index);
                    if(path != "/")
                        listFatFSAll(path + "/" + filename, modelIndex);
                    else
                        listFatFSAll("/" + filename, modelIndex);
                    break;
                } else {
                    QString filename(QByteArray(fn,strlen(fn)));
                    uint64_t size = fno.fsize;
                    mode->addTree(filename, FSView_REG_FILE, size, index);
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
                        mode->addTree(filename, dt2fsv[d->type], len, index);
                        break;
                    }
                    case FSView_DIR:
                        break;
                }

                if (dt2fsv[d->type] == FSView_DIR) {
                    QString filename(QByteArray(d->name,d->nsize));
                    QModelIndex modelIndex = mode->addTree(filename, dt2fsv[d->type], 0, index);
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

