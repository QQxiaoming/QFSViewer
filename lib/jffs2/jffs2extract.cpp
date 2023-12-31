/* vi: set sw=4 ts=4: */
/*
 * jffs2extract v0.1: Extract the contents of a JFFS2 image file.
 *
 * Based on jffs2reader by Jari Kirma
 *
 * Copyright (c) 2014 Rickard Lyrenius
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the author be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must
 * not claim that you wrote the original software. If you use this
 * software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must
 * not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 *
 * 
 *
 * Usage: jffs2extract {-t | -x} [-f imagefile] [-C path] [-v] [file1 [file2 ...]]
 *
 * Options mimic the 'tar' command as close as possible.
 *
 */

#define PROGRAM_NAME "jffs2reader"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "crc32.h"
#include "jffs2extract.h"
#include "mini_inflate.h"
#include "common.h"

#if defined(__WINDOWS__)
#define DT_UNKNOWN	0
#define DT_FIFO		1
#define DT_CHR		2
#define DT_DIR		4
#define DT_BLK		6
#define DT_REG		8
#define DT_LNK		10
#define DT_SOCK		12
#define DT_WHT		14
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)
#define bcopy(b1,b2,len) (memmove((b2), (b1), (len)), (void) 0)
#endif

static uint8_t * ram_disk_data = nullptr;
static uint64_t ram_disk_size = 0;

#define SCRATCH_SIZE (5*1024*1024)

/* macro to avoid "lvalue required as left operand of assignment" error */
#define ADD_BYTES(p, n)		((p) = (typeof(p))((char *)(p) + (n)))

#define DIRENT_INO(dirent) ((dirent) !=NULL ? je32_to_cpu((dirent)->ino) : 0)
#define DIRENT_PINO(dirent) ((dirent) !=NULL ? je32_to_cpu((dirent)->pino) : 0)

int target_endian = __BYTE_ORDER;

void putblock(char *, size_t, size_t *, struct jffs2_raw_inode *);
struct dir *putdir(struct dir *, struct jffs2_raw_dirent *);
void printdir( struct dir *d, const char *path, 
     int verbose);
void freedir(struct dir *);

long zlib_decompress(unsigned char *data_in, unsigned char *cpage_out )
{
    return (decompress_block(cpage_out, data_in + 2));
}

static int jffs2_rtime_decompress(unsigned char *data_in,
                  unsigned char *cpage_out, uint32_t destlen)
{
	short positions[256];
    uint32_t outpos = 0;
	int pos=0;
	memset(positions,0,sizeof(positions));
	while (outpos<destlen) {
		unsigned char value;
        uint32_t backoffs;
        uint32_t repeat;
		value = data_in[pos++];
		cpage_out[outpos++] = value; /* first the verbatim copied byte */
		repeat = data_in[pos++];
		backoffs = positions[value];
		positions[value]=outpos;
		if (repeat) {
			if (backoffs + repeat >= outpos) {
				while(repeat) {
					cpage_out[outpos++] = cpage_out[backoffs++];
					repeat--;
				}
			} else {
				memcpy(&cpage_out[outpos],&cpage_out[backoffs],repeat);
				outpos+=repeat;
			}
		}
	}
	return 0;
}

#define RUBIN_REG_SIZE   16
#define UPPER_BIT_RUBIN    (((long) 1)<<(RUBIN_REG_SIZE-1))
#define LOWER_BITS_RUBIN   ((((long) 1)<<(RUBIN_REG_SIZE-1))-1)

void rubin_do_decompress(unsigned char *bits, unsigned char *in,
			 unsigned char *page_out, uint32_t destlen)
{
	char *curr = (char *)page_out;
	char *end = (char *)(page_out + destlen);
	uint32_t temp;
	uint32_t result;
	uint32_t p;
	uint32_t q;
	uint32_t rec_q;
	uint32_t bit;
	long i0;
	uint32_t i;

	/* init_pushpull */
	temp = *(uint32_t *) in;
	bit = 16;

	/* init_rubin */
	q = 0;
	p = (long) (2 * UPPER_BIT_RUBIN);

	/* init_decode */
	rec_q = (in[0] << 8) | in[1];

	while (curr < end) {
		/* in byte */

		result = 0;
		for (i = 0; i < 8; i++) {
			/* decode */

			while ((q & UPPER_BIT_RUBIN) || ((p + q) <= UPPER_BIT_RUBIN)) {
				q &= ~UPPER_BIT_RUBIN;
				q <<= 1;
				p <<= 1;
				rec_q &= ~UPPER_BIT_RUBIN;
				rec_q <<= 1;
				rec_q |= (temp >> (bit++ ^ 7)) & 1;
				if (bit > 31) {
					uint32_t *p = (uint32_t *)in;
					bit = 0;
					temp = *(++p);
					in = (unsigned char *)p;
				}
			}
			i0 =  (bits[i] * p) >> 8;

			if (i0 <= 0) i0 = 1;
			/* if it fails, it fails, we have our crc
			if (i0 >= p) i0 = p - 1; */

			result >>= 1;
			if (rec_q < q + i0) {
				/* result |= 0x00; */
				p = i0;
			} else {
				result |= 0x80;
				p -= i0;
				q += i0;
			}
		}
		*(curr++) = result;
	}
}

