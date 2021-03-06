/*
 * Copyright (c) 1994-1995 S�ren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/compat/linux/linux_ioctl.c,v 1.95 2002/10/19 21:11:43 marcel Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/cdio.h>
#include <sys/dvdio.h>
#include <sys/consio.h>
#include <sys/ctype.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/kbio.h>
#include <sys/linker_set.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/soundcard.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>

#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_util.h>

static linux_ioctl_function_t linux_ioctl_cdrom;
static linux_ioctl_function_t linux_ioctl_vfat;
static linux_ioctl_function_t linux_ioctl_console;
static linux_ioctl_function_t linux_ioctl_disk;
static linux_ioctl_function_t linux_ioctl_socket;
static linux_ioctl_function_t linux_ioctl_sound;
static linux_ioctl_function_t linux_ioctl_termio;
static linux_ioctl_function_t linux_ioctl_private;
static linux_ioctl_function_t linux_ioctl_special;

static struct linux_ioctl_handler cdrom_handler =
{ linux_ioctl_cdrom, LINUX_IOCTL_CDROM_MIN, LINUX_IOCTL_CDROM_MAX };
static struct linux_ioctl_handler vfat_handler =
{ linux_ioctl_vfat, LINUX_IOCTL_VFAT_MIN, LINUX_IOCTL_VFAT_MAX };
static struct linux_ioctl_handler console_handler =
{ linux_ioctl_console, LINUX_IOCTL_CONSOLE_MIN, LINUX_IOCTL_CONSOLE_MAX };
static struct linux_ioctl_handler disk_handler =
{ linux_ioctl_disk, LINUX_IOCTL_DISK_MIN, LINUX_IOCTL_DISK_MAX };
static struct linux_ioctl_handler socket_handler =
{ linux_ioctl_socket, LINUX_IOCTL_SOCKET_MIN, LINUX_IOCTL_SOCKET_MAX };
static struct linux_ioctl_handler sound_handler =
{ linux_ioctl_sound, LINUX_IOCTL_SOUND_MIN, LINUX_IOCTL_SOUND_MAX };
static struct linux_ioctl_handler termio_handler =
{ linux_ioctl_termio, LINUX_IOCTL_TERMIO_MIN, LINUX_IOCTL_TERMIO_MAX };
static struct linux_ioctl_handler private_handler =
{ linux_ioctl_private, LINUX_IOCTL_PRIVATE_MIN, LINUX_IOCTL_PRIVATE_MAX };

DATA_SET(linux_ioctl_handler_set, cdrom_handler);
DATA_SET(linux_ioctl_handler_set, vfat_handler);
DATA_SET(linux_ioctl_handler_set, console_handler);
DATA_SET(linux_ioctl_handler_set, disk_handler);
DATA_SET(linux_ioctl_handler_set, socket_handler);
DATA_SET(linux_ioctl_handler_set, sound_handler);
DATA_SET(linux_ioctl_handler_set, termio_handler);
DATA_SET(linux_ioctl_handler_set, private_handler);

struct handler_element 
{
	TAILQ_ENTRY(handler_element) list;
	int	(*func)(struct thread *, struct linux_ioctl_args *);
	int	low, high, span;
};

static TAILQ_HEAD(, handler_element) handlers =
	TAILQ_HEAD_INITIALIZER(handlers);

static int
linux_ioctl_disk(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	int error;
	struct disklabel dl;

	if ((error = fget(td, args->fd, &fp)) != 0)
		return (error);
	switch (args->cmd & 0xffff) {
	case LINUX_BLKGETSIZE:
		/* XXX: wrong, should use DIOCGMEDIASIZE/DIOCGSECTORSIZE */
		error = fo_ioctl(fp, DIOCGDINFO, (caddr_t)&dl, td->td_ucred,
		    td);
		fdrop(fp, td);
		if (error)
			return (error);
		return (copyout(&(dl.d_secperunit), (caddr_t)args->arg,
		     sizeof(dl.d_secperunit)));
	}
	fdrop(fp, td);
	return (ENOIOCTL);
}

/*
 * termio related ioctls
 */

struct linux_termio {
	unsigned short c_iflag;
	unsigned short c_oflag;
	unsigned short c_cflag;
	unsigned short c_lflag;
	unsigned char c_line;
	unsigned char c_cc[LINUX_NCC];
};

struct linux_termios {
	unsigned int c_iflag;
	unsigned int c_oflag;
	unsigned int c_cflag;
	unsigned int c_lflag;
#ifdef __alpha__
	unsigned char c_cc[LINUX_NCCS];
	unsigned char c_line;
	unsigned int  c_ispeed;
	unsigned int  c_ospeed;
#else
	unsigned char c_line;
	unsigned char c_cc[LINUX_NCCS];
#endif
};

struct linux_winsize {
	unsigned short ws_row, ws_col;
	unsigned short ws_xpixel, ws_ypixel;
};

static struct speedtab sptab[] = {
	{ B0, LINUX_B0 }, { B50, LINUX_B50 },
	{ B75, LINUX_B75 }, { B110, LINUX_B110 },
	{ B134, LINUX_B134 }, { B150, LINUX_B150 },
	{ B200, LINUX_B200 }, { B300, LINUX_B300 },
	{ B600, LINUX_B600 }, { B1200, LINUX_B1200 },
	{ B1800, LINUX_B1800 }, { B2400, LINUX_B2400 },
	{ B4800, LINUX_B4800 }, { B9600, LINUX_B9600 },
	{ B19200, LINUX_B19200 }, { B38400, LINUX_B38400 },
	{ B57600, LINUX_B57600 }, { B115200, LINUX_B115200 },
	{-1, -1 }
};

struct linux_serial_struct {
	int	type;
	int	line;
	int	port;
	int	irq;
	int	flags;
	int	xmit_fifo_size;
	int	custom_divisor;
	int	baud_base;
	unsigned short close_delay;
	char	reserved_char[2];
	int	hub6;
	unsigned short closing_wait;
	unsigned short closing_wait2;
	int	reserved[4];
};

static int
linux_to_bsd_speed(int code, struct speedtab *table)
{
	for ( ; table->sp_code != -1; table++)
		if (table->sp_code == code)
			return (table->sp_speed);
	return -1;
}

static int
bsd_to_linux_speed(int speed, struct speedtab *table)
{
	for ( ; table->sp_speed != -1; table++)
		if (table->sp_speed == speed)
			return (table->sp_code);
	return -1;
}

