/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD: src/sys/sys/disk.h,v 1.26 2002/09/20 19:36:05 phk Exp $
 *
 */

#ifndef _SYS_DISK_H_
#define	_SYS_DISK_H_

#include <sys/ioccom.h>

#ifdef _KERNEL

#include <sys/queue.h>

struct disk {
	u_int			d_flags;
	u_int			d_dsflags;
	struct cdevsw		*d_devsw;
	dev_t			d_dev;

	/* These four fields must be valid while opened */
	u_int			d_sectorsize;
	off_t			d_mediasize;
	u_int			d_fwsectors;
	u_int			d_fwheads;

	struct diskslices	*d_slice;
	struct disklabel	*d_label;
	LIST_ENTRY(disk)	d_list;
	void			*d_softc;
};

#define DISKFLAG_LOCK		0x1
#define DISKFLAG_WANTED		0x2

dev_t disk_create(int unit, struct disk *disk, int flags, struct cdevsw *cdevsw, struct cdevsw *diskdevsw);
void disk_destroy(dev_t dev);
struct disk *disk_enumerate(struct disk *disk);
void disk_err(struct bio *bp, const char *what, int blkdone, int nl);
void disk_invalidate(struct disk *disk);

#endif

#define DIOCGSECTORSIZE	_IOR('d', 128, u_int)
	/*-
	 * Get the sectorsize of the device in bytes.  The sectorsize is the
	 * smallest unit of data which can be transfered from this device.
	 * Usually this is a power of two but it may not be. (ie: CDROM audio)
	 */

#define DIOCGMEDIASIZE	_IOR('d', 129, off_t)	/* Get media size in bytes */
	/*-
	 * Get the size of the entire device in bytes.  This should be a
	 * multiple of the sectorsize.
	 */

#define DIOCGFWSECTORS	_IOR('d', 130, u_int)	/* Get firmware sectorcount */
	/*-
	 * Get the firmwares notion of number of sectors per track.  This
	 * value is mostly used for compatibility with various ill designed
	 * disk label formats.  Don't use it unless you have to.
	 */

#define DIOCGFWHEADS	_IOR('d', 131, u_int)	/* Get firmware headcount */
	/*-
	 * Get the firmwares notion of number of heads per cylinder.  This
	 * value is mostly used for compatibility with various ill designed
	 * disk label formats.  Don't use it unless you have to.
	 */

#define DIOCSKERNELDUMP _IOW('d', 133, u_int)	/* Set/Clear kernel dumps */
	/*-
	 * Enable/Disable (the argument is boolean) the device for kernel
	 * core dumps.
	 */
	
#define DIOCGFRONTSTUFF _IOR('d', 134, off_t)
	/*-
	 * Many disk formats have some amount of space reserved at the
	 * start of the disk to hold bootblocks, various disklabels and
	 * similar stuff.  This ioctl returns the number of such bytes
	 * which may apply to the device.
	 */

#endif /* _SYS_DISK_H_ */