void dynrubin_decompress(unsigned char *data_in, unsigned char *cpage_out, uint32_t dstlen)
{
	unsigned char bits[8];
	int c;

	for (c=0; c<8; c++)
		bits[c] = (256 - data_in[c]);

	rubin_do_decompress(bits, data_in+8, cpage_out, dstlen);
}

/* writes file node into buffer, to the proper position. */
/* reading all valid nodes in version order reconstructs the file. */

/*
   b       - buffer
   bsize   - buffer size
   rsize   - result size
   n       - node
 */

void putblock(char *b, size_t bsize, size_t * rsize,
		struct jffs2_raw_inode *n)
{
    unsigned long dlen = je32_to_cpu(n->dsize);

	if (je32_to_cpu(n->isize) > bsize || (je32_to_cpu(n->offset) + dlen) > bsize)
	{
		errmsg_die("File does not fit into buffer!");
	}

	if (*rsize < je32_to_cpu(n->isize))
		bzero(b + *rsize, je32_to_cpu(n->isize) - *rsize);

	switch (n->compr) {
		case JFFS2_COMPR_ZLIB:
            zlib_decompress((unsigned char *) ((char *) n) + sizeof(struct jffs2_raw_inode),
                    (unsigned char *) (b + je32_to_cpu(n->offset)));
			break;

		case JFFS2_COMPR_NONE:
			memcpy(b + je32_to_cpu(n->offset),
					((char *) n) + sizeof(struct jffs2_raw_inode), dlen);
			break;

		case JFFS2_COMPR_ZERO:
			bzero(b + je32_to_cpu(n->offset), dlen);
			break;

		case JFFS2_COMPR_RTIME:
			jffs2_rtime_decompress((unsigned char *) ((char *) n) + sizeof(struct jffs2_raw_inode),
                    (unsigned char *) (b + je32_to_cpu(n->offset)),je32_to_cpu(n->dsize));
			break;
			
		case JFFS2_COMPR_DYNRUBIN:
			dynrubin_decompress((unsigned char *) ((char *) n) + sizeof(struct jffs2_raw_inode),
                    (unsigned char *) (b + je32_to_cpu(n->offset)), je32_to_cpu(n->dsize));
			break;

		default:
			errmsg_die("Unsupported compression method %d!",n->compr);
	}

	*rsize = je32_to_cpu(n->isize);
}

/* adds/removes directory node into dir struct. */
/* reading all valid nodes in version order reconstructs the directory. */

/*
   dd      - directory struct being processed
   n       - node

   return value: directory struct value replacing dd
 */

struct dir *putdir(struct dir *dd, struct jffs2_raw_dirent *n)
{
	struct dir *o, *d, *p;

	o = dd;

	if (je32_to_cpu(n->ino)) {
        if (dd == NULL) {
            d = (struct dir *)xmalloc(sizeof(struct dir));
			d->type = n->type;
			memcpy(d->name, n->name, n->nsize);
			d->nsize = n->nsize;
			d->ino = je32_to_cpu(n->ino);
			d->next = NULL;

			return d;
		}

		while (1) {
			if (n->nsize == dd->nsize &&
					!memcmp(n->name, dd->name, n->nsize)) {
				dd->type = n->type;
				dd->ino = je32_to_cpu(n->ino);

				return o;
			}

			if (dd->next == NULL) {
                dd->next = (struct dir *)xmalloc(sizeof(struct dir));
				dd->next->type = n->type;
				memcpy(dd->next->name, n->name, n->nsize);
				dd->next->nsize = n->nsize;
				dd->next->ino = je32_to_cpu(n->ino);
				dd->next->next = NULL;

				return o;
			}

			dd = dd->next;
		}
	} else {
		if (dd == NULL)
			return NULL;

		if (n->nsize == dd->nsize && !memcmp(n->name, dd->name, n->nsize)) {
			d = dd->next;
			free(dd);
			return d;
		}

		while (1) {
			p = dd;
			dd = dd->next;

			if (dd == NULL)
				return o;

			if (n->nsize == dd->nsize &&
					!memcmp(n->name, dd->name, n->nsize)) {
				p->next = dd->next;
				free(dd);

				return o;
			}
		}
	}
}