static void
bsd_to_linux_termios(struct termios *bios, struct linux_termios *lios)
{
	int i;

#ifdef DEBUG
	if (ldebug(ioctl)) {
		printf("LINUX: BSD termios structure (input):\n");
		printf("i=%08x o=%08x c=%08x l=%08x ispeed=%d ospeed=%d\n",
		    bios->c_iflag, bios->c_oflag, bios->c_cflag, bios->c_lflag,
		    bios->c_ispeed, bios->c_ospeed);
		printf("c_cc ");
		for (i=0; i<NCCS; i++)
			printf("%02x ", bios->c_cc[i]);
		printf("\n");
	}
#endif

	lios->c_iflag = 0;
	if (bios->c_iflag & IGNBRK)
		lios->c_iflag |= LINUX_IGNBRK;
	if (bios->c_iflag & BRKINT)
		lios->c_iflag |= LINUX_BRKINT;
	if (bios->c_iflag & IGNPAR)
		lios->c_iflag |= LINUX_IGNPAR;
	if (bios->c_iflag & PARMRK)
		lios->c_iflag |= LINUX_PARMRK;
	if (bios->c_iflag & INPCK)
		lios->c_iflag |= LINUX_INPCK;
	if (bios->c_iflag & ISTRIP)
		lios->c_iflag |= LINUX_ISTRIP;
	if (bios->c_iflag & INLCR)
		lios->c_iflag |= LINUX_INLCR;
	if (bios->c_iflag & IGNCR)
		lios->c_iflag |= LINUX_IGNCR;
	if (bios->c_iflag & ICRNL)
		lios->c_iflag |= LINUX_ICRNL;
	if (bios->c_iflag & IXON)
		lios->c_iflag |= LINUX_IXON;
	if (bios->c_iflag & IXANY)
		lios->c_iflag |= LINUX_IXANY;
	if (bios->c_iflag & IXOFF)
		lios->c_iflag |= LINUX_IXOFF;
	if (bios->c_iflag & IMAXBEL)
		lios->c_iflag |= LINUX_IMAXBEL;

	lios->c_oflag = 0;
	if (bios->c_oflag & OPOST)
		lios->c_oflag |= LINUX_OPOST;
	if (bios->c_oflag & ONLCR)
		lios->c_oflag |= LINUX_ONLCR;
	if (bios->c_oflag & OXTABS)
		lios->c_oflag |= LINUX_XTABS;

	lios->c_cflag = bsd_to_linux_speed(bios->c_ispeed, sptab);
	lios->c_cflag |= (bios->c_cflag & CSIZE) >> 4;
	if (bios->c_cflag & CSTOPB)
		lios->c_cflag |= LINUX_CSTOPB;
	if (bios->c_cflag & CREAD)
		lios->c_cflag |= LINUX_CREAD;
	if (bios->c_cflag & PARENB)
		lios->c_cflag |= LINUX_PARENB;
	if (bios->c_cflag & PARODD)
		lios->c_cflag |= LINUX_PARODD;
	if (bios->c_cflag & HUPCL)
		lios->c_cflag |= LINUX_HUPCL;
	if (bios->c_cflag & CLOCAL)
		lios->c_cflag |= LINUX_CLOCAL;
	if (bios->c_cflag & CRTSCTS)
		lios->c_cflag |= LINUX_CRTSCTS;

	lios->c_lflag = 0;
	if (bios->c_lflag & ISIG)
		lios->c_lflag |= LINUX_ISIG;
	if (bios->c_lflag & ICANON)
		lios->c_lflag |= LINUX_ICANON;
	if (bios->c_lflag & ECHO)
		lios->c_lflag |= LINUX_ECHO;
	if (bios->c_lflag & ECHOE)
		lios->c_lflag |= LINUX_ECHOE;
	if (bios->c_lflag & ECHOK)
		lios->c_lflag |= LINUX_ECHOK;
	if (bios->c_lflag & ECHONL)
		lios->c_lflag |= LINUX_ECHONL;
	if (bios->c_lflag & NOFLSH)
		lios->c_lflag |= LINUX_NOFLSH;
	if (bios->c_lflag & TOSTOP)
		lios->c_lflag |= LINUX_TOSTOP;
	if (bios->c_lflag & ECHOCTL)
		lios->c_lflag |= LINUX_ECHOCTL;
	if (bios->c_lflag & ECHOPRT)
		lios->c_lflag |= LINUX_ECHOPRT;
	if (bios->c_lflag & ECHOKE)
		lios->c_lflag |= LINUX_ECHOKE;
	if (bios->c_lflag & FLUSHO)
		lios->c_lflag |= LINUX_FLUSHO;
	if (bios->c_lflag & PENDIN)
		lios->c_lflag |= LINUX_PENDIN;
	if (bios->c_lflag & IEXTEN)
		lios->c_lflag |= LINUX_IEXTEN;

	for (i=0; i<LINUX_NCCS; i++)
		lios->c_cc[i] = LINUX_POSIX_VDISABLE;
	lios->c_cc[LINUX_VINTR] = bios->c_cc[VINTR];
	lios->c_cc[LINUX_VQUIT] = bios->c_cc[VQUIT];
	lios->c_cc[LINUX_VERASE] = bios->c_cc[VERASE];
	lios->c_cc[LINUX_VKILL] = bios->c_cc[VKILL];
	lios->c_cc[LINUX_VEOF] = bios->c_cc[VEOF];
	lios->c_cc[LINUX_VEOL] = bios->c_cc[VEOL];
	lios->c_cc[LINUX_VMIN] = bios->c_cc[VMIN];
	lios->c_cc[LINUX_VTIME] = bios->c_cc[VTIME];
	lios->c_cc[LINUX_VEOL2] = bios->c_cc[VEOL2];
	lios->c_cc[LINUX_VSUSP] = bios->c_cc[VSUSP];
	lios->c_cc[LINUX_VSTART] = bios->c_cc[VSTART];
	lios->c_cc[LINUX_VSTOP] = bios->c_cc[VSTOP];
	lios->c_cc[LINUX_VREPRINT] = bios->c_cc[VREPRINT];
	lios->c_cc[LINUX_VDISCARD] = bios->c_cc[VDISCARD];
	lios->c_cc[LINUX_VWERASE] = bios->c_cc[VWERASE];
	lios->c_cc[LINUX_VLNEXT] = bios->c_cc[VLNEXT];

	for (i=0; i<LINUX_NCCS; i++) {
		if (lios->c_cc[i] == _POSIX_VDISABLE)
			lios->c_cc[i] = LINUX_POSIX_VDISABLE;
	}
	lios->c_line = 0;

#ifdef DEBUG
	if (ldebug(ioctl)) {
		printf("LINUX: LINUX termios structure (output):\n");
		printf("i=%08x o=%08x c=%08x l=%08x line=%d\n",
		    lios->c_iflag, lios->c_oflag, lios->c_cflag,
		    lios->c_lflag, (int)lios->c_line);
		printf("c_cc ");
		for (i=0; i<LINUX_NCCS; i++) 
			printf("%02x ", lios->c_cc[i]);
		printf("\n");
	}
#endif
}

static void
linux_to_bsd_termios(struct linux_termios *lios, struct termios *bios)
{
	int i;

#ifdef DEBUG
	if (ldebug(ioctl)) {
		printf("LINUX: LINUX termios structure (input):\n");
		printf("i=%08x o=%08x c=%08x l=%08x line=%d\n", 
		    lios->c_iflag, lios->c_oflag, lios->c_cflag,
		    lios->c_lflag, (int)lios->c_line);
		printf("c_cc ");
		for (i=0; i<LINUX_NCCS; i++)
			printf("%02x ", lios->c_cc[i]);
		printf("\n");
	}
#endif

	bios->c_iflag = 0;
	if (lios->c_iflag & LINUX_IGNBRK)
		bios->c_iflag |= IGNBRK;
	if (lios->c_iflag & LINUX_BRKINT)
		bios->c_iflag |= BRKINT;
	if (lios->c_iflag & LINUX_IGNPAR)
		bios->c_iflag |= IGNPAR;
	if (lios->c_iflag & LINUX_PARMRK)
		bios->c_iflag |= PARMRK;
	if (lios->c_iflag & LINUX_INPCK)
		bios->c_iflag |= INPCK;
	if (lios->c_iflag & LINUX_ISTRIP)
		bios->c_iflag |= ISTRIP;
	if (lios->c_iflag & LINUX_INLCR)
		bios->c_iflag |= INLCR;
	if (lios->c_iflag & LINUX_IGNCR)
		bios->c_iflag |= IGNCR;
	if (lios->c_iflag & LINUX_ICRNL)
		bios->c_iflag |= ICRNL;
	if (lios->c_iflag & LINUX_IXON)
		bios->c_iflag |= IXON;
	if (lios->c_iflag & LINUX_IXANY)
		bios->c_iflag |= IXANY;
	if (lios->c_iflag & LINUX_IXOFF)
		bios->c_iflag |= IXOFF;
	if (lios->c_iflag & LINUX_IMAXBEL)
		bios->c_iflag |= IMAXBEL;

	bios->c_oflag = 0;
	if (lios->c_oflag & LINUX_OPOST)
		bios->c_oflag |= OPOST;
	if (lios->c_oflag & LINUX_ONLCR)
		bios->c_oflag |= ONLCR;
	if (lios->c_oflag & LINUX_XTABS)
		bios->c_oflag |= OXTABS;

	bios->c_cflag = (lios->c_cflag & LINUX_CSIZE) << 4;
	if (lios->c_cflag & LINUX_CSTOPB)
		bios->c_cflag |= CSTOPB;
	if (lios->c_cflag & LINUX_CREAD)
		bios->c_cflag |= CREAD;
	if (lios->c_cflag & LINUX_PARENB)
		bios->c_cflag |= PARENB;
	if (lios->c_cflag & LINUX_PARODD)
		bios->c_cflag |= PARODD;
	if (lios->c_cflag & LINUX_HUPCL)
		bios->c_cflag |= HUPCL;
	if (lios->c_cflag & LINUX_CLOCAL)
		bios->c_cflag |= CLOCAL;
	if (lios->c_cflag & LINUX_CRTSCTS)
		bios->c_cflag |= CRTSCTS;

	bios->c_lflag = 0;
	if (lios->c_lflag & LINUX_ISIG)
		bios->c_lflag |= ISIG;
	if (lios->c_lflag & LINUX_ICANON)
		bios->c_lflag |= ICANON;
	if (lios->c_lflag & LINUX_ECHO)
		bios->c_lflag |= ECHO;
	if (lios->c_lflag & LINUX_ECHOE)
		bios->c_lflag |= ECHOE;
	if (lios->c_lflag & LINUX_ECHOK)
		bios->c_lflag |= ECHOK;
	if (lios->c_lflag & LINUX_ECHONL)
		bios->c_lflag |= ECHONL;
	if (lios->c_lflag & LINUX_NOFLSH)
		bios->c_lflag |= NOFLSH;
	if (lios->c_lflag & LINUX_TOSTOP)
		bios->c_lflag |= TOSTOP;
	if (lios->c_lflag & LINUX_ECHOCTL)
		bios->c_lflag |= ECHOCTL;
	if (lios->c_lflag & LINUX_ECHOPRT)
		bios->c_lflag |= ECHOPRT;
	if (lios->c_lflag & LINUX_ECHOKE)
		bios->c_lflag |= ECHOKE;
	if (lios->c_lflag & LINUX_FLUSHO)
		bios->c_lflag |= FLUSHO;
	if (lios->c_lflag & LINUX_PENDIN)
		bios->c_lflag |= PENDIN;
	if (lios->c_lflag & LINUX_IEXTEN)
		bios->c_lflag |= IEXTEN;

	for (i=0; i<NCCS; i++)
		bios->c_cc[i] = _POSIX_VDISABLE;
	bios->c_cc[VINTR] = lios->c_cc[LINUX_VINTR];
	bios->c_cc[VQUIT] = lios->c_cc[LINUX_VQUIT];
	bios->c_cc[VERASE] = lios->c_cc[LINUX_VERASE];
	bios->c_cc[VKILL] = lios->c_cc[LINUX_VKILL];
	bios->c_cc[VEOF] = lios->c_cc[LINUX_VEOF];
	bios->c_cc[VEOL] = lios->c_cc[LINUX_VEOL];
	bios->c_cc[VMIN] = lios->c_cc[LINUX_VMIN];
	bios->c_cc[VTIME] = lios->c_cc[LINUX_VTIME];
	bios->c_cc[VEOL2] = lios->c_cc[LINUX_VEOL2];
	bios->c_cc[VSUSP] = lios->c_cc[LINUX_VSUSP];
	bios->c_cc[VSTART] = lios->c_cc[LINUX_VSTART];
	bios->c_cc[VSTOP] = lios->c_cc[LINUX_VSTOP];
	bios->c_cc[VREPRINT] = lios->c_cc[LINUX_VREPRINT];
	bios->c_cc[VDISCARD] = lios->c_cc[LINUX_VDISCARD];
	bios->c_cc[VWERASE] = lios->c_cc[LINUX_VWERASE];
	bios->c_cc[VLNEXT] = lios->c_cc[LINUX_VLNEXT];

	for (i=0; i<NCCS; i++) {
		if (bios->c_cc[i] == LINUX_POSIX_VDISABLE)
			bios->c_cc[i] = _POSIX_VDISABLE;
	}

	bios->c_ispeed = bios->c_ospeed =
	    linux_to_bsd_speed(lios->c_cflag & LINUX_CBAUD, sptab);

#ifdef DEBUG
	if (ldebug(ioctl)) {
		printf("LINUX: BSD termios structure (output):\n");
		printf("i=%08x o=%08x c=%08x l=%08x ispeed=%d ospeed=%d\n",
		    bios->c_iflag, bios->c_oflag, bios->c_cflag, bios->c_lflag,
		    bios->c_ispeed, bios->c_ospeed);
		printf("c_cc ");
		for (i=0; i<NCCS; i++) 
			printf("%02x ", bios->c_cc[i]);
		printf("\n");
	}
#endif
}

static void
bsd_to_linux_termio(struct termios *bios, struct linux_termio *lio)
{
	struct linux_termios lios;

	bsd_to_linux_termios(bios, &lios);
	lio->c_iflag = lios.c_iflag;
	lio->c_oflag = lios.c_oflag;
	lio->c_cflag = lios.c_cflag;
	lio->c_lflag = lios.c_lflag;
	lio->c_line  = lios.c_line;
#ifdef __alpha__
	lio->c_cc[LINUX__VINTR] = lios.c_cc[LINUX_VINTR];
	lio->c_cc[LINUX__VQUIT] = lios.c_cc[LINUX_VQUIT];
	lio->c_cc[LINUX__VERASE] = lios.c_cc[LINUX_VERASE];
	lio->c_cc[LINUX__VKILL] = lios.c_cc[LINUX_VKILL];
	lio->c_cc[LINUX__VEOF] =
	    lios.c_cc[(lios.c_lflag & ICANON) ? LINUX_VEOF : LINUX_VMIN];
	lio->c_cc[LINUX__VEOL] =
	    lios.c_cc[(lios.c_lflag & ICANON) ? LINUX_VEOL : LINUX_VTIME];
	lio->c_cc[LINUX__VEOL2] = lios.c_cc[LINUX_VEOL2];
	lio->c_cc[LINUX__VSWTC] = lios.c_cc[LINUX_VSWTC];
#else
	memcpy(lio->c_cc, lios.c_cc, LINUX_NCC);
#endif
}

static void
linux_to_bsd_termio(struct linux_termio *lio, struct termios *bios)
{
	struct linux_termios lios;
	int i;

	lios.c_iflag = lio->c_iflag;
	lios.c_oflag = lio->c_oflag;
	lios.c_cflag = lio->c_cflag;
	lios.c_lflag = lio->c_lflag;
#ifdef __alpha__
	for (i=0; i<LINUX_NCCS; i++)
		lios.c_cc[i] = LINUX_POSIX_VDISABLE;
	lios.c_cc[LINUX_VINTR] = lio->c_cc[LINUX__VINTR];
	lios.c_cc[LINUX_VQUIT] = lio->c_cc[LINUX__VQUIT];
	lios.c_cc[LINUX_VERASE] = lio->c_cc[LINUX__VERASE];
	lios.c_cc[LINUX_VKILL] = lio->c_cc[LINUX__VKILL];
	lios.c_cc[LINUX_VEOL2] = lio->c_cc[LINUX__VEOL2];
	lios.c_cc[LINUX_VSWTC] = lio->c_cc[LINUX__VSWTC];
	lios.c_cc[(lio->c_lflag & ICANON) ? LINUX_VEOF : LINUX_VMIN] =
	    lio->c_cc[LINUX__VEOF];
	lios.c_cc[(lio->c_lflag & ICANON) ? LINUX_VEOL : LINUX_VTIME] =
	    lio->c_cc[LINUX__VEOL];
#else
	for (i=LINUX_NCC; i<LINUX_NCCS; i++)
		lios.c_cc[i] = LINUX_POSIX_VDISABLE;
	memcpy(lios.c_cc, lio->c_cc, LINUX_NCC);
#endif
	linux_to_bsd_termios(&lios, bios);
}

static int
linux_ioctl_termio(struct thread *td, struct linux_ioctl_args *args)
{
	struct termios bios;
	struct linux_termios lios;
	struct linux_termio lio;
	struct file *fp;
	int error;

	if ((error = fget(td, args->fd, &fp)) != 0)
		return (error);

	switch (args->cmd & 0xffff) {

	case LINUX_TCGETS:
		error = fo_ioctl(fp, TIOCGETA, (caddr_t)&bios, td->td_ucred,
		    td);
		if (error)
			break;
		bsd_to_linux_termios(&bios, &lios);
		error = copyout(&lios, (caddr_t)args->arg, sizeof(lios));
		break;

	case LINUX_TCSETS:
		error = copyin((caddr_t)args->arg, &lios, sizeof(lios));
		if (error)
			break;
		linux_to_bsd_termios(&lios, &bios);
		error = (fo_ioctl(fp, TIOCSETA, (caddr_t)&bios, td->td_ucred,
		    td));
		break;

	case LINUX_TCSETSW:
		error = copyin((caddr_t)args->arg, &lios, sizeof(lios));
		if (error)
			break;
		linux_to_bsd_termios(&lios, &bios);
		error = (fo_ioctl(fp, TIOCSETAW, (caddr_t)&bios, td->td_ucred,
		    td));
		break;

	case LINUX_TCSETSF:
		error = copyin((caddr_t)args->arg, &lios, sizeof(lios));
		if (error)
			break;
		linux_to_bsd_termios(&lios, &bios);
		error = (fo_ioctl(fp, TIOCSETAF, (caddr_t)&bios, td->td_ucred,
		    td));
		break;

	case LINUX_TCGETA:
		error = fo_ioctl(fp, TIOCGETA, (caddr_t)&bios, td->td_ucred,
		    td);
		if (error)
			break;
		bsd_to_linux_termio(&bios, &lio);
		error = (copyout(&lio, (caddr_t)args->arg, sizeof(lio)));
		break;

	case LINUX_TCSETA:
		error = copyin((caddr_t)args->arg, &lio, sizeof(lio));
		if (error)
			break;
		linux_to_bsd_termio(&lio, &bios);
		error = (fo_ioctl(fp, TIOCSETA, (caddr_t)&bios, td->td_ucred,
		    td));
		break;

	case LINUX_TCSETAW:
		error = copyin((caddr_t)args->arg, &lio, sizeof(lio));
		if (error)
			break;
		linux_to_bsd_termio(&lio, &bios);
		error = (fo_ioctl(fp, TIOCSETAW, (caddr_t)&bios, td->td_ucred,
		    td));
		break;

	case LINUX_TCSETAF:
		error = copyin((caddr_t)args->arg, &lio, sizeof(lio));
		if (error)
			break;
		linux_to_bsd_termio(&lio, &bios);
		error = (fo_ioctl(fp, TIOCSETAF, (caddr_t)&bios, td->td_ucred,
		    td));
		break;

	/* LINUX_TCSBRK */

	case LINUX_TCXONC: {
		switch (args->arg) {
		case LINUX_TCOOFF:
			args->cmd = TIOCSTOP;
			break;
		case LINUX_TCOON:
			args->cmd = TIOCSTART;
			break;
		case LINUX_TCIOFF:
		case LINUX_TCION: {
			int c;
			struct write_args wr;
			error = fo_ioctl(fp, TIOCGETA, (caddr_t)&bios,
			    td->td_ucred, td);
			if (error)
				break;
			fdrop(fp, td);
			c = (args->arg == LINUX_TCIOFF) ? VSTOP : VSTART;
			c = bios.c_cc[c];
			if (c != _POSIX_VDISABLE) {
				wr.fd = args->fd;
				wr.buf = &c;
				wr.nbyte = sizeof(c);
				return (write(td, &wr));
			} else
				return (0);
		}
		default:
			fdrop(fp, td);
			return (EINVAL);
		}
		args->arg = 0;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;
	}

	case LINUX_TCFLSH: {
		args->cmd = TIOCFLUSH;
		switch (args->arg) {
		case LINUX_TCIFLUSH:
			args->arg = FREAD;
			break;
		case LINUX_TCOFLUSH:
			args->arg = FWRITE;
			break;
		case LINUX_TCIOFLUSH:
			args->arg = FREAD | FWRITE;
			break;
		default:
			fdrop(fp, td);
			return (EINVAL);
		}
		error = (ioctl(td, (struct ioctl_args *)args));
		break;
	}

	case LINUX_TIOCEXCL:
		args->cmd = TIOCEXCL;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCNXCL:
		args->cmd = TIOCNXCL;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCSCTTY:
		args->cmd = TIOCSCTTY;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCGPGRP:
		args->cmd = TIOCGPGRP;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCSPGRP:
		args->cmd = TIOCSPGRP;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	/* LINUX_TIOCOUTQ */
	/* LINUX_TIOCSTI */

	case LINUX_TIOCGWINSZ:
		args->cmd = TIOCGWINSZ;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCSWINSZ:
		args->cmd = TIOCSWINSZ;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCMGET:
		args->cmd = TIOCMGET;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCMBIS:
		args->cmd = TIOCMBIS;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCMBIC:
		args->cmd = TIOCMBIC;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCMSET:
		args->cmd = TIOCMSET;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	/* TIOCGSOFTCAR */
	/* TIOCSSOFTCAR */

	case LINUX_FIONREAD: /* LINUX_TIOCINQ */
		args->cmd = FIONREAD;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	/* LINUX_TIOCLINUX */

	case LINUX_TIOCCONS:
		args->cmd = TIOCCONS;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCGSERIAL: {
		struct linux_serial_struct lss;
		lss.type = LINUX_PORT_16550A;
		lss.flags = 0;
		lss.close_delay = 0;
		error = copyout(&lss, (caddr_t)args->arg, sizeof(lss));
		break;
	}

	case LINUX_TIOCSSERIAL: {
		struct linux_serial_struct lss;
		error = copyin((caddr_t)args->arg, &lss, sizeof(lss));
		if (error)
			break;
		/* XXX - It really helps to have an implementation that
		 * does nothing. NOT!
		 */
		error = 0;
		break;
	}

	/* LINUX_TIOCPKT */

	case LINUX_FIONBIO:
		args->cmd = FIONBIO;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCNOTTY:
		args->cmd = TIOCNOTTY;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_TIOCSETD: {
		int line;
		switch (args->arg) {
		case LINUX_N_TTY:
			line = TTYDISC;
			break;
		case LINUX_N_SLIP:
			line = SLIPDISC;
			break;
		case LINUX_N_PPP:
			line = PPPDISC;
			break;
		default:
			fdrop(fp, td);
			return (EINVAL);
		}
		error = (fo_ioctl(fp, TIOCSETD, (caddr_t)&line, td->td_ucred,
		    td));
		break;
	}

	case LINUX_TIOCGETD: {
		int linux_line;
		int bsd_line = TTYDISC;
		error = fo_ioctl(fp, TIOCGETD, (caddr_t)&bsd_line,
		    td->td_ucred, td);
		if (error)
			return (error);
		switch (bsd_line) {
		case TTYDISC:
			linux_line = LINUX_N_TTY;
			break;
		case SLIPDISC:
			linux_line = LINUX_N_SLIP;
			break;
		case PPPDISC:
			linux_line = LINUX_N_PPP;
			break;
		default:
			fdrop(fp, td);
			return (EINVAL);
		}
		error = (copyout(&linux_line, (caddr_t)args->arg, sizeof(int)));
		break;
	}

	/* LINUX_TCSBRKP */
	/* LINUX_TIOCTTYGSTRUCT */

	case LINUX_FIONCLEX:
		args->cmd = FIONCLEX;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_FIOCLEX:
		args->cmd = FIOCLEX;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_FIOASYNC:
		args->cmd = FIOASYNC;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	/* LINUX_TIOCSERCONFIG */
	/* LINUX_TIOCSERGWILD */
	/* LINUX_TIOCSERSWILD */
	/* LINUX_TIOCGLCKTRMIOS */
	/* LINUX_TIOCSLCKTRMIOS */

	default:
		error = ENOIOCTL;
		break;
	}

	fdrop(fp, td);
	return (error);
}