#if defined(__WINDOWS__)
#define S_IFSOCK 0140000
#define S_IFLNK	 0120000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m)	(((m) & S_IFMT) == S_IFSOCK)
#endif

#define TYPEINDEX(mode) (((mode) >> 12) & 0x0f)
#define TYPECHAR(mode)  ("0pcCd?bB-?l?s???" [TYPEINDEX(mode)])

/* The special bits. If set, display SMODE0/1 instead of MODE0/1 */
static const mode_t SBIT[] = {
	0, 0, S_ISUID,
	0, 0, S_ISGID,
	0, 0, S_ISVTX
};

/* The 9 mode bits to test */
static const mode_t MBIT[] = {
	S_IRUSR, S_IWUSR, S_IXUSR,
	S_IRGRP, S_IWGRP, S_IXGRP,
	S_IROTH, S_IWOTH, S_IXOTH
};

static const char MODE1[] = "rwxrwxrwx";
static const char MODE0[] = "---------";
static const char SMODE1[] = "..s..s..t";
static const char SMODE0[] = "..S..S..T";

/*
 * Return the standard ls-like mode string from a file mode.
 * This is static and so is overwritten on each call.
 */
const char *mode_string(int mode)
{
	static char buf[12];

	int i;

	buf[0] = TYPECHAR(mode);
	for (i = 0; i < 9; i++) {
		if (mode & SBIT[i])
			buf[i + 1] = (mode & MBIT[i]) ? SMODE1[i] : SMODE0[i];
		else
			buf[i + 1] = (mode & MBIT[i]) ? MODE1[i] : MODE0[i];
	}
	return buf;
}

void freedir(struct dir *d)
{
	struct dir *t;

	while (d != NULL) {
		t = d->next;
		free(d);
		d = t;
	}
}

struct jffs2_raw_inode *find_raw_inode(uint32_t ino, 
	uint32_t vcur)
{
	/* aligned! */
	union jffs2_node_union *n;
	union jffs2_node_union *e = (union jffs2_node_union *) (ram_disk_data + ram_disk_size);
	union jffs2_node_union *lr;	/* last block position */
	union jffs2_node_union *mp = NULL;	/* minimum position */

	uint32_t vmin, vmint, vmaxt, vmax, v;

	vmin = 0;					/* next to read */
	vmax = ~((uint32_t) 0);		/* last to read */
	vmint = ~((uint32_t) 0);
	vmaxt = 0;					/* found maximum */

    n = (union jffs2_node_union *) ram_disk_data;
	lr = n;

	do {
		while (n < e && je16_to_cpu(n->u.magic) != JFFS2_MAGIC_BITMASK)
			ADD_BYTES(n, 4);

		if (n < e && je16_to_cpu(n->u.magic) == JFFS2_MAGIC_BITMASK) {
			if (je16_to_cpu(n->u.nodetype) == JFFS2_NODETYPE_INODE &&
				je32_to_cpu(n->i.ino) == ino && (v = je32_to_cpu(n->i.version)) > vcur) {
				/* XXX crc check */

				if (vmaxt < v)
					vmaxt = v;
				if (vmint > v) {
					vmint = v;
					mp = n;
				}

				if (v == (vcur + 1))
					return (&(n->i));
			}

			ADD_BYTES(n, ((je32_to_cpu(n->u.totlen) + 3) & ~3));
		} else
			n = (union jffs2_node_union *) ram_disk_data;	/* we're at the end, rewind to the beginning */

		if (lr == n) {			/* whole loop since last read */
			vmax = vmaxt;
			vmin = vmint;
			vmint = ~((uint32_t) 0);

			if (vcur < vmax && vcur < vmin)
				return (&(mp->i));
		}
	} while (vcur < vmax);

	return NULL;
}

struct dir *collectdir(uint32_t ino, struct dir *d)
{
	/* aligned! */
	union jffs2_node_union *n;
	union jffs2_node_union *e = (union jffs2_node_union *) (ram_disk_data + ram_disk_size);
	union jffs2_node_union *lr;	/* last block position */
	union jffs2_node_union *mp = NULL;	/* minimum position */

	uint32_t vmin, vmint, vmaxt, vmax, vcur, v;

	vmin = 0;					/* next to read */
	vmax = ~((uint32_t) 0);		/* last to read */
	vmint = ~((uint32_t) 0);
	vmaxt = 0;					/* found maximum */
	vcur = 0;					/* XXX what is smallest version number used? */
	/* too low version number can easily result excess log rereading */

    n = (union jffs2_node_union *) ram_disk_data;
	lr = n;

	do {
		while (n < e && je16_to_cpu(n->u.magic) != JFFS2_MAGIC_BITMASK)
			ADD_BYTES(n, 4);

		if (n < e && je16_to_cpu(n->u.magic) == JFFS2_MAGIC_BITMASK) {
			if (je16_to_cpu(n->u.nodetype) == JFFS2_NODETYPE_DIRENT &&
				je32_to_cpu(n->d.pino) == ino && (v = je32_to_cpu(n->d.version)) >= vcur) {
				/* XXX crc check */

				if (vmaxt < v)
					vmaxt = v;
				if (vmint > v) {
					vmint = v;
					mp = n;
				}

				if (v == (vcur)) {
					d = putdir(d, &(n->d));

					lr = n;
					vcur++;
					vmint = ~((uint32_t) 0);
				}
			}

			ADD_BYTES(n, ((je32_to_cpu(n->u.totlen) + 3) & ~3));
		} else
			n = (union jffs2_node_union *) ram_disk_data;	/* we're at the end, rewind to the beginning */

		if (lr == n) {			/* whole loop since last read */
			vmax = vmaxt;
			vmin = vmint;
			vmint = ~((uint32_t) 0);

            if (vcur <= vmax && vcur <= vmin) {
                if(mp) {
                    d = putdir(d, &(mp->d));

                    lr = n =
                        (union jffs2_node_union *) (((char *) mp) +
                                ((je32_to_cpu(mp->u.totlen) + 3) & ~3));

                    vcur = vmin;
                } else {
                    vcur = vmin;
                }
			}
		}
    } while (vcur <= vmax);

	return d;
}

int deletenode(uint32_t ino)
{
	int ret = -1;
	union jffs2_node_union *e = (union jffs2_node_union *) (ram_disk_data + ram_disk_size);
    union jffs2_node_union *n = (union jffs2_node_union *) ram_disk_data;
	do {
		while (n < e && je16_to_cpu(n->u.magic) != JFFS2_MAGIC_BITMASK)
			ADD_BYTES(n, 4);

		if (n < e && je16_to_cpu(n->u.magic) == JFFS2_MAGIC_BITMASK) {
			if(je16_to_cpu(n->u.nodetype) == JFFS2_NODETYPE_DIRENT) {
				if(ino == je32_to_cpu(n->d.ino)) {
                    n->u.magic = cpu_to_je16(0);
					ret = 0;
				}
			} else if (je16_to_cpu(n->u.nodetype) == JFFS2_NODETYPE_INODE) {
				if(ino == je32_to_cpu(n->i.ino)) {
                    n->u.magic = cpu_to_je16(0);
					ret = 0;
				}
			}
			ADD_BYTES(n, ((je32_to_cpu(n->u.totlen) + 3) & ~3));
		} else
			break;
    } while (1);
	return ret;
}

void find_free(uint32_t *ino, uint64_t *offset)
{
	union jffs2_node_union *e = (union jffs2_node_union *) (ram_disk_data + ram_disk_size);
    union jffs2_node_union *n = (union jffs2_node_union *) ram_disk_data;
	*ino = 0;
	*offset = 0;
	do {
		while (n < e && je16_to_cpu(n->u.magic) != JFFS2_MAGIC_BITMASK)
			ADD_BYTES(n, 4);

		if (n < e && je16_to_cpu(n->u.magic) == JFFS2_MAGIC_BITMASK) {
			if(je16_to_cpu(n->u.nodetype) == JFFS2_NODETYPE_DIRENT) {
				if(*ino < je32_to_cpu(n->d.ino))
					*ino = je32_to_cpu(n->d.ino);
			} else if (je16_to_cpu(n->u.nodetype) == JFFS2_NODETYPE_INODE) {
				if(*ino < je32_to_cpu(n->i.ino))
					*ino = je32_to_cpu(n->i.ino);
			} else if (je16_to_cpu(n->u.nodetype) == JFFS2_NODETYPE_XREF) {
				if(*ino < je32_to_cpu(n->r.ino))
					*ino = je32_to_cpu(n->r.ino);
			}
			ADD_BYTES(n, ((je32_to_cpu(n->u.totlen) + 3) & ~3));
			*offset = ((uint8_t *)n) - ram_disk_data;
		} else
			break;
    } while (1);
	*ino += 1;
	*offset += 1;
	if(*offset > ram_disk_size )
		*offset = ram_disk_size;
}