/*
 * CDROM related ioctls
 */

struct linux_cdrom_msf
{
	u_char	cdmsf_min0;
	u_char	cdmsf_sec0;
	u_char	cdmsf_frame0;
	u_char	cdmsf_min1;
	u_char	cdmsf_sec1;
	u_char	cdmsf_frame1;
};

struct linux_cdrom_tochdr
{
	u_char	cdth_trk0;
	u_char	cdth_trk1;
};

union linux_cdrom_addr
{
	struct {
		u_char	minute;
		u_char	second;
		u_char	frame;
	} msf;
	int	lba;
};

struct linux_cdrom_tocentry
{
	u_char	cdte_track;     
	u_char	cdte_adr:4;
	u_char	cdte_ctrl:4;
	u_char	cdte_format;    
	union linux_cdrom_addr cdte_addr;
	u_char	cdte_datamode;  
};

struct linux_cdrom_subchnl
{
	u_char	cdsc_format;
	u_char	cdsc_audiostatus;
	u_char	cdsc_adr:4;
	u_char	cdsc_ctrl:4;
	u_char	cdsc_trk;
	u_char	cdsc_ind;
	union linux_cdrom_addr cdsc_absaddr;
	union linux_cdrom_addr cdsc_reladdr;
};

struct l_cdrom_read_audio {
	union linux_cdrom_addr addr;
	u_char		addr_format;
	l_int		nframes;
	u_char		*buf;
};

struct l_dvd_layer {
	u_char		book_version:4;
	u_char		book_type:4;
	u_char		min_rate:4;
	u_char		disc_size:4;
	u_char		layer_type:4;
	u_char		track_path:1;
	u_char		nlayers:2;
	u_char		track_density:4;
	u_char		linear_density:4;
	u_char		bca:1;
	u_int32_t	start_sector;
	u_int32_t	end_sector;
	u_int32_t	end_sector_l0;
};

struct l_dvd_physical {
	u_char		type;
	u_char		layer_num;
	struct l_dvd_layer layer[4];
};

struct l_dvd_copyright {
	u_char		type;
	u_char		layer_num;
	u_char		cpst;
	u_char		rmi;
};

struct l_dvd_disckey {
	u_char		type;
	l_uint		agid:2;
	u_char		value[2048];
};

struct l_dvd_bca {
	u_char		type;
	l_int		len;
	u_char		value[188];
};

struct l_dvd_manufact {
	u_char		type;
	u_char		layer_num;
	l_int		len;
	u_char		value[2048];
};

typedef union {
	u_char			type;
	struct l_dvd_physical	physical;
	struct l_dvd_copyright	copyright;
	struct l_dvd_disckey	disckey;
	struct l_dvd_bca	bca;
	struct l_dvd_manufact	manufact;
} l_dvd_struct;

typedef u_char l_dvd_key[5];
typedef u_char l_dvd_challenge[10];

struct l_dvd_lu_send_agid {
	u_char		type;
	l_uint		agid:2;
};

struct l_dvd_host_send_challenge {
	u_char		type;
	l_uint		agid:2;
	l_dvd_challenge	chal;
};

struct l_dvd_send_key {
	u_char		type;
	l_uint		agid:2;
	l_dvd_key	key;
};

struct l_dvd_lu_send_challenge {
	u_char		type;
	l_uint		agid:2;
	l_dvd_challenge	chal;
};

struct l_dvd_lu_send_title_key {
	u_char		type;
	l_uint		agid:2;
	l_dvd_key	title_key;
	l_int		lba;
	l_uint		cpm:1;
	l_uint		cp_sec:1;
	l_uint		cgms:2;
};

struct l_dvd_lu_send_asf {
	u_char		type;
	l_uint		agid:2;
	l_uint		asf:1;
};

struct l_dvd_host_send_rpcstate {
	u_char		type;
	u_char		pdrc;
};

struct l_dvd_lu_send_rpcstate {
	u_char		type:2;
	u_char		vra:3;
	u_char		ucca:3;
	u_char		region_mask;
	u_char		rpc_scheme;
};

typedef union {
	u_char				type;
	struct l_dvd_lu_send_agid	lsa;
	struct l_dvd_host_send_challenge hsc;
	struct l_dvd_send_key		lsk;
	struct l_dvd_lu_send_challenge	lsc;
	struct l_dvd_send_key		hsk;
	struct l_dvd_lu_send_title_key	lstk;
	struct l_dvd_lu_send_asf	lsasf;
	struct l_dvd_host_send_rpcstate	hrpcs;
	struct l_dvd_lu_send_rpcstate	lrpcs;
} l_dvd_authinfo;

static void
bsd_to_linux_msf_lba(u_char af, union msf_lba *bp, union linux_cdrom_addr *lp)
{
	if (af == CD_LBA_FORMAT)
		lp->lba = bp->lba;
	else {
		lp->msf.minute = bp->msf.minute;
		lp->msf.second = bp->msf.second;
		lp->msf.frame = bp->msf.frame;
	}
}

static void
linux_to_bsd_msf_lba(u_char af, union linux_cdrom_addr *lp, union msf_lba *bp)
{
	if (af == CD_LBA_FORMAT)
		bp->lba = lp->lba;
	else {
		bp->msf.minute = lp->msf.minute;
		bp->msf.second = lp->msf.second;
		bp->msf.frame = lp->msf.frame;
	}
}

static void
set_linux_cdrom_addr(union linux_cdrom_addr *addr, int format, int lba)
{
	if (format == LINUX_CDROM_MSF) {
		addr->msf.frame = lba % 75;
		lba /= 75;
		lba += 2;
		addr->msf.second = lba % 60;
		addr->msf.minute = lba / 60;
	} else
		addr->lba = lba;
}