/* resolve dirent based on criteria */
/*
   o       - filesystem image pointer
   size    - size of filesystem image
   ino     - if zero, ignore,
   otherwise compare against dirent inode
   pino    - if zero, ingore,
   otherwise compare against parent inode
   and use name and nsize as extra criteria
   name    - name of wanted dirent, used if pino!=0
   nsize   - length of name of wanted dirent, used if pino!=0

   return value: pointer to relevant dirent structure in
   filesystem image or NULL
 */

struct jffs2_raw_dirent *resolvedirent(
		uint32_t ino, uint32_t pino,
		char *name, uint8_t nsize)
{
	/* aligned! */
	union jffs2_node_union *n;
	union jffs2_node_union *e = (union jffs2_node_union *) (ram_disk_data + ram_disk_size);

	struct jffs2_raw_dirent *dd = NULL;

	uint32_t vmax, v;

	if (!pino && ino <= 1)
		return dd;

	vmax = 0;

    n = (union jffs2_node_union *) ram_disk_data;

	do {
		while (n < e && je16_to_cpu(n->u.magic) != JFFS2_MAGIC_BITMASK)
			ADD_BYTES(n, 4);

		if (n < e && je16_to_cpu(n->u.magic) == JFFS2_MAGIC_BITMASK) {
			if (je16_to_cpu(n->u.nodetype) == JFFS2_NODETYPE_DIRENT &&
					(!ino || je32_to_cpu(n->d.ino) == ino) &&
					(v = je32_to_cpu(n->d.version)) >= vmax &&
					(!pino || (je32_to_cpu(n->d.pino) == pino &&
							   nsize == n->d.nsize &&
							   !memcmp(name, n->d.name, nsize)))) {
				/* XXX crc check */

				if (vmax <= v) {
					vmax = v;
					dd = &(n->d);
				}
			}

			ADD_BYTES(n, ((je32_to_cpu(n->u.totlen) + 3) & ~3));
		} else
			return dd;
	} while (1);
}

/* resolve name under certain parent inode to dirent */

/*
   o       - filesystem image pointer
   size    - size of filesystem image
   pino    - requested parent inode
   name    - name of wanted dirent
   nsize   - length of name of wanted dirent

   return value: pointer to relevant dirent structure in
   filesystem image or NULL
 */

struct jffs2_raw_dirent *resolvename( uint32_t pino,
		char *name, uint8_t nsize)
{
	return resolvedirent(0, pino, name, nsize);
}

/* resolve inode to dirent */

/*
   o       - filesystem image pointer
   size    - size of filesystem image
   ino     - compare against dirent inode

   return value: pointer to relevant dirent structure in
   filesystem image or NULL
 */

struct jffs2_raw_dirent *resolveinode(uint32_t ino)
{
	return resolvedirent(ino, 0, NULL, 0);
}