static int
linux_to_bsd_dvd_struct(l_dvd_struct *lp, struct dvd_struct *bp)
{
	bp->format = lp->type;
	switch (bp->format) {
	case DVD_STRUCT_PHYSICAL:
		if (bp->layer_num >= 4)
			return (EINVAL);
		bp->layer_num = lp->physical.layer_num;
		break;
	case DVD_STRUCT_COPYRIGHT:
		bp->layer_num = lp->copyright.layer_num;
		break;
	case DVD_STRUCT_DISCKEY:
		bp->agid = lp->disckey.agid;
		break;
	case DVD_STRUCT_BCA:
	case DVD_STRUCT_MANUFACT:
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
bsd_to_linux_dvd_struct(struct dvd_struct *bp, l_dvd_struct *lp)
{
	switch (bp->format) {
	case DVD_STRUCT_PHYSICAL: {
		struct dvd_layer *blp = (struct dvd_layer *)bp->data;
		struct l_dvd_layer *llp = &lp->physical.layer[bp->layer_num];
		memset(llp, 0, sizeof(*llp));
		llp->book_version = blp->book_version;
		llp->book_type = blp->book_type;
		llp->min_rate = blp->max_rate;
		llp->disc_size = blp->disc_size;
		llp->layer_type = blp->layer_type;
		llp->track_path = blp->track_path;
		llp->nlayers = blp->nlayers;
		llp->track_density = blp->track_density;
		llp->linear_density = blp->linear_density;
		llp->bca = blp->bca;
		llp->start_sector = blp->start_sector;
		llp->end_sector = blp->end_sector;
		llp->end_sector_l0 = blp->end_sector_l0;
		break;
	}
	case DVD_STRUCT_COPYRIGHT:
		lp->copyright.cpst = bp->cpst;
		lp->copyright.rmi = bp->rmi;
		break;
	case DVD_STRUCT_DISCKEY:
		memcpy(lp->disckey.value, bp->data, sizeof(lp->disckey.value));
		break;
	case DVD_STRUCT_BCA:
		lp->bca.len = bp->length;
		memcpy(lp->bca.value, bp->data, sizeof(lp->bca.value));
		break;
	case DVD_STRUCT_MANUFACT:
		lp->manufact.len = bp->length;
		memcpy(lp->manufact.value, bp->data,
		    sizeof(lp->manufact.value));
		/* lp->manufact.layer_num is unused in linux (redhat 7.0) */
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
linux_to_bsd_dvd_authinfo(l_dvd_authinfo *lp, int *bcode,
    struct dvd_authinfo *bp)
{
	switch (lp->type) {
	case LINUX_DVD_LU_SEND_AGID:
		*bcode = DVDIOCREPORTKEY;
		bp->format = DVD_REPORT_AGID;
		bp->agid = lp->lsa.agid;
		break;
	case LINUX_DVD_HOST_SEND_CHALLENGE:
		*bcode = DVDIOCSENDKEY;
		bp->format = DVD_SEND_CHALLENGE;
		bp->agid = lp->hsc.agid;
		memcpy(bp->keychal, lp->hsc.chal, 10);
		break;
	case LINUX_DVD_LU_SEND_KEY1:
		*bcode = DVDIOCREPORTKEY;
		bp->format = DVD_REPORT_KEY1;
		bp->agid = lp->lsk.agid;
		break;
	case LINUX_DVD_LU_SEND_CHALLENGE:
		*bcode = DVDIOCREPORTKEY;
		bp->format = DVD_REPORT_CHALLENGE;
		bp->agid = lp->lsc.agid;
		break;
	case LINUX_DVD_HOST_SEND_KEY2:
		*bcode = DVDIOCSENDKEY;
		bp->format = DVD_SEND_KEY2;
		bp->agid = lp->hsk.agid;
		memcpy(bp->keychal, lp->hsk.key, 5);
		break;
	case LINUX_DVD_LU_SEND_TITLE_KEY:
		*bcode = DVDIOCREPORTKEY;
		bp->format = DVD_REPORT_TITLE_KEY;
		bp->agid = lp->lstk.agid;
		bp->lba = lp->lstk.lba;
		break;
	case LINUX_DVD_LU_SEND_ASF:
		*bcode = DVDIOCREPORTKEY;
		bp->format = DVD_REPORT_ASF;
		bp->agid = lp->lsasf.agid;
		break;
	case LINUX_DVD_INVALIDATE_AGID:
		*bcode = DVDIOCREPORTKEY;
		bp->format = DVD_INVALIDATE_AGID;
		bp->agid = lp->lsa.agid;
		break;
	case LINUX_DVD_LU_SEND_RPC_STATE:
		*bcode = DVDIOCREPORTKEY;
		bp->format = DVD_REPORT_RPC;
		break;
	case LINUX_DVD_HOST_SEND_RPC_STATE:
		*bcode = DVDIOCSENDKEY;
		bp->format = DVD_SEND_RPC;
		bp->region = lp->hrpcs.pdrc;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
bsd_to_linux_dvd_authinfo(struct dvd_authinfo *bp, l_dvd_authinfo *lp)
{
	switch (lp->type) {
	case LINUX_DVD_LU_SEND_AGID:
		lp->lsa.agid = bp->agid;
		break;
	case LINUX_DVD_HOST_SEND_CHALLENGE:
		lp->type = LINUX_DVD_LU_SEND_KEY1;
		break;
	case LINUX_DVD_LU_SEND_KEY1:
		memcpy(lp->lsk.key, bp->keychal, sizeof(lp->lsk.key));
		break;
	case LINUX_DVD_LU_SEND_CHALLENGE:
		memcpy(lp->lsc.chal, bp->keychal, sizeof(lp->lsc.chal));
		break;
	case LINUX_DVD_HOST_SEND_KEY2:
		lp->type = LINUX_DVD_AUTH_ESTABLISHED;
		break;
	case LINUX_DVD_LU_SEND_TITLE_KEY:
		memcpy(lp->lstk.title_key, bp->keychal,
		    sizeof(lp->lstk.title_key));
		lp->lstk.cpm = bp->cpm;
		lp->lstk.cp_sec = bp->cp_sec;
		lp->lstk.cgms = bp->cgms;
		break;
	case LINUX_DVD_LU_SEND_ASF:
		lp->lsasf.asf = bp->asf;
		break;
	case LINUX_DVD_INVALIDATE_AGID:
		break;
	case LINUX_DVD_LU_SEND_RPC_STATE:
		lp->lrpcs.type = bp->reg_type;
		lp->lrpcs.vra = bp->vend_rsts;
		lp->lrpcs.ucca = bp->user_rsts;
		lp->lrpcs.region_mask = bp->region;
		lp->lrpcs.rpc_scheme = bp->rpc_scheme;
		break;
	case LINUX_DVD_HOST_SEND_RPC_STATE:
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
linux_ioctl_cdrom(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	int error;

	if ((error = fget(td, args->fd, &fp)) != 0)
		return (error);
	switch (args->cmd & 0xffff) {

	case LINUX_CDROMPAUSE:
		args->cmd = CDIOCPAUSE;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_CDROMRESUME:
		args->cmd = CDIOCRESUME;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_CDROMPLAYMSF:
		args->cmd = CDIOCPLAYMSF;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_CDROMPLAYTRKIND:
		args->cmd = CDIOCPLAYTRACKS;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_CDROMREADTOCHDR: {
		struct ioc_toc_header th;
		struct linux_cdrom_tochdr lth;
		error = fo_ioctl(fp, CDIOREADTOCHEADER, (caddr_t)&th,
		    td->td_ucred, td);
		if (!error) {
			lth.cdth_trk0 = th.starting_track;
			lth.cdth_trk1 = th.ending_track;
			copyout(&lth, (caddr_t)args->arg, sizeof(lth));
		}
		break;
	}

	case LINUX_CDROMREADTOCENTRY: {
		struct linux_cdrom_tocentry lte, *ltep =
		    (struct linux_cdrom_tocentry *)args->arg;
		struct ioc_read_toc_single_entry irtse;
		irtse.address_format = ltep->cdte_format;
		irtse.track = ltep->cdte_track;
		error = fo_ioctl(fp, CDIOREADTOCENTRY, (caddr_t)&irtse,
		    td->td_ucred, td);
		if (!error) {
			lte = *ltep;
			lte.cdte_ctrl = irtse.entry.control;
			lte.cdte_adr = irtse.entry.addr_type;
			bsd_to_linux_msf_lba(irtse.address_format,
			    &irtse.entry.addr, &lte.cdte_addr);
			copyout(&lte, (caddr_t)args->arg, sizeof(lte));
		}
		break;
	}

	case LINUX_CDROMSTOP:
		args->cmd = CDIOCSTOP;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_CDROMSTART:
		args->cmd = CDIOCSTART;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_CDROMEJECT:
		args->cmd = CDIOCEJECT;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	/* LINUX_CDROMVOLCTRL */

	case LINUX_CDROMSUBCHNL: {
		struct linux_cdrom_subchnl sc;
		struct ioc_read_subchannel bsdsc;
		struct cd_sub_channel_info *bsdinfo;
		caddr_t sg = stackgap_init();
		bsdinfo = (struct cd_sub_channel_info*)stackgap_alloc(&sg,
		    sizeof(struct cd_sub_channel_info));
		bsdsc.address_format = CD_LBA_FORMAT;
		bsdsc.data_format = CD_CURRENT_POSITION;
		bsdsc.track = 0;
		bsdsc.data_len = sizeof(struct cd_sub_channel_info);
		bsdsc.data = bsdinfo;
		error = fo_ioctl(fp, CDIOCREADSUBCHANNEL, (caddr_t)&bsdsc,
		    td->td_ucred, td);
		if (error)
			break;
		error = copyin((caddr_t)args->arg, &sc,
		    sizeof(struct linux_cdrom_subchnl));
		if (error)
			break;
		sc.cdsc_audiostatus = bsdinfo->header.audio_status;
		sc.cdsc_adr = bsdinfo->what.position.addr_type;
		sc.cdsc_ctrl = bsdinfo->what.position.control;
		sc.cdsc_trk = bsdinfo->what.position.track_number;
		sc.cdsc_ind = bsdinfo->what.position.index_number;
		set_linux_cdrom_addr(&sc.cdsc_absaddr, sc.cdsc_format,
		    bsdinfo->what.position.absaddr.lba);
		set_linux_cdrom_addr(&sc.cdsc_reladdr, sc.cdsc_format,
		    bsdinfo->what.position.reladdr.lba);
		error = copyout(&sc, (caddr_t)args->arg,
		    sizeof(struct linux_cdrom_subchnl));
		break;
	}

	/* LINUX_CDROMREADMODE2 */
	/* LINUX_CDROMREADMODE1 */

	case LINUX_CDROMREADAUDIO: {
		struct l_cdrom_read_audio lra;
		struct ioc_read_audio bra;

		error = copyin((caddr_t)args->arg, &lra, sizeof(lra));
		if (error)
			break;
		bra.address_format = lra.addr_format;
		linux_to_bsd_msf_lba(bra.address_format, &lra.addr,
		    &bra.address);
		bra.nframes = lra.nframes;
		bra.buffer = lra.buf;
		error = fo_ioctl(fp, CDIOCREADAUDIO, (caddr_t)&bra,
		    td->td_ucred, td);
		break;
	}

	/* LINUX_CDROMEJECT_SW */
	/* LINUX_CDROMMULTISESSION */
	/* LINUX_CDROM_GET_UPC */

	case LINUX_CDROMRESET:
		args->cmd = CDIOCRESET;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	/* LINUX_CDROMVOLREAD */
	/* LINUX_CDROMREADRAW */
	/* LINUX_CDROMREADCOOKED */
	/* LINUX_CDROMSEEK */
	/* LINUX_CDROMPLAYBLK */
	/* LINUX_CDROMREADALL */
	/* LINUX_CDROMCLOSETRAY */
	/* LINUX_CDROMLOADFROMSLOT */
	/* LINUX_CDROMGETSPINDOWN */
	/* LINUX_CDROMSETSPINDOWN */
	/* LINUX_CDROM_SET_OPTIONS */
	/* LINUX_CDROM_CLEAR_OPTIONS */
	/* LINUX_CDROM_SELECT_SPEED */
	/* LINUX_CDROM_SELECT_DISC */
	/* LINUX_CDROM_MEDIA_CHANGED */
	/* LINUX_CDROM_DRIVE_STATUS */
	/* LINUX_CDROM_DISC_STATUS */
	/* LINUX_CDROM_CHANGER_NSLOTS */
	/* LINUX_CDROM_LOCKDOOR */
	/* LINUX_CDROM_DEBUG */
	/* LINUX_CDROM_GET_CAPABILITY */
	/* LINUX_CDROMAUDIOBUFSIZ */

	case LINUX_DVD_READ_STRUCT: {
		l_dvd_struct lds;
		struct dvd_struct bds;

		error = copyin((caddr_t)args->arg, &lds, sizeof(l_dvd_struct));
		if (error)
			break;
		error = linux_to_bsd_dvd_struct(&lds, &bds);
		if (error)
			break;
		error = fo_ioctl(fp, DVDIOCREADSTRUCTURE, (caddr_t)&bds,
		    td->td_ucred, td);
		if (error)
			break;
		error = bsd_to_linux_dvd_struct(&bds, &lds);
		if (error)
			break;
		error = copyout(&lds, (caddr_t)args->arg,
				sizeof(l_dvd_struct));
		break;
	}

	/* LINUX_DVD_WRITE_STRUCT */

	case LINUX_DVD_AUTH: {
		l_dvd_authinfo lda;
		struct dvd_authinfo bda;
		int bcode;

		error = copyin((caddr_t)args->arg, &lda,
		    sizeof(l_dvd_authinfo));
		if (error)
			break;
		error = linux_to_bsd_dvd_authinfo(&lda, &bcode, &bda);
		if (error)
			break;
		error = fo_ioctl(fp, bcode, (caddr_t)&bda, td->td_ucred,
		    td);
		if (error) {
			if (lda.type == LINUX_DVD_HOST_SEND_KEY2) {
				lda.type = LINUX_DVD_AUTH_FAILURE;
				copyout(&lda, (caddr_t)args->arg,
				    sizeof(l_dvd_authinfo));
			}
			break;
		}
		error = bsd_to_linux_dvd_authinfo(&bda, &lda);
		if (error)
			break;
		error = copyout(&lda, (caddr_t)args->arg,
				sizeof(l_dvd_authinfo));
		break;
	}

	/* LINUX_CDROM_SEND_PACKET */
	/* LINUX_CDROM_NEXT_WRITABLE */
	/* LINUX_CDROM_LAST_WRITTEN */

	default:
		error = ENOIOCTL;
		break;
	}

	fdrop(fp, td);
	return (error);
}

static int
linux_ioctl_vfat(struct thread *td, struct linux_ioctl_args *args)
{

	return (ENOTTY);
}

/*
 * Sound related ioctls
 */

static u_int32_t dirbits[4] = { IOC_VOID, IOC_IN, IOC_OUT, IOC_INOUT };

#define	SETDIR(c)	(((c) & ~IOC_DIRMASK) | dirbits[args->cmd >> 30])

static int
linux_ioctl_sound(struct thread *td, struct linux_ioctl_args *args)
{

	switch (args->cmd & 0xffff) {

	case LINUX_SOUND_MIXER_WRITE_VOLUME:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_VOLUME);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_BASS:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_BASS);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_TREBLE:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_TREBLE);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_SYNTH:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_SYNTH);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_PCM:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_PCM);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_SPEAKER:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_SPEAKER);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_LINE:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_LINE);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_MIC:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_MIC);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_CD:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_CD);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_IMIX:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_IMIX);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_ALTPCM:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_ALTPCM);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_RECLEV:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_RECLEV);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_IGAIN:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_IGAIN);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_OGAIN:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_OGAIN);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_LINE1:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_LINE1);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_LINE2:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_LINE2);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_LINE3:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_LINE3);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_OSS_GETVERSION: {
		int version = linux_get_oss_version(td->td_proc);
		return (copyout(&version, (caddr_t)args->arg, sizeof(int)));
	}

	case LINUX_SOUND_MIXER_READ_STEREODEVS:
		args->cmd = SOUND_MIXER_READ_STEREODEVS;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_READ_DEVMASK:
		args->cmd = SOUND_MIXER_READ_DEVMASK;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_MIXER_WRITE_RECSRC:
		args->cmd = SETDIR(SOUND_MIXER_WRITE_RECSRC);
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_RESET:
		args->cmd = SNDCTL_DSP_RESET;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SYNC:
		args->cmd = SNDCTL_DSP_SYNC;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SPEED:
		args->cmd = SNDCTL_DSP_SPEED;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_STEREO:
		args->cmd = SNDCTL_DSP_STEREO;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETBLKSIZE: /* LINUX_SNDCTL_DSP_SETBLKSIZE */
		args->cmd = SNDCTL_DSP_GETBLKSIZE;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SETFMT:
		args->cmd = SNDCTL_DSP_SETFMT;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_PCM_WRITE_CHANNELS:
		args->cmd = SOUND_PCM_WRITE_CHANNELS;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SOUND_PCM_WRITE_FILTER:
		args->cmd = SOUND_PCM_WRITE_FILTER;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_POST:
		args->cmd = SNDCTL_DSP_POST;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SUBDIVIDE:
		args->cmd = SNDCTL_DSP_SUBDIVIDE;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SETFRAGMENT:
		args->cmd = SNDCTL_DSP_SETFRAGMENT;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETFMTS:
		args->cmd = SNDCTL_DSP_GETFMTS;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETOSPACE:
		args->cmd = SNDCTL_DSP_GETOSPACE;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETISPACE:
		args->cmd = SNDCTL_DSP_GETISPACE;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_NONBLOCK:
		args->cmd = SNDCTL_DSP_NONBLOCK;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETCAPS:
		args->cmd = SNDCTL_DSP_GETCAPS;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_SETTRIGGER: /* LINUX_SNDCTL_GETTRIGGER */
		args->cmd = SNDCTL_DSP_SETTRIGGER;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETIPTR:
		args->cmd = SNDCTL_DSP_GETIPTR;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETOPTR:
		args->cmd = SNDCTL_DSP_GETOPTR;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_DSP_GETODELAY:
		args->cmd = SNDCTL_DSP_GETODELAY;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_RESET:
		args->cmd = SNDCTL_SEQ_RESET;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_SYNC:
		args->cmd = SNDCTL_SEQ_SYNC;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SYNTH_INFO:
		args->cmd = SNDCTL_SYNTH_INFO;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_CTRLRATE:
		args->cmd = SNDCTL_SEQ_CTRLRATE;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_GETOUTCOUNT:
		args->cmd = SNDCTL_SEQ_GETOUTCOUNT;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_GETINCOUNT:
		args->cmd = SNDCTL_SEQ_GETINCOUNT;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_PERCMODE:
		args->cmd = SNDCTL_SEQ_PERCMODE;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_FM_LOAD_INSTR:
		args->cmd = SNDCTL_FM_LOAD_INSTR;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_TESTMIDI:
		args->cmd = SNDCTL_SEQ_TESTMIDI;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_RESETSAMPLES:
		args->cmd = SNDCTL_SEQ_RESETSAMPLES;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_NRSYNTHS:
		args->cmd = SNDCTL_SEQ_NRSYNTHS;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_NRMIDIS:
		args->cmd = SNDCTL_SEQ_NRMIDIS;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_MIDI_INFO:
		args->cmd = SNDCTL_MIDI_INFO;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SEQ_TRESHOLD:
		args->cmd = SNDCTL_SEQ_TRESHOLD;
		return (ioctl(td, (struct ioctl_args *)args));

	case LINUX_SNDCTL_SYNTH_MEMAVL:
		args->cmd = SNDCTL_SYNTH_MEMAVL;
		return (ioctl(td, (struct ioctl_args *)args));

	}

	return (ENOIOCTL);
}

/*
 * Console related ioctls
 */

#define ISSIGVALID(sig)		((sig) > 0 && (sig) < NSIG)

static int
linux_ioctl_console(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	int error;

	if ((error = fget(td, args->fd, &fp)) != 0)
		return (error);
	switch (args->cmd & 0xffff) {

	case LINUX_KIOCSOUND:
		args->cmd = KIOCSOUND;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_KDMKTONE:
		args->cmd = KDMKTONE;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_KDGETLED:
		args->cmd = KDGETLED;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_KDSETLED:
		args->cmd = KDSETLED;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_KDSETMODE:
		args->cmd = KDSETMODE;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_KDGETMODE:
		args->cmd = KDGETMODE;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_KDGKBMODE:
		args->cmd = KDGKBMODE;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_KDSKBMODE: {
		int kbdmode;
		switch (args->arg) {
		case LINUX_KBD_RAW:
			kbdmode = K_RAW;
			break;
		case LINUX_KBD_XLATE:
			kbdmode = K_XLATE;
			break;
		case LINUX_KBD_MEDIUMRAW:
			kbdmode = K_RAW;
			break;
		default:
			fdrop(fp, td);
			return (EINVAL);
		}
		error = (fo_ioctl(fp, KDSKBMODE, (caddr_t)&kbdmode,
		    td->td_ucred, td));
		break;
	}

	case LINUX_VT_OPENQRY:
		args->cmd = VT_OPENQRY;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_VT_GETMODE:
		args->cmd = VT_GETMODE;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_VT_SETMODE: {
		struct vt_mode *mode;
		args->cmd = VT_SETMODE;
		mode = (struct vt_mode *)args->arg;
		if (!ISSIGVALID(mode->frsig) && ISSIGVALID(mode->acqsig))
			mode->frsig = mode->acqsig;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;
	}

	case LINUX_VT_GETSTATE:
		args->cmd = VT_GETACTIVE;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_VT_RELDISP:
		args->cmd = VT_RELDISP;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_VT_ACTIVATE:
		args->cmd = VT_ACTIVATE;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	case LINUX_VT_WAITACTIVE:
		args->cmd = VT_WAITACTIVE;
		error = (ioctl(td, (struct ioctl_args *)args));
		break;

	default:
		error = ENOIOCTL;
		break;
	}
	
	fdrop(fp, td);
	return (error);
}