struct jffs2_raw_dirent *resolvepath0(uint32_t ino,
		const char *p, uint32_t * inos, int recc)
{
	struct jffs2_raw_dirent *dir = NULL;

	int d = 1;
	uint32_t tino;

	char *next;

	char *path, *pp;

	char symbuf[1024];
	size_t symsize;

	if (recc > 16) {
		/* probably symlink loop */
		*inos = 0;
		return NULL;
	}

	pp = path = strdup(p);

	if (*path == '/') {
		path++;
        ino = 1;
    }

	if (ino > 1) {
		dir = resolveinode(ino);

		ino = DIRENT_INO(dir);
	}

    next = path - 1;
	while (ino && next != NULL && next[1] != 0 && d) {
		path = next + 1;
		next = strchr(path, '/');
		
		if (next != NULL)
			*next = 0;

		if (*path == '.' && path[1] == 0)
			continue;
		if (*path == '.' && path[1] == '.' && path[2] == 0) {
			if (DIRENT_PINO(dir) == 1) {
				ino = 1;
				dir = NULL;
			} else {
				dir = resolveinode(DIRENT_PINO(dir));
				ino = DIRENT_INO(dir);
			}

			continue;
		}

		dir = resolvename(ino, path, (uint8_t) strlen(path));

		if (DIRENT_INO(dir) == 0 ||
				(next != NULL &&
				 !(dir->type == DT_DIR || dir->type == DT_LNK))) {
			free(pp);

			*inos = 0;

			return NULL;
		}

		if (dir->type == DT_LNK) {
			struct jffs2_raw_inode *ri;
			ri = find_raw_inode(DIRENT_INO(dir), 0);
			putblock(symbuf, sizeof(symbuf), &symsize, ri);
			symbuf[symsize] = 0;

			tino = ino;
			ino = 0;

			dir = resolvepath0(tino, symbuf, &ino, ++recc);

			if (dir != NULL && next != NULL &&
					!(dir->type == DT_DIR || dir->type == DT_LNK)) {
				free(pp);

				*inos = 0;
				return NULL;
			}
		}
		if (dir != NULL)
			ino = DIRENT_INO(dir);
	}

	free(pp);

	*inos = ino;

	return dir;
}

struct jffs2_raw_dirent *resolvepath(uint32_t ino,
		const char *p, uint32_t * inos)
{
	return resolvepath0(ino, p, inos, 0);
}

uint16_t jffs2_compress( unsigned char *data_in, unsigned char **cpage_out,
		uint32_t *datalen, uint32_t *cdatalen)
{
	int ret = JFFS2_COMPR_NONE;
    unsigned char *output_buf = NULL;

	if (ret == JFFS2_COMPR_NONE) {
		*cpage_out = data_in;
		*datalen = *cdatalen;
	}
	else {
		*cpage_out = output_buf;
	}
	return ret;
}

static uint64_t pre_pad_block(uint64_t offset, uint32_t req, uint32_t add_cleanmarkers, uint32_t erase_block_size)
{
	const static unsigned char ffbuf[16] =
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff
	};
    const struct jffs2_unknown_node cleanmarker = {
        .magic = {0}, .nodetype = {0}, .totlen = {0}, .hdr_crc = {0}};
    const int cleanmarker_size = sizeof(cleanmarker);

	if(erase_block_size) {
		if (add_cleanmarkers) {
			if ((offset % erase_block_size) == 0) {
				memcpy(ram_disk_data + offset, &cleanmarker,  sizeof(cleanmarker));
				offset += sizeof(cleanmarker);
                unsigned int req = cleanmarker_size - sizeof(cleanmarker);
				while (req) {
					if (req > sizeof(ffbuf)) {
						memcpy(ram_disk_data + offset, ffbuf,  sizeof(ffbuf));
						offset += sizeof(ffbuf);
						req -= sizeof(ffbuf);
					} else {
						memcpy(ram_disk_data + offset, ffbuf, req);
						offset += req;
						req = 0;
					}
				}
				if (offset % 4) {
					memcpy(ram_disk_data + offset, ffbuf, 4 - (offset % 4));
					offset += 4 - (offset % 4);
				}
			}
		}
		if ((offset % erase_block_size) + req > erase_block_size) {
			while (offset % erase_block_size) {
				memcpy(ram_disk_data + offset, ffbuf,  min(sizeof(ffbuf),
							erase_block_size - (offset % erase_block_size)));
				offset += min(sizeof(ffbuf), erase_block_size - (offset % erase_block_size));
			}
		}
		if (add_cleanmarkers) {
			if ((offset % erase_block_size) == 0) {
				memcpy(ram_disk_data + offset, &cleanmarker,  sizeof(cleanmarker));
				offset += sizeof(cleanmarker);
                uint32_t req = cleanmarker_size - sizeof(cleanmarker);
				while (req) {
					if (req > sizeof(ffbuf)) {
						memcpy(ram_disk_data + offset, ffbuf,  sizeof(ffbuf));
						offset += sizeof(ffbuf);
						req -= sizeof(ffbuf);
					} else {
						memcpy(ram_disk_data + offset, ffbuf, req);
						offset += req;
						req = 0;
					}
				}
				if (offset % 4) {
					memcpy(ram_disk_data + offset, ffbuf, 4 - (offset % 4));
					offset += 4 - (offset % 4);
				}
			}
		}
	}
	return offset;
}