/*
 * Criteria for interface name translation
 */
#define IFP_IS_ETH(ifp) (ifp->if_type == IFT_ETHER)

/*
 * Interface function used by linprocfs (at the time of writing). It's not
 * used by the Linuxulator itself.
 */
int
linux_ifname(struct ifnet *ifp, char *buffer, size_t buflen)
{
	struct ifnet *ifscan;
	int ethno;

	/* Short-circuit non ethernet interfaces */
	if (!IFP_IS_ETH(ifp))
		return (snprintf(buffer, buflen, "%s%d", ifp->if_name,
		    ifp->if_unit));

	/* Determine the (relative) unit number for ethernet interfaces */
	ethno = 0;
	TAILQ_FOREACH(ifscan, &ifnet, if_link) {
		if (ifscan == ifp)
			return (snprintf(buffer, buflen, "eth%d", ethno));
		if (IFP_IS_ETH(ifscan))
			ethno++;
	}

	return (0);
}

/*
 * Translate a Linux interface name to a FreeBSD interface name,
 * and return the associated ifnet structure
 * bsdname and lxname need to be least IFNAMSIZ bytes long, but
 * can point to the same buffer.
 */

static struct ifnet *
ifname_linux_to_bsd(const char *lxname, char *bsdname)
{
	struct ifnet *ifp;
	int len, unit;
	char *ep;
	int is_eth, index;

	for (len = 0; len < LINUX_IFNAMSIZ; ++len)
		if (!isalpha(lxname[len]))
			break;
	if (len == 0 || len == LINUX_IFNAMSIZ)
		return (NULL);
	unit = (int)strtoul(lxname + len, &ep, 10);
	if (ep == NULL || ep == lxname + len || ep >= lxname + LINUX_IFNAMSIZ)
		return (NULL);
	index = 0;
	is_eth = (len == 3 && !strncmp(lxname, "eth", len)) ? 1 : 0;
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		/*
		 * Allow Linux programs to use FreeBSD names. Don't presume
		 * we never have an interface named "eth", so don't make
		 * the test optional based on is_eth.
		 */
		if (ifp->if_unit == unit && ifp->if_name[len] == '\0' &&
		    strncmp(ifp->if_name, lxname, len) == 0)
			break;
		if (is_eth && IFP_IS_ETH(ifp) && unit == index++)
			break;
	}
	if (ifp != NULL)
		snprintf(bsdname, IFNAMSIZ, "%s%d", ifp->if_name, ifp->if_unit);
	return (ifp);
}

/*
 * Implement the SIOCGIFCONF ioctl
 */

static int
linux_ifconf(struct thread *td, struct ifconf *uifc)
{
	struct ifconf ifc;
	struct l_ifreq ifr;
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct iovec iov;
	struct uio uio;
	int error, ethno;

	error = copyin(uifc, &ifc, sizeof ifc);
	if (error != 0)
		return (error);

	/* much easier to use uiomove than keep track ourselves */
	iov.iov_base = ifc.ifc_buf;
	iov.iov_len = ifc.ifc_len;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_resid = ifc.ifc_len;
	uio.uio_segflg = UIO_USERSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_td = td;

	/* Keep track of eth interfaces */
	ethno = 0;

	/* Return all AF_INET addresses of all interfaces */
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		if (uio.uio_resid <= 0)
			break;

		bzero(&ifr, sizeof ifr);
		if (IFP_IS_ETH(ifp))
			snprintf(ifr.ifr_name, LINUX_IFNAMSIZ, "eth%d",
			    ethno++);
		else
			snprintf(ifr.ifr_name, LINUX_IFNAMSIZ, "%s%d",
			    ifp->if_name, ifp->if_unit);

		/* Walk the address list */
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			struct sockaddr *sa = ifa->ifa_addr;

			if (uio.uio_resid <= 0)
				break;

			if (sa->sa_family == AF_INET) {
				ifr.ifr_addr.sa_family = LINUX_AF_INET;
				memcpy(ifr.ifr_addr.sa_data, sa->sa_data,
				    sizeof(ifr.ifr_addr.sa_data));

				error = uiomove((caddr_t)&ifr, sizeof ifr,
				    &uio);
				if (error != 0)
					return (error);
			}
		}
	}

	ifc.ifc_len -= uio.uio_resid;
	error = copyout(&ifc, uifc, sizeof ifc);

	return (error);
}

static int
linux_gifflags(struct thread *td, struct ifnet *ifp, struct l_ifreq *ifr)
{
	l_short flags;

	flags = ifp->if_flags & 0xffff;
	/* these flags have no Linux equivalent */
	flags &= ~(IFF_SMART|IFF_OACTIVE|IFF_SIMPLEX|
	    IFF_LINK0|IFF_LINK1|IFF_LINK2);
	/* Linux' multicast flag is in a different bit */
	if (flags & IFF_MULTICAST) {
		flags &= ~IFF_MULTICAST;
		flags |= 0x1000;
	}

	return (copyout(&flags, &ifr->ifr_flags, sizeof flags));
}

#define ARPHRD_ETHER	1
#define ARPHRD_LOOPBACK	772

static int
linux_gifhwaddr(struct ifnet *ifp, struct l_ifreq *ifr)
{
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
	struct l_sockaddr lsa;

	if (ifp->if_type == IFT_LOOP) {
		bzero(&lsa, sizeof lsa);
		lsa.sa_family = ARPHRD_LOOPBACK;
		return (copyout(&lsa, &ifr->ifr_hwaddr, sizeof lsa));
	}
	
	if (ifp->if_type != IFT_ETHER)
		return (ENOENT);

	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		sdl = (struct sockaddr_dl*)ifa->ifa_addr;
		if (sdl != NULL && (sdl->sdl_family == AF_LINK) &&
		    (sdl->sdl_type == IFT_ETHER)) {
			bzero(&lsa, sizeof lsa);
			lsa.sa_family = ARPHRD_ETHER;
			bcopy(LLADDR(sdl), lsa.sa_data, LINUX_IFHWADDRLEN);
			return (copyout(&lsa, &ifr->ifr_hwaddr, sizeof lsa));
		}
	}
	
	return (ENOENT);
}

/*
 * Socket related ioctls
 */

static int
linux_ioctl_socket(struct thread *td, struct linux_ioctl_args *args)
{
	char lifname[LINUX_IFNAMSIZ], ifname[IFNAMSIZ];
	struct ifnet *ifp;
	struct file *fp;
	int error, type;

	KASSERT(LINUX_IFNAMSIZ == IFNAMSIZ,
	    ("%s(): LINUX_IFNAMSIZ != IFNAMSIZ", __func__));
	
	ifp = NULL;
	error = 0;
	
	if ((error = fget(td, args->fd, &fp)) != 0)
		return (error);
	type = fp->f_type;
	fdrop(fp, td);
	if (type != DTYPE_SOCKET) {
		/* not a socket - probably a tap / vmnet device */
		switch (args->cmd) {
		case LINUX_SIOCGIFADDR:
		case LINUX_SIOCSIFADDR:
		case LINUX_SIOCGIFFLAGS:
			return (linux_ioctl_special(td, args));
		default:
			return (ENOIOCTL);
		}
	}

	switch (args->cmd & 0xffff) {
		
	case LINUX_FIOGETOWN:
	case LINUX_FIOSETOWN:
	case LINUX_SIOCADDMULTI:
	case LINUX_SIOCATMARK:
	case LINUX_SIOCDELMULTI:
	case LINUX_SIOCGIFCONF:
	case LINUX_SIOCGPGRP:
	case LINUX_SIOCSPGRP:
		/* these ioctls don't take an interface name */
#ifdef DEBUG
		printf("%s(): ioctl %d\n", __func__,
		    args->cmd & 0xffff);
#endif
		break;
		
	case LINUX_SIOCGIFFLAGS:
	case LINUX_SIOCGIFADDR:
	case LINUX_SIOCSIFADDR:
	case LINUX_SIOCGIFDSTADDR:
	case LINUX_SIOCGIFBRDADDR:
	case LINUX_SIOCGIFNETMASK:
	case LINUX_SIOCSIFNETMASK:
	case LINUX_SIOCGIFMTU:
	case LINUX_SIOCSIFMTU:
	case LINUX_SIOCSIFNAME:
	case LINUX_SIOCGIFHWADDR:
	case LINUX_SIOCSIFHWADDR:
	case LINUX_SIOCDEVPRIVATE:
	case LINUX_SIOCDEVPRIVATE+1:
		/* copy in the interface name and translate it. */
		error = copyin((char *)args->arg, lifname, LINUX_IFNAMSIZ);
		if (error != 0)
			return (error);
#ifdef DEBUG
		printf("%s(): ioctl %d on %.*s\n", __func__,
		    args->cmd & 0xffff, LINUX_IFNAMSIZ, lifname);
#endif
		ifp = ifname_linux_to_bsd(lifname, ifname);
		if (ifp == NULL)
			return (EINVAL);
		/*
		 * We need to copy it back out in case we pass the
		 * request on to our native ioctl(), which will expect
		 * the ifreq to be in user space and have the correct
		 * interface name.
		 */
		error = copyout(ifname, (char *)args->arg, IFNAMSIZ);
		if (error != 0)
			return (error);
#ifdef DEBUG
		printf("%s(): %s translated to %s\n", __func__,
		    lifname, ifname);
#endif
		break;
		
	default:
		return (ENOIOCTL);
	}

	switch (args->cmd & 0xffff) {

	case LINUX_FIOSETOWN:
		args->cmd = FIOSETOWN;
		error = ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_SIOCSPGRP:
		args->cmd = SIOCSPGRP;
		error = ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_FIOGETOWN:
		args->cmd = FIOGETOWN;
		error = ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_SIOCGPGRP:
		args->cmd = SIOCGPGRP;
		error = ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_SIOCATMARK:
		args->cmd = SIOCATMARK;
		error = ioctl(td, (struct ioctl_args *)args);
		break;

	/* LINUX_SIOCGSTAMP */

	case LINUX_SIOCGIFCONF:
		error = linux_ifconf(td, (struct ifconf *)args->arg);
		break;

	case LINUX_SIOCGIFFLAGS:
		args->cmd = SIOCGIFFLAGS;
		error = linux_gifflags(td, ifp, (struct l_ifreq *)args->arg);
		break;

	case LINUX_SIOCGIFADDR:
		args->cmd = OSIOCGIFADDR;
		error = ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_SIOCSIFADDR:
		/* XXX probably doesn't work, included for completeness */
		args->cmd = SIOCSIFADDR;
		error = ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_SIOCGIFDSTADDR:
		args->cmd = OSIOCGIFDSTADDR;
		error = ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_SIOCGIFBRDADDR:
		args->cmd = OSIOCGIFBRDADDR;
		error = ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_SIOCGIFNETMASK:
		args->cmd = OSIOCGIFNETMASK;
		error = ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_SIOCSIFNETMASK:
		error = ENOIOCTL;
		break;
		
	case LINUX_SIOCGIFMTU:
		args->cmd = SIOCGIFMTU;
		error = ioctl(td, (struct ioctl_args *)args);
		break;
		
	case LINUX_SIOCSIFMTU:
		args->cmd = SIOCSIFMTU;
		error = ioctl(td, (struct ioctl_args *)args);
		break;
		
	case LINUX_SIOCSIFNAME:
		error = ENOIOCTL;
		break;
		
	case LINUX_SIOCGIFHWADDR:
		error = linux_gifhwaddr(ifp, (struct l_ifreq *)args->arg);
		break;

	case LINUX_SIOCSIFHWADDR:
		error = ENOIOCTL;
		break;
		
	case LINUX_SIOCADDMULTI:
		args->cmd = SIOCADDMULTI;
		error = ioctl(td, (struct ioctl_args *)args);
		break;

	case LINUX_SIOCDELMULTI:
		args->cmd = SIOCDELMULTI;
		error = ioctl(td, (struct ioctl_args *)args);
		break;

	/*
	 * XXX This is slightly bogus, but these ioctls are currently
	 * XXX only used by the aironet (if_an) network driver.
	 */
	case LINUX_SIOCDEVPRIVATE:
		args->cmd = SIOCGPRIVATE_0;
		error = ioctl(td, (struct ioctl_args *)args);
		break;
		
	case LINUX_SIOCDEVPRIVATE+1:
		args->cmd = SIOCGPRIVATE_1;
		error = ioctl(td, (struct ioctl_args *)args);
		break;
	}

	if (ifp != NULL)
		/* restore the original interface name */
		copyout(lifname, (char *)args->arg, LINUX_IFNAMSIZ);

#ifdef DEBUG
	printf("%s(): returning %d\n", __func__, error);
#endif
	return (error);
}

/*
 * Device private ioctl handler
 */
static int
linux_ioctl_private(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	int error, type;

	if ((error = fget(td, args->fd, &fp)) != 0)
		return (error);
	type = fp->f_type;
	fdrop(fp, td);
	if (type == DTYPE_SOCKET)
		return (linux_ioctl_socket(td, args));
	return (ENOIOCTL);
}

/*
 * Special ioctl handler
 */
static int
linux_ioctl_special(struct thread *td, struct linux_ioctl_args *args)
{
	int error;

	switch (args->cmd) {
	case LINUX_SIOCGIFADDR:
		args->cmd = SIOCGIFADDR;
		error = ioctl(td, (struct ioctl_args *)args);
		break;
	case LINUX_SIOCSIFADDR:
		args->cmd = SIOCSIFADDR;
		error = ioctl(td, (struct ioctl_args *)args);
		break;
	case LINUX_SIOCGIFFLAGS:
		args->cmd = SIOCGIFFLAGS;
		error = ioctl(td, (struct ioctl_args *)args);
		break;
	default:
		error = ENOIOCTL;
	}

	return (error);
}

/*
 * main ioctl syscall function
 */

int
linux_ioctl(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	struct handler_element *he;
	int error, cmd;

#ifdef DEBUG
	if (ldebug(ioctl))
		printf(ARGS(ioctl, "%d, %04lx, *"), args->fd,
		    (unsigned long)args->cmd);
#endif

	if ((error = fget(td, args->fd, &fp)) != 0)
		return (error);
	if ((fp->f_flag & (FREAD|FWRITE)) == 0) {
		fdrop(fp, td);
		return (EBADF);
	}

	/* Iterate over the ioctl handlers */
	cmd = args->cmd & 0xffff;
	TAILQ_FOREACH(he, &handlers, list) {
		if (cmd >= he->low && cmd <= he->high) {
			error = (*he->func)(td, args);
			if (error != ENOIOCTL) {
				fdrop(fp, td);
				return (error);
			}
		}
	}
	fdrop(fp, td);

	printf("linux: 'ioctl' fd=%d, cmd=0x%x ('%c',%d) not implemented\n",
	    args->fd, (int)(args->cmd & 0xffff),
	    (int)(args->cmd & 0xff00) >> 8, (int)(args->cmd & 0xff));

	return (EINVAL);
}

int
linux_ioctl_register_handler(struct linux_ioctl_handler *h)
{
	struct handler_element *he, *cur;

	if (h == NULL || h->func == NULL)
		return (EINVAL);

	/*
	 * Reuse the element if the handler is already on the list, otherwise
	 * create a new element.
	 */
	TAILQ_FOREACH(he, &handlers, list) {
		if (he->func == h->func)
			break;
	}
	if (he == NULL) {
		MALLOC(he, struct handler_element *, sizeof(*he),
		    M_LINUX, M_WAITOK);
		he->func = h->func;
	} else
		TAILQ_REMOVE(&handlers, he, list);
	
	/* Initialize range information. */
	he->low = h->low;
	he->high = h->high;
	he->span = h->high - h->low + 1;

	/* Add the element to the list, sorted on span. */
	TAILQ_FOREACH(cur, &handlers, list) {
		if (cur->span > he->span) {
			TAILQ_INSERT_BEFORE(cur, he, list);
			return (0);
		}
	}
	TAILQ_INSERT_TAIL(&handlers, he, list);

	return (0);
}

int
linux_ioctl_unregister_handler(struct linux_ioctl_handler *h)
{
	struct handler_element *he;

	if (h == NULL || h->func == NULL)
		return (EINVAL);

	TAILQ_FOREACH(he, &handlers, list) {
		if (he->func == h->func) {
			TAILQ_REMOVE(&handlers, he, list);
			FREE(he, M_LINUX);
			return (0);
		}
	}

	return (EINVAL);
}