static uint64_t post_pad_block(uint64_t offset)
{
	const static unsigned char ffbuf[16] =
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff
	};
	if (offset % 4) {
		memcpy(ram_disk_data + offset, ffbuf, 4 - (offset % 4));
		offset += 4 - (offset % 4);
	}
	return offset;
}

static uint64_t write_format_data(const uint8_t *data, uint64_t size, uint64_t offset)
{
	memcpy(ram_disk_data + offset, data, size);
	offset += size;
	return offset;
}

void write_dir(const char *name, uint32_t pino, uint32_t ino, uint32_t timestamp,
				uint64_t offset, int add_cleanmarkers,int erase_block_size )
{
	struct jffs2_raw_dirent rd;
	memset(&rd, 0, sizeof(rd));
	rd.magic = cpu_to_je16(JFFS2_MAGIC_BITMASK);
	rd.nodetype = cpu_to_je16(JFFS2_NODETYPE_DIRENT);
	rd.totlen = cpu_to_je32(sizeof(rd) + strlen(name));
	rd.hdr_crc = cpu_to_je32(mtd_crc32(0, &rd,
				sizeof(struct jffs2_unknown_node) - 4));
    rd.pino = cpu_to_je32(pino);
    rd.version = cpu_to_je32(1);
    rd.ino = cpu_to_je32(ino);
    rd.mctime = cpu_to_je32(timestamp);//st_mtime
	rd.nsize = strlen(name);
	rd.type = 4;
	//rd.unused[0] = 0;
	//rd.unused[1] = 0;
	rd.node_crc = cpu_to_je32(mtd_crc32(0, &rd, sizeof(rd) - 8));
	rd.name_crc = cpu_to_je32(mtd_crc32(0, name, strlen(name)));

	offset = pre_pad_block(offset, sizeof(rd) + rd.nsize,add_cleanmarkers,erase_block_size);
	offset = write_format_data((uint8_t *)&rd, sizeof(rd), offset);
    offset = write_format_data((uint8_t *)name, rd.nsize, offset);
    offset = post_pad_block(offset);

	struct jffs2_raw_inode ri;
	memset(&ri, 0, sizeof(ri));
	ri.magic = cpu_to_je16(JFFS2_MAGIC_BITMASK);
	ri.nodetype = cpu_to_je16(JFFS2_NODETYPE_INODE);
	ri.totlen = cpu_to_je32(sizeof(ri));
	ri.hdr_crc = cpu_to_je32(mtd_crc32(0,
				&ri, sizeof(struct jffs2_unknown_node) - 4));
	ri.ino = cpu_to_je32(ino);
	ri.mode = cpu_to_jemode(S_IFDIR);//st_mode
	ri.uid = cpu_to_je16(0);//st_uid
	ri.gid = cpu_to_je16(0);//st_gid
	ri.atime = cpu_to_je32(0);//st_atime
	ri.ctime = cpu_to_je32(0);//st_ctime
	ri.mtime = cpu_to_je32(0);//st_mtime
	ri.isize = cpu_to_je32(0);
	ri.version = cpu_to_je32(1);
	ri.csize = cpu_to_je32(0);
	ri.dsize = cpu_to_je32(0);
	ri.node_crc = cpu_to_je32(mtd_crc32(0, &ri, sizeof(ri) - 8));
	ri.data_crc = cpu_to_je32(0);

    offset = pre_pad_block(offset, sizeof(ri),add_cleanmarkers,erase_block_size);
	offset = write_format_data((uint8_t *)&ri, sizeof(ri), offset);
	offset = post_pad_block(offset);
}

void write_file(const char *name, const unsigned char *buff, size_t size, 
				uint32_t pino, uint32_t ino, uint32_t timestamp,
				uint64_t offset, int add_cleanmarkers,int erase_block_size )
{
	struct jffs2_raw_dirent rd;
	memset(&rd, 0, sizeof(rd));
	rd.magic = cpu_to_je16(JFFS2_MAGIC_BITMASK);
	rd.nodetype = cpu_to_je16(JFFS2_NODETYPE_DIRENT);
	rd.totlen = cpu_to_je32(sizeof(rd) + strlen(name));
	rd.hdr_crc = cpu_to_je32(mtd_crc32(0, &rd,
				sizeof(struct jffs2_unknown_node) - 4));
    rd.pino = cpu_to_je32(pino);
    rd.version = cpu_to_je32(1);
    rd.ino = cpu_to_je32(ino);
	rd.mctime = cpu_to_je32(0);//st_mtime
	rd.nsize = strlen(name);
	rd.type = 8;
	//rd.unused[0] = 0;
	//rd.unused[1] = 0;
	rd.node_crc = cpu_to_je32(mtd_crc32(0, &rd, sizeof(rd) - 8));
	rd.name_crc = cpu_to_je32(mtd_crc32(0, name, strlen(name)));

	offset = pre_pad_block(offset, sizeof(rd) + rd.nsize,add_cleanmarkers,erase_block_size);
	offset = write_format_data((uint8_t *)&rd, sizeof(rd), offset);
	offset = write_format_data((uint8_t *)name, rd.nsize, offset);
	offset = post_pad_block(offset);

	struct jffs2_raw_inode ri;
	memset(&ri, 0, sizeof(ri));
	ri.magic = cpu_to_je16(JFFS2_MAGIC_BITMASK);
	ri.nodetype = cpu_to_je16(JFFS2_NODETYPE_INODE);
	ri.ino = cpu_to_je32(ino);
	ri.mode = cpu_to_jemode(S_IFREG);//st_mode
	ri.uid = cpu_to_je16(0);//st_uid
	ri.gid = cpu_to_je16(0);//st_gid
	ri.atime = cpu_to_je32(timestamp);//st_atime
	ri.ctime = cpu_to_je32(timestamp);//st_ctime
	ri.mtime = cpu_to_je32(timestamp);//st_mtime
	ri.isize = cpu_to_je32(size);

	if(size == 0) {
		/* Was empty file */
		ri.version = cpu_to_je32(1);
		ri.totlen = cpu_to_je32(sizeof(ri));
		ri.hdr_crc = cpu_to_je32(mtd_crc32(0,
					&ri, sizeof(struct jffs2_unknown_node) - 4));
		ri.csize = cpu_to_je32(0);
		ri.dsize = cpu_to_je32(0);
		ri.node_crc = cpu_to_je32(mtd_crc32(0, &ri, sizeof(ri) - 8));

		offset = pre_pad_block(offset, sizeof(ri),add_cleanmarkers,erase_block_size);
		offset = write_format_data((uint8_t *)&ri, sizeof(ri), offset);
		offset = post_pad_block(offset);
    } else {
        unsigned char *tbuf = (unsigned char *)buff;
		uint32_t ver = 0, woff = 0;
        unsigned char *cbuf, *wbuf;
		while (size) {
			uint32_t dsize, space;
			uint16_t compression;
			offset = pre_pad_block(offset, sizeof(ri) + JFFS2_MIN_DATA_LEN,add_cleanmarkers,erase_block_size);
			dsize = size;
            space = erase_block_size - (offset % erase_block_size) - sizeof(ri);
			if (space > dsize)
				space = dsize;
			compression = jffs2_compress(tbuf, &cbuf, &dsize, &space);
			ri.compr = compression & 0xff;
			ri.usercompr = (compression >> 8) & 0xff;

			if (ri.compr) {
				wbuf = cbuf;
			} else {
				wbuf = tbuf;
				dsize = space;
			}
			ri.totlen = cpu_to_je32(sizeof(ri) + space);
			ri.hdr_crc = cpu_to_je32(mtd_crc32(0,
						&ri, sizeof(struct jffs2_unknown_node) - 4));
            ++ver;
            ri.version = cpu_to_je32(ver);
			ri.offset = cpu_to_je32(woff);
			ri.csize = cpu_to_je32(space);
			ri.dsize = cpu_to_je32(dsize);
			ri.node_crc = cpu_to_je32(mtd_crc32(0, &ri, sizeof(ri) - 8));
			ri.data_crc = cpu_to_je32(mtd_crc32(0, wbuf, space));

			offset = write_format_data((uint8_t *)&ri, sizeof(ri), offset);
			offset = write_format_data((uint8_t *)wbuf, space, offset);
			offset = post_pad_block(offset);

			if (tbuf != cbuf) {
				free(cbuf);
				cbuf = NULL;
			}

			tbuf += dsize;
			size -= dsize;
			woff += dsize;
		}
	}
}

void jffs2_init(uint8_t * data, uint64_t data_size) {
	ram_disk_data = data;
	ram_disk_size = data_size;
}
